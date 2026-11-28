#include "BossHealth.h"

#include <algorithm>
#include <cstdio>
#include <string>

#include "BossHealthBars.h"
#include "BossIntro.h"
#include "BossKasumiStep.h"
#include "BossMath.h"
#include "BossMoveExecutor.h"
#include "BossPhaseShift.h"
#include "BossPose.h"
#include "BossRules.h"
#include "BossSelection.h"
#include "../Combat/Damageable.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

HIKARI::Vec3 ToGame(Vec3 value) { return {value.x, value.y, value.z}; }

// 気絶：技を打ち切って沈み込み、動けなくなる（飛んでいる斬撃はそのまま）。気絶値がたまった時と、大技が崩れた時。
void EnterStun(Boss& boss) {
    ++boss.stunCount;
    boss.stun = 0.0f;
    BossMoveExecutor::Interrupt(boss);
    BossRules::CancelRequest(boss);
    boss.state = BossState::Stunned;
    boss.stateFrame = 0;
    boss.breakFeedbackPending = true;
    char message[160]{};
    std::snprintf(message, sizeof(message), "Stunned tick=%llu count=%d frames=%d",
                  static_cast<unsigned long long>(boss.tick), boss.stunCount, boss.stunFrames);
    BossLog::Write(message);
}

void AddStun(Boss& boss, float amount) {
    if (amount <= 0.0f || boss.state == BossState::Stunned || boss.state == BossState::Dead ||
        (boss.stunLimit > 0 && boss.stunCount >= boss.stunLimit))
        return;
    boss.stun += amount;
    boss.stunHitSeconds = boss.clockSeconds;
    const float threshold = BossRules::GetStunThreshold(boss);
    char message[160]{};
    std::snprintf(message, sizeof(message), "StunGain tick=%llu amount=%.3f stun=%.3f max=%.3f",
                  static_cast<unsigned long long>(boss.tick), static_cast<double>(amount),
                  static_cast<double>(boss.stun), static_cast<double>(threshold));
    BossLog::Write(message);
    if (boss.stun >= threshold) {
        // 技・連段の途中は打ち切らない：満ちた事を覚えて、連段が終わった時にひざまずく（c-808641）。
        if (boss.state == BossState::Act) {
            boss.stun = threshold;
            if (!boss.stunPending)
                BossLog::Write(("StunPending tick=" + std::to_string(boss.tick)).c_str());
            boss.stunPending = true;
        } else {
            EnterStun(boss);
        }
    }
}

// 崩せる大技：前隙（溜めを含む）の間に表の「崩せる被弾数」だけ当てられたら、技を打ち切って気絶させる。
// 当たりは Damageable が受け付けた命中（無敵で弾かれたものは数えない）。崩れた気絶も気絶の回数に数える。
void CheckBreak(Boss& boss, uint64_t freshHits) {
    if (!boss.breakDuringWindup || freshHits == 0 || boss.state != BossState::Act || boss.phase != BossActPhase::Pre)
        return;
    const BossMove* move = BossRules::GetMove(boss, boss.move);
    if (move == nullptr || move->breakHits <= 0)
        return;
    boss.windupHits += static_cast<int>(std::min<uint64_t>(freshHits, kBossHitMemory));
    if (boss.windupHits < move->breakHits)
        return;
    char message[192]{};
    std::snprintf(message, sizeof(message), "MoveBroken tick=%llu move=%s hits=%d need=%d stateFrame=%d",
                  static_cast<unsigned long long>(boss.tick), move->name.c_str(), boss.windupHits, move->breakHits,
                  boss.stateFrame);
    BossLog::Write(message);
    EnterStun(boss);
}

} // namespace

