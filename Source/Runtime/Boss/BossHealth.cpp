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
#include "../Combat/DamageableRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 気絶：技を打ち切って沈み込み、動けなくなる（飛んでいる斬撃はそのまま）。気絶値がたまった時と、大技が崩れた時。
void EnterStun(BossComponent& boss) {
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

void AddStun(BossComponent& boss, float amount) {
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
void CheckBreak(BossComponent& boss, uint64_t freshHits) {
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

uint64_t BossHealth::Sync(BossFrame& frame, HIKARI::ObjectView object, DamageableComponent& health) {
    BossComponent& boss = frame.boss;
    BossHealthBars::Prepare(boss, frame.shift, health);
    DamageableRules::Initialize(health);
    if (boss.testDamageAtFrame >= 0 && boss.testDamageDone < boss.testDamageRepeatCount &&
        static_cast<int64_t>(boss.tick) >=
            static_cast<int64_t>(boss.testDamageAtFrame) +
                static_cast<int64_t>(boss.testDamageDone) * std::max(0, boss.testDamageRepeatFrames)) {
        if (boss.testDamageDone == 0 || boss.testDamageRepeatFrames > 0) {
            const bool accepted = DamageableRules::ApplyDamage(health, boss.testDamageAmount, Forward(boss.yaw));
            char message[192]{};
            std::snprintf(message, sizeof(message), "TestDamage tick=%llu amount=%.3f accepted=%d health=%.3f",
                          static_cast<unsigned long long>(boss.tick), static_cast<double>(boss.testDamageAmount),
                          static_cast<int>(accepted), static_cast<double>(DamageableRules::GetHealth(health)));
            BossLog::Write(message);
        }
        ++boss.testDamageDone;
    }
    // 当てられた回数は技の選び方が覚える（連続攻撃への割り込み、突進系の出やすさ）。前隙の間なら大技が崩れる。
    const uint64_t hits = BossSelection::NoteHits(boss, DamageableRules::GetHitCount(health));
    // 累計は条を満たしても戻さない（差分だけを読むので、2 本目を満たした tick も気絶値の基準はずれない）。
    const double base = DamageableRules::GetBaseDamageTotal(health);
    const double delta = base - boss.seenBaseDamage;
    boss.seenBaseDamage = base;
    // 1 本目が空になった tick は、崩れも気絶値も数えずに変わり目へ入る（気絶値は 0 に戻る）。変わり目の演出（c-8dd296）を
    // 始める（打ち切る前の体の高さを記録に書く）。
    const float rootBefore = BossPose::RootHeight(boss);
    if (BossHealthBars::CheckEmptied(boss, frame.shift, health, delta)) {
        BossPhaseShift::Begin(boss, frame.shift, frame.playerCenter, frame.player != nullptr, rootBefore);
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
    if (!DamageableRules::IsAlive(health) && boss.state != BossState::Dead) {
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
    DamageableRules::SetIncomingDamageScale(health, boss.state == BossState::Stunned ? boss.stunDamageScale
                                                    : inRecovery                     ? boss.recoveryDamageScale
                                                                                     : 1.0f);
    if (inRecovery) {
        DamageableRules::BoostHitFeedback(health, boss.recoveryHitPushScale, boss.recoveryHitStopFrames);
        boss.recoveryBoosted = true;
    } else if (boss.recoveryBoosted) {
        DamageableRules::BoostHitFeedback(health, 1.0f, 0);
        boss.recoveryBoosted = false;
    }
    // 弾をかわす霞歩の跳び始め（kasumiStepGuardFrames）は当たらない。登場の間（構えに収まるまで）と、フェーズの
    // 変わり目の間も当たらない。
    const bool shielded =
        BossKasumiStep::Guarding(boss) || BossIntro::Shielded(frame.intro) || BossRules::IsPhaseShifting(boss);
    if (shielded != boss.shieldApplied) {
        DamageableRules::SetInvulnerable(health, shielded);
        boss.shieldApplied = shielded;
    }
    // HUD へ出す値は、この tick の被弾と条の入れ替えを済ませた後に写す（1 本目が空の 0 を出さない。出すと Flow が
    // 撃破と読む）。
    BossHealthBars::Publish(boss, health);
    return hits;
}

bool BossHealth::ResolvePoise(BossFrame& frame) {
    BossComponent& boss = frame.boss;
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

void BossHealth::Describe(HIKARI::PropertyFields& fields, BossComponent& boss) {
    if (fields.Section("フェーズと気絶", true)) {
        fields.Float("phaseOneHealth", "HP の条 1 本目（フェーズ 1。0 で、Damageable の最大 HP を下の割合で分ける）",
                     boss.phaseOneHealth, 0.0f, 100000.0f);
        fields.Float("phaseTwoHealth",
                     "HP の条 2 本目（フェーズ 2。1 本目が空になると満ちる。0 で、Damageable の最大 HP の残り）",
                     boss.phaseTwoHealth, 0.0f, 100000.0f);
        fields.Float("phaseOneShare", "条の値が 0 の時、ボス全体の HP のうち 1 本目にする割合", boss.phaseOneShare,
                     0.05f, 0.95f);
        fields.Int("phaseShiftFrames", "変わり目の長さ (F。変わり目の演出を切った時だけ。演出をする時は拍の長さの合計)",
                   boss.phaseShiftFrames, 1, 3600);
        fields.Note(
            "HP はフェーズごとの条です。1 本目を空にすると、いまの技を打ち切って変わり目に入り（気絶値は 0 に"
            "戻る）、同じ条が 2 本目として満ちて戻ります（余ったダメージは持ち越さない）。2 本目を空にすると撃破。"
            "同じ物体の「被弾と HP」の最大 HP は、この 2 つの値で上書きします。火を残す技・火を使う技は"
            "フェーズ 2 から（技の表の「残す火」の @フェーズ2 と「発生条件」）。");
        fields.Float("stunMax", "気絶値（これだけたまると気絶）", boss.stunMax, 1.0f, 100000.0f);
        fields.Float("stunMaxGrowth", "気絶するたびに気絶値を増やす割合（0.25 で 1 回ごとに +25%、0 で増えない）",
                     boss.stunMaxGrowth, 0.0f, 10.0f, 0.01f);
        fields.Int("stunLimit", "気絶する回数の上限（0＝上限なし）", boss.stunLimit, 0, 99);
        fields.Int("stunFrames", "気絶している長さ (F)", boss.stunFrames, 1, 3600);
        fields.Int("breakHitStopFrames", "崩し・気絶の止め (F、0 は無効)", boss.breakHitStopFrames, 0, 30);
        fields.Float("breakHitStopScale", "崩し・気絶の止め中の時間倍率", boss.breakHitStopScale, 0.0f, 1.0f);
        fields.Float("breakCameraAmplitude", "崩し・気絶のカメラ揺れ幅 (m、0 は無効)", boss.breakCameraAmplitude, 0.0f,
                     1.0f);
        fields.Int("breakCameraFrames", "崩し・気絶のカメラ揺れ時間 (F)", boss.breakCameraFrames, 0, 120);
        fields.Float("breakCameraFrequency", "崩し・気絶のカメラ揺れ周波数 (Hz)", boss.breakCameraFrequency, 0.0f,
                     60.0f);
        fields.Float("breakRumbleLow", "崩し・気絶の振動：重いモーター (0〜1)", boss.breakRumbleLow, 0.0f, 1.0f);
        fields.Float("breakRumbleHigh", "崩し・気絶の振動：軽いモーター (0〜1)", boss.breakRumbleHigh, 0.0f, 1.0f);
        fields.Int("breakRumbleFrames", "崩し・気絶の振動時間 (F、0 は無効)", boss.breakRumbleFrames, 0, 120);
        fields.Int("breakFlashFrames", "崩し・気絶の本体の光り (F、0 は無効)", boss.breakFlashFrames, 0, 120);
        fields.Float("breakBlastScale",
                     "崩し・気絶の瞬間に体から弾ける塊の大きさ（当たった所の爆発に対する倍。0 で無効）",
                     boss.breakBlastScale, 0.0f, 6.0f);
        fields.Note("光り方（色・強さ・材質）は Damageable の被弾の発光と同じです。いまのボスの体（色を読める材質の無い"
                    "カプセル）では光りは見えません。崩した合図は「体から弾ける塊」で見せます。");
        fields.Float("stunDamageScale", "気絶中に受けるダメージ (倍)", boss.stunDamageScale, 0.0f, 10.0f);
        fields.Float("stunDecayDelaySeconds", "気絶値が減り始めるまで (秒)", boss.stunDecayDelaySeconds, 0.0f, 120.0f);
        fields.Float("stunDecayPerSecond", "気絶値が 1 秒に減る量", boss.stunDecayPerSecond, 0.0f, 10000.0f);
        fields.Float("stunPerShot", "気絶値：煙弾 1 発", boss.stunPerShot, 0.0f, 10000.0f);
        fields.Float("stunReferenceDamage", "…この威力の命中を 1 発とみなす（砲撃の基礎威力）",
                     boss.stunReferenceDamage, 0.01f, 100000.0f);
        fields.Note("気絶値は受けた威力に比例してたまります（遠くで弱まった弾は少なく）。気絶中はたまりません。");
    }
}

} // namespace Project_TD2_2