uint64_t BossHealth::Sync(BossFrame& frame, HIKARI::ObjectView object, Damageable& health) {
    Boss& boss = frame.boss;
    BossHealthBars::Prepare(boss, frame.shift, health);
    if (boss.testDamageAtFrame >= 0 && boss.testDamageDone < boss.testDamageRepeatCount &&
        static_cast<int64_t>(boss.tick) >=
            static_cast<int64_t>(boss.testDamageAtFrame) +
                static_cast<int64_t>(boss.testDamageDone) * std::max(0, boss.testDamageRepeatFrames)) {
        if (boss.testDamageDone == 0 || boss.testDamageRepeatFrames > 0) {
            const bool accepted = health.Apply(boss.testDamageAmount, ToGame(Forward(boss.yaw)));
            char message[192]{};
            std::snprintf(message, sizeof(message), "TestDamage tick=%llu amount=%.3f accepted=%d health=%.3f",
                          static_cast<unsigned long long>(boss.tick), static_cast<double>(boss.testDamageAmount),
                          static_cast<int>(accepted), static_cast<double>(health.Health()));
            BossLog::Write(message);
        }
        ++boss.testDamageDone;
    }
    // 当てられた回数は技の選び方が覚える（連続攻撃への割り込み、突進系の出やすさ）。前隙の間なら大技が崩れる。
    const uint64_t hits = BossSelection::NoteHits(boss, health.hitCount);
    // 累計は条を満たしても戻さない（差分だけを読むので、2 本目を満たした tick も気絶値の基準はずれない）。
    const double base = health.baseDamageTotal;
    const double delta = base - boss.seenBaseDamage;
    boss.seenBaseDamage = base;
    // 1 本目が空になった tick は、崩れも気絶値も数えずに変わり目へ入る（気絶値は 0 に戻る）。変わり目の演出（c-8dd296）を
    // 始める（打ち切る前の体の高さを記録に書く）。
    const float rootBefore = BossPose::RootHeight(boss);
    if (BossHealthBars::CheckEmptied(boss, frame.shift, health, delta)) {
        BossPhaseShiftRules::Begin(boss, frame.shift, frame.playerCenter, frame.player != nullptr, rootBefore);
    } else {
        CheckBreak(boss, hits);
        // 戻り（後隙）の間に当てた弾は、気絶値が多く入る（c-808641）。
        const bool recovering = boss.state == BossState::Act && boss.phase == BossActPhase::Post;
        if (delta > 0.0)
            AddStun(boss, static_cast<float>(delta) / std::max(0.01f, boss.stunReferenceDamage) * boss.stunPerShot *
                              (recovering ? boss.recoveryStunScale : 1.0f));
    }
    // しばらく当てないと気絶値が減る。
    if (boss.state != BossState::Stunned && boss.stun > 0.0f &&
        boss.clockSeconds - boss.stunHitSeconds > boss.stunDecayDelaySeconds)
        boss.stun = std::max(0.0f, boss.stun - boss.stunDecayPerSecond * frame.dt);
    if (!health.IsAlive() && boss.state != BossState::Dead) {
        BossMoveExecutor::Interrupt(boss);
        BossRules::CancelRequest(boss);
        boss.state = BossState::Dead;
        boss.stateFrame = 0;
        char message[128]{};
        std::snprintf(message, sizeof(message), "Defeated tick=%llu object=%llu",
                      static_cast<unsigned long long>(boss.tick),
                      static_cast<unsigned long long>(object.GetSceneObjectId()));
        BossLog::Write(message);
    }
    // 気絶（ひざまずき）の間と、戻り（後隙）の間は、受ける威力が増える。戻りの当たりは手応えも強める（次の当たりに効く）。
    const bool inRecovery = boss.state == BossState::Act && boss.phase == BossActPhase::Post;
    health.incomingScale = (boss.state == BossState::Stunned ? boss.stunDamageScale
                                                    : inRecovery                     ? boss.recoveryDamageScale
                                                                                     : 1.0f);
    if (inRecovery) {
        health.Boost(boss.recoveryHitPushScale, boss.recoveryHitStopFrames);
        boss.recoveryBoosted = true;
    } else if (boss.recoveryBoosted) {
        health.Boost(1.0f, 0);
        boss.recoveryBoosted = false;
    }
    // 弾をかわす霞歩の跳び始め（kasumiStepGuardFrames）は当たらない。登場の間（構えに収まるまで）と、フェーズの
    // 変わり目の間も当たらない。
    const bool shielded =
        BossKasumiStep::Guarding(boss) || BossIntroRules::Shielded(frame.intro) || BossRules::IsPhaseShifting(boss);
    if (shielded != boss.shieldApplied) {
        health.invulnerable = shielded;
        boss.shieldApplied = shielded;
    }
    // HUD へ出す値は、この tick の被弾と条の入れ替えを済ませた後に写す（1 本目が空の 0 を出さない。出すと Flow が
    // 撃破と読む）。
    BossHealthBars::Publish(boss, health);
    return hits;
}

bool BossHealth::ResolvePoise(BossFrame& frame) {
    Boss& boss = frame.boss;
    if (boss.state == BossState::Stunned || boss.state == BossState::Dead)
        return false;
    const float threshold = BossRules::GetStunThreshold(boss);
    if (boss.stunPending || boss.stun >= threshold) {
        boss.stunPending = false;
        EnterStun(boss);
        return true;
    }
    if (!(boss.stun >= threshold * boss.poiseFlinchShare) || boss.clockSeconds < boss.flinchReadySeconds)
        return false;
    const int row = BossRules::FindMove(boss, boss.poiseFlinchMove);
    const BossMove* move = BossRules::GetMove(boss, row);
    if (move == nullptr || !move->executable)
        return false;
    boss.flinchReadySeconds = boss.clockSeconds + boss.poiseFlinchCooldownSeconds;
    ++boss.flinchCount;
    char message[160]{};
    std::snprintf(message, sizeof(message), "Flinch tick=%llu stun=%.3f max=%.3f move=%s",
                  static_cast<unsigned long long>(boss.tick), static_cast<double>(boss.stun),
                  static_cast<double>(threshold), move->name.c_str());
    BossLog::Write(message);
    BossRules::CancelRequest(boss);
    BossMoveExecutor::Start(frame, row, 0);
    return true;
}

} // namespace Project_TD2_2
