#include "BossSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "HIKARI/Project/GameProjectSession.h"
#include "HIKARI/Project/ProjectDataView.h"
#include "HIKARI/Runtime/ComponentTypeId.h"
#include "BossCloneComponent.h"
#include "BossClones.h"
#include "BossDetonate.h"
#include "BossFeedback.h"
#include "BossFlameRun.h"
#include "BossGourdRead.h"
#include "BossHealthBars.h"
#include "BossIntro.h"
#include "BossKasumiStep.h"
#include "BossMath.h"
#include "BossMoveExecutor.h"
#include "BossObserver.h"
#include "BossPhaseShift.h"
#include "BossPose.h"
#include "BossRules.h"
#include "BossSelection.h"
#include "BossSkyfall.h"
#include "BossSmoke.h"
#include "BossVisuals.h"
#include "BossWaves.h"
#include "../Common/ComponentLookup.h"
#include "../Common/GameSession.h"
#include "../Common/SessionMemory.h"
#include "../Common/TraceInterval.h"
#include "../BossBody/BossBodyVisuals.h"
#include "../BossBody/BossIntroActor.h"
#include "../BossBody/BossIntroCamera.h"
#include "../BossBody/BossIntroEffects.h"
#include "../BossBody/BossPhaseTwoField.h"
#include "../Combat/DamageableRules.h"
#include "../Gourd/GourdRules.h"
#include "../Gourd/GourdShotComponent.h"
#include "../Gourd/GourdShotRules.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

// 登場の演出の間、プレイヤーの入力を止めておく長さ（毎 tick 入れ直す。tick の順がずれても途切れない）。
constexpr int kCinematicHoldTicks = 2;

using ComponentLookup::ForEachOnObject;

using namespace BossMath;

// 姿勢をなめらかにする割合（1 tick あたり）。攻撃中は速く追う（試作の描画と同じ）。
constexpr float kPoseBlend = 0.32f;
constexpr float kPoseBlendFast = 0.7f;
constexpr float kLeanBlend = 0.1f;
constexpr float kStunCrouch = 0.85f;
constexpr float kStunCrouchBlend = 0.12f;
// 剣の上下（度）がこの値のとき体はまっすぐ。上げると後ろへ、振り下ろすと前へ傾く。
constexpr float kPoseLeanNeutral = 30.0f;
// 両手の型の間の左右の傾き：握りが体の横へ 1 m 出ると、片手の構えで剣を横へ何度向けた時と同じだけ傾けるか。両手の
// 型は刀が後ろや真上を向く（剣の左右の角度で傾けると、上段で体が大きく横へ倒れる）ので、握りの横の位置で傾ける。
// 握りの位置は体と一緒に大きくなるので、1 倍の体（Boss.bodyScale で割った値）の 1 m あたりで見る（c-a30eaf）。
constexpr float kSwayDegreesPerGripMeter = 90.0f;
// これより速い位置の変化（瞬間移動）では、動きの傾きを付けない (m/秒)。突進の滑り（最も速い所で約 35 m/秒）・後ろ跳び
// （約 30 m/秒）の間は傾ける（c-95444e。前の 30 では滑りの真ん中で傾きが切れた）。瞬間移動は 1 tick に数 m 動く。
constexpr float kLeanSpeedLimit = 60.0f;
// 撃破の後に体を縮める下限の倍率（0 にすると変換が潰れるので、見えない大きさで止める）。
constexpr float kVanishMinScale = 0.001f;
// 体のゆれの周期（rad/秒）。
constexpr float kBobRate = 2.1f;
constexpr float kStunWobbleRate = 1.7f;
constexpr float kStunWobble = 0.05f;
// 煙の量の換算が取れないとき（ヒョウタンがいない）の値。ヒョウタンの既定値と同じ。
constexpr float kFallbackSmokePerVolume = 4.0f;
// 体がこの高さ（m）より上にある間は、通った跡の煙を地面に残さない。
constexpr float kTrailMaxHeight = 0.5f;

using GameSession::IsPlaying;

class TextSink final : public HIKARI::IProjectDataTextSink {
public:
    void Receive(const HIKARI::ProjectDataReadResult& result, std::string_view utf8Text) override {
        ok = result.Succeeded();
        text.assign(utf8Text.data(), utf8Text.size());
    }
    bool ok = false;
    std::string text;
};

BossTiming TimingOf(const BossComponent& boss) {
    BossTiming timing;
    timing.swingFrames = boss.swingFrames;
    timing.gapSmallFrames = boss.gapSmallFrames;
    timing.gapMediumFrames = boss.gapMediumFrames;
    timing.gapLargeFrames = boss.gapLargeFrames;
    timing.followFrames = boss.followFrames;
    timing.feintFrames = boss.feintFrames;
    timing.kamaeFrames = boss.kamaeFrames;
    timing.raiseFrames = boss.raiseFrames;
    timing.swingInFrames = boss.swingInFrames;
    timing.skyfallDiveFrames = boss.skyfallDiveFrames;
    timing.damageBase = boss.damageBase;
    timing.damagePerFrame = boss.damagePerFrame;
    return timing;
}

void LogMoves(const BossComponent& boss) {
    const auto& moves = boss.table.GetMoves();
    for (size_t i = 0; i < moves.size(); ++i) {
        const BossMove& move = moves[i];
        std::string segments, strikes;
        for (const auto& segment : move.segments)
            segments += std::to_string(segment.length) + (segment.hit ? "h" : "") + (segment.feint ? "f" : "") +
                        (segment.kind == BossSegmentKind::Hide     ? "H"
                         : segment.kind == BossSegmentKind::Appear ? "A"
                                                                   : "") +
                        "/";
        for (const auto& strike : move.strikes) {
            char text[96]{};
            std::snprintf(text, sizeof(text), "[d%d s%.2f v%.2f h%.0f st%.2f %s r%d]", strike.damage,
                          static_cast<double>(strike.size), static_cast<double>(strike.speed),
                          static_cast<double>(strike.homing), static_cast<double>(strike.step),
                          strike.blow ? "blow" : "flinch", strike.reference);
            strikes += text;
        }
        std::string extras;
        for (const int extra : move.windupExtras)
            extras += (extras.empty() ? "" : "/") + std::to_string(extra);
        // 残す火：形[:一撃]〔@フェーズ〕/幅/秒 を , で並べる（1 つで一撃・フェーズの限りが無ければ以前と同じ line/1.50/25.00）。
        std::string burns;
        for (int k = 0; k < move.burnMarkCount; ++k) {
            const BossBurnMark& mark = move.burnMarks[static_cast<size_t>(k)];
            char values[48]{};
            std::snprintf(values, sizeof(values), "/%.2f/%.2f", static_cast<double>(mark.width),
                          static_cast<double>(mark.seconds));
            burns += std::string(k > 0 ? "," : "") + BossMoveTable::BurnKey(mark.shape) +
                     (mark.strike > 0 ? ":" + std::to_string(mark.strike) : "") +
                     (mark.minPhase > 1 ? "@" + std::to_string(mark.minPhase) : "") + values;
        }
        char message[1024]{};
        std::snprintf(message, sizeof(message),
                      "Move index=%zu name=%s type=%s executable=%d pre=%d act=%d post=%d segments=%s damage=%d "
                      "auto=%d size=%.2f speed=%.2f homing=%.1f step=%.2f burn=%s trackStop=%d windup=%s "
                      "breakHits=%d strikes=%s",
                      i, move.name.c_str(), BossMoveTable::TypeKey(move.type), static_cast<int>(move.executable),
                      move.pre, move.act, move.post, segments.c_str(), move.damage, static_cast<int>(move.damageAuto),
                      static_cast<double>(move.size), static_cast<double>(move.speed), static_cast<double>(move.homing),
                      static_cast<double>(move.step), burns.empty() ? "none" : burns.c_str(), move.trackStopFrames,
                      extras.empty() ? "-" : extras.c_str(), move.breakHits, strikes.c_str());
        BossLog::Write(message);
    }
}

// 表を読む。中身が前と同じなら何もしない（技を出すたびに呼ぶので、Play 中に保存した CSV が次の技から効く）。
void LoadTable(HIKARI::WorldView& world, BossComponent& boss) {
    TextSink types, moves, stance, clips, segments;
    world.GetProjectData().ReadText(boss.typesPath, types);
    world.GetProjectData().ReadText(boss.movesPath, moves);
    world.GetProjectData().ReadText(boss.stancePath, stance);
    if (!boss.clipsPath.empty())
        world.GetProjectData().ReadText(boss.clipsPath, clips);
    if (!boss.segmentsPath.empty())
        world.GetProjectData().ReadText(boss.segmentsPath, segments);
    if (!types.ok || !moves.ok) {
        if (!boss.tableLoaded) {
            char message[256]{};
            std::snprintf(message, sizeof(message), "技の表が読めません（Data/%s、Data/%s）", boss.movesPath.c_str(),
                          boss.typesPath.c_str());
            BossLog::Warn(message);
            boss.tableLoaded = true;
        }
        return;
    }
    if (types.text == boss.loadedTypesText && moves.text == boss.loadedMovesText &&
        (!stance.ok || stance.text == boss.loadedStanceText) && (!clips.ok || clips.text == boss.loadedClipsText) &&
        (!segments.ok || segments.text == boss.loadedSegmentsText))
        return;
    std::vector<std::string> warnings;
    BossMoveTable table;
    table.LoadTypes(types.text, warnings);
    table.LoadMoves(moves.text, TimingOf(boss), warnings);
    // モーションの技の区切り（c-7b1a08）：アニメーションの列がある技の長さを、クリップの長さと区間の表で決め直す。
    table.ApplyClips(clips.ok ? std::string_view(clips.text) : std::string_view{},
                     segments.ok ? std::string_view(segments.text) : std::string_view{}, warnings);
    boss.table = std::move(table);
    boss.loadedTypesText = types.text;
    boss.loadedMovesText = moves.text;
    boss.loadedClipsText = clips.ok ? clips.text : std::string{};
    boss.loadedSegmentsText = segments.ok ? segments.text : std::string{};
    // 姿勢と帯の重みの表（無ければ姿勢の節は歩くだけ）。技の名前を表の行へ結び付ける。
    BossStanceTable stanceTable;
    if (stance.ok) {
        stanceTable.Load(stance.text, warnings);
        stanceTable.Resolve(boss.table, warnings);
        boss.loadedStanceText = stance.text;
    }
    boss.stanceTable = std::move(stanceTable);
    boss.stanceMemory.readyAt.assign(boss.table.GetMoves().size(), 0.0f);
    boss.tableLoaded = true;
    // 行が変わったので、行動ツリーの節と表の突き合わせ（節の無い始動技・表に無い名前）をやり直す。
    boss.brain.scoredRows = 0;
    boss.brain.coverageChecked = false;
    boss.brain.unknownCount = 0;
    // 咎められた回数は行の番号で数えているので、行が変わったら数え直す。
    boss.watch.punished = {};
    for (const auto& warning : warnings)
        BossLog::Warn(warning.c_str());
    char message[160]{};
    std::snprintf(message, sizeof(message), "TableLoaded tick=%llu moves=%zu warnings=%zu",
                  static_cast<unsigned long long>(boss.tick), boss.table.GetMoves().size(), warnings.size());
    BossLog::Write(message);
    LogMoves(boss);
}

void Initialize(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss) {
    HIKARI::GameProjectTransform transform{};
    object.GetWorldTransform(transform);
    boss.ground = {transform.position.x, transform.position.y - boss.bodyHalfHeight, transform.position.z};
    const Vec3 forward = Rotate(transform.rotation, {0.0f, 0.0f, 1.0f});
    boss.yaw = YawOf(forward);
    boss.random = static_cast<uint32_t>(boss.randomSeed) * 2654435761u + 1u;
    boss.floatHeight = boss.floatTarget = std::clamp(0.5f, boss.floatMin, std::max(boss.floatMin, boss.floatMax));
    boss.previousGround = boss.lastBodyPosition = boss.ground;
    boss.state = BossState::Idle;
    boss.initialized = true;
    // 登場は Play の間に 1 度だけ：やり直し（シーンの読み直し）の後は、Play session が覚えている。
    auto* memory = dynamic_cast<ISessionMemory*>(world.GetGameSession());
    BossIntro::Begin(boss, memory != nullptr && memory->GetMemory().bossIntroSeen);
    // 役者（骨格の動画で演じる体）を探す。いなければ登場をしない。登場をしない時は、役者と落ちている部品を隠す。
    if (!BossIntroActor::Find(world, boss))
        BossIntro::Cancel(boss);
    if (!BossIntro::Holding(boss))
        BossIntroActor::Hide(world, boss);
    BossIntroCamera::Load(world, boss);
    BossIntroCamera::LoadShift(world, boss);
    LoadTable(world, boss);
}

// 気絶：技を打ち切って沈み込み、動けなくなる（飛んでいる斬撃はそのまま）。気絶値がたまった時と、大技が崩れた時。
void EnterStun(BossComponent& boss) {
    ++boss.stunCount;
    boss.stun = 0.0f;
    BossMoveExecutor::Interrupt(boss);
    BossRules::CancelRequest(boss);
    // 気絶すると、分身を撃たれた後の突進も取りやめる（試作と同じ）。
    boss.brain.cloneStruck = false;
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
    // 退き溜めは、退いている間（溜める前）の被弾は数えない。
    if (move->type == BossMoveType::Retreat && !boss.retreat.charging)
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

// HP の持ち主は Damageable。受けた量から気絶値をため、フェーズと死亡を決める。この tick に新しく当てられた数を返す。
// HP はフェーズごとの条（BossHealthBars。1 本目が空になった tick に変わり目へ入る）。
uint64_t SyncHealth(BossFrame& frame, HIKARI::ObjectView object, DamageableComponent& health) {
    BossComponent& boss = frame.boss;
    BossHealthBars::Prepare(boss, health);
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
    // 始める（打ち切る前の体の高さから、落ちて膝をつく）。
    const float rootBefore = BossPose::RootHeight(boss);
    if (BossHealthBars::CheckEmptied(boss, health, delta)) {
        BossPhaseShift::Begin(boss, frame.playerCenter, frame.player != nullptr, rootBefore);
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
    // 霞に隠れている間は当たらない（体は地面の下へ沈めるが、沈みきるまでの数 F も無敵にする）。弾をかわす霞歩の
    // 跳び始め（kasumiStepGuardFrames）も当たらない。
    // 登場の間（構えに収まるまで）と、フェーズの変わり目の間も当たらない。
    const bool shielded =
        boss.hidden || BossKasumiStep::Guarding(boss) || BossIntro::Shielded(boss) || BossRules::IsPhaseShifting(boss);
    if (shielded != boss.hiddenApplied) {
        health.invulnerable = shielded;
        boss.hiddenApplied = shielded;
    }
    // HUD へ出す値は、この tick の被弾と条の入れ替えを済ませた後に写す（1 本目が空の 0 を出さない。出すと Flow が
    // 撃破と読む）。
    BossHealthBars::Publish(boss, health);
    return hits;
}

// 連段が終わった時（待機へ戻る時・新しく技を始める時）に、たまった体勢を決める（c-808641）：満ちていればひざまずき、
// 半分ほど（poiseFlinchShare）なら怯んで後ろへ跳ぶ技を始める。どちらかを始めたら true。
bool ResolvePoise(BossFrame& frame) {
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

void StartRequested(BossFrame& frame, int chain) {
    BossComponent& boss = frame.boss;
    LoadTable(frame.world, boss);
    // 連段の続きでない新しい技の前に、たまった体勢を決める（怯み・ひざまずきが先）。
    if (chain == 0 && ResolvePoise(frame)) {
        BossRules::CancelRequest(boss);
        return;
    }
    const int index = boss.requestedMove;
    BossRules::CancelRequest(boss);
    const BossMove* move = BossRules::GetMove(boss, index);
    if (move == nullptr || !move->executable) {
        // Play 中に表を保存して読み直した時、いまの技の行が無くなった・区間の数が変わったなら、古い番号のまま続けず
        // （次の tick に表の外や区間の外を読む）、打ち切って待機へ戻す。
        if (boss.state == BossState::Act) {
            const BossMove* current = BossRules::GetMove(boss, boss.move);
            if (current == nullptr || current->segments.size() != boss.segmentDone.size()) {
                BossMoveExecutor::Interrupt(boss);
                BossMoveExecutor::EnterIdle(boss);
            }
        }
        return;
    }
    BossMoveExecutor::Start(frame, index, chain);
    BossSelection::NoteStarted(boss, index, chain);
}

void StepState(BossFrame& frame, IBossBrain& brain, BossBrainContext& context) {
    BossComponent& boss = frame.boss;
    switch (boss.state) {
    case BossState::Dead:
        ++boss.stateFrame;
        boss.crouch = Lerp(boss.crouch, 1.0f, kStunCrouchBlend);
        return;
    case BossState::Stunned:
        ++boss.stateFrame;
        boss.crouch = Lerp(boss.crouch, kStunCrouch, kStunCrouchBlend);
        boss.velocity = {};
        if (boss.stateFrame >= boss.stunFrames) {
            boss.crouch = 0.0f;
            BossLog::Write(("StunEnd tick=" + std::to_string(boss.tick)).c_str());
            BossMoveExecutor::EnterIdle(boss);
        }
        return;
    case BossState::Idle:
        // 戦いが始まるまで（Flow が止めている間）は、歩かず技も出さない。
        // 登場が済むまでも同じ。
        if (boss.held || BossIntro::Holding(boss)) {
            boss.velocity = {};
            return;
        }
        // フェーズの変わり目（c-3e0ffa）の間も、歩かず技も出さない。気絶の屈みから起き上がり、構えは待機の下ろした
        // 構えへ戻していく（崩れる・燃える見た目は演出のカード c-8dd296 が作る）。
        if (BossRules::IsPhaseShifting(boss)) {
            ++boss.idleFrames;
            boss.crouch = Lerp(boss.crouch, 0.0f, kStunCrouchBlend);
            boss.velocity = {};
            return;
        }
        BossMoveExecutor::StepIdle(frame);
        // 変わり目の演出の後、少しの間は技を選ばない（構えたまま横へ歩く。次の技は行動ツリーが決める）。
        if (boss.requestedMove < 0 && !BossPhaseShift::Calm(boss))
            brain.OnIdle(context);
        if (boss.requestedMove >= 0)
            StartRequested(frame, 0);
        return;
    case BossState::Act: {
        const auto result = BossMoveExecutor::Step(frame);
        if (result == BossMoveExecutor::StepResult::ActFinished) {
            brain.OnActFinished(context);
            // コンボ：攻撃中が終わった時点で続ける。後隙は出さない。
            if (boss.requestedMove >= 0 && boss.requestedAsCombo)
                StartRequested(frame, boss.chain + 1);
            else
                BossMoveExecutor::EnterPost(boss);
        } else if (boss.phase == BossActPhase::Post) {
            // 後隙：brain が技を頼めば切り上げて出す（連続攻撃への割り込み）。頼まれなければ最後まで待って待機へ。
            // 後隙の最初の recoveryGuardFrames は brain に聞かない：大技の後の隙を、撃ち返されて切り上げられる前に咎められる。
            context.recoveryDone = result == BossMoveExecutor::StepResult::Finished;
            if (context.recoveryDone || boss.stateFrame >= boss.recoveryGuardFrames)
                brain.OnRecovery(context);
            if (boss.requestedMove >= 0)
                StartRequested(frame, boss.requestedAsCombo ? boss.chain + 1 : 0);
            else if (context.recoveryDone && !ResolvePoise(frame))
                BossMoveExecutor::EnterIdle(boss);
        } else if (BossMoveExecutor::IsWalking(boss)) {
            // 歩きの技の間も木を進める。「移動中も反応する」反応の枝（射撃をかわす霞歩）だけが、歩きを切り上げて技を出す。
            brain.OnWalking(context);
            if (boss.requestedMove >= 0)
                StartRequested(frame, 0);
        }
        return;
    }
    }
}

// 体の姿勢：動いた向きへの傾き、剣の上下・左右に合わせた構えの傾き、屈み（縦に縮む）、浮遊のゆれ。
void ApplyTransform(HIKARI::ObjectView object, BossComponent& boss, float dt) {
    const Vec3 moved = Sub(boss.ground, boss.previousGround);
    boss.previousGround = boss.ground;
    const Vec3 velocity = Mul(moved, 1.0f / std::max(1e-4f, dt));
    const float speedGate = FlatLength(velocity) < kLeanSpeedLimit ? 1.0f : 0.0f;
    // 両手の斬りの間は、速さの傾きを小さく抑える（踏み込みで大きく傾くと、胴と兜が両手の通り道へ入る）。
    const float leanMax = Lerp(boss.leanMaxRadians, boss.swing.speedLeanMaxDegrees * kRadiansPerDegree,
                               std::clamp(boss.swordPose.twoHand, 0.0f, 1.0f));
    const float forwardSpeed = velocity.x * std::sin(boss.yaw) + velocity.z * std::cos(boss.yaw);
    const float sideSpeed = velocity.x * std::cos(boss.yaw) - velocity.z * std::sin(boss.yaw);
    boss.leanForward = Lerp(boss.leanForward,
                            speedGate * std::clamp(forwardSpeed * boss.leanFromSpeed, -leanMax, leanMax), kLeanBlend);
    boss.leanSide =
        Lerp(boss.leanSide, speedGate * std::clamp(-sideSpeed * boss.leanFromSpeed, -leanMax, leanMax), kLeanBlend);
    // 剣の上下（前後の傾き、度）・左右（度）。下へ向けるほど前へ、左へ向けるほど右へ体を傾ける。両手の型の左右は
    // 握りの横の位置から。
    const Vec3 direction = Normalize(boss.swordPose.direction, {0.0f, -1.0f, 0.0f});
    const float pitch = std::asin(std::clamp(-direction.y, -1.0f, 1.0f)) / kRadiansPerDegree;
    const float side = Lerp(std::atan2(direction.x, direction.z) / kRadiansPerDegree,
                            boss.swordPose.grip.x / std::max(0.1f, boss.body.bodyScale) * kSwayDegreesPerGripMeter,
                            std::clamp(boss.swordPose.twoHand, 0.0f, 1.0f));
    const float blend = boss.state == BossState::Act && boss.phase == BossActPhase::Act ? kPoseBlendFast : kPoseBlend;
    boss.poseX = Lerp(boss.poseX, pitch, blend);
    boss.poseY = Lerp(boss.poseY, side, blend);
    boss.poseLean = Lerp(boss.poseLean, boss.swordPose.lean * kRadiansPerDegree, blend);
    boss.poseCrouch = Lerp(boss.poseCrouch, boss.swordPose.crouch, blend);
    // 剣の上下に合わせた前後の傾きは片手の構えのもの。両手の型は型が前傾を決めるので、その重みの分だけ消す（縦の
    // 振りかぶりで体が後ろへ反ると、頭上の手から肩が遠くなる）。左右の傾きは残す（握りの側へ肩を寄せる）。
    const float oneHand = 1.0f - std::clamp(boss.swordPose.twoHand, 0.0f, 1.0f);
    float lean = boss.leanForward +
                 oneHand * boss.poseLeanDegrees * kRadiansPerDegree * (boss.poseX - kPoseLeanNeutral) / 100.0f +
                 boss.poseLean;
    float sway = boss.leanSide - boss.poseSwayDegrees * kRadiansPerDegree * boss.poseY / 100.0f;
    // 霞歩：跳ぶ向きへ傾ける（速さの傾きは瞬間移動とみなす速さで切れるので、跳ぶ間は別に足す）。
    BossKasumiStep::AddLean(boss, lean, sway);
    if (boss.state == BossState::Stunned)
        lean += boss.stunLeanRadians + std::sin(boss.floatClock * kStunWobbleRate) * kStunWobble;
    // 怯み（c-808641）：怯んで跳ぶ技の前隙の間、上体を後ろへのけぞらせる（真ん中で最も大きい）。
    if (const BossMove* move = BossRules::GetMove(boss, boss.move);
        boss.state == BossState::Act && boss.phase == BossActPhase::Pre && move != nullptr &&
        move->name == boss.poiseFlinchMove)
        lean -= boss.poiseFlinchLeanDegrees * kRadiansPerDegree *
                std::sin(kPi * Share(boss.stateFrame, std::max(1, move->pre)));
    if (boss.state == BossState::Dead)
        lean += boss.stunLeanRadians * 2.0f;
    const float squash = 1.0f - std::clamp(boss.crouch, 0.0f, 1.0f) * boss.crouchSquash;
    const float bob = std::sin(boss.floatClock * kBobRate) * boss.floatBob;
    // 両手の斬りで腰を落とす分だけ体を下げる（見た目だけ。剣の線は構えの握りの高さのまま）。
    const Vec3 root{boss.ground.x, boss.ground.y + BossPose::RootHeight(boss) + bob - boss.poseCrouch, boss.ground.z};
    HIKARI::GameProjectTransform transform{};
    transform.rotation = YawLeanSway(boss.yaw + boss.spin, lean, sway);
    // 足元を支点に傾ける（中心で回すと足が反対へ流れる）。
    transform.position = Add(root, Rotate(transform.rotation, {0.0f, boss.bodyHalfHeight * squash, 0.0f}));
    // 撃破の後は、待ってから体を縮めて見えなくする（撃破の演出は別に作る）。
    float vanish = 1.0f;
    if (boss.state == BossState::Dead && boss.stateFrame > boss.deadVanishDelayFrames) {
        const float progress = boss.deadVanishFrames > 0
                                   ? static_cast<float>(boss.stateFrame - boss.deadVanishDelayFrames) /
                                         static_cast<float>(boss.deadVanishFrames)
                                   : 1.0f;
        vanish = std::max(kVanishMinScale, 1.0f - progress);
    }
    transform.scale = {vanish, squash * vanish, vanish};
    // 登場の演技の前と間は、体の物体を舞台に止める（演出の資産のカメラは物体に付く。浮遊のゆれ・傾きを入れない。
    // c-e0c500）。上の傾き・構えの値は、いつも通り進めておく（演技の後の戦いの見た目を変えない）。
    if (BossIntro::Pinned(boss))
        transform = BossIntro::StageTransform(boss);
    object.SetWorldTransform(transform);
    // 外形は描画のフレームで、前の tick とこの tick の間を補間して置く。
    BossBodyVisuals::NoteTick(boss, transform.position);
}

// プレイヤーの砲撃（最初の 1 つ）を、砲撃の規則の読むだけの関数から見る。
void SeeShot(HIKARI::WorldView& world, BossGearSight& gear) {
    bool seen = false;
    world.ForEachComponent<GourdShotComponent>(HIKARI::MakeComponentTypeId(kGourdShotType),
                                               [&](HIKARI::ObjectView, GourdShotComponent& shot) {
                                                   if (seen)
                                                       return;
                                                   seen = true;
                                                   gear.shotWindup = GourdShotRules::IsWindingUp(shot);
                                                   gear.shotChaining = GourdShotRules::IsChaining(shot);
                                                   gear.shotsFired = GourdShotRules::GetShotsFired(shot);
                                                   gear.shotCost = shot.smokeCost;
                                                   gear.aimOrigin = GourdShotRules::GetAimOrigin(shot);
                                                   gear.aimDirection = GourdShotRules::GetAimDirection(shot);
                                               });
}

void Trace(const BossComponent& boss, const DamageableComponent* health) {
    if (!TraceInterval::Due(boss.traceEveryFrames, boss.tick))
        return;
    // 規則の剣の構え（握り・向き）。姿勢の切り替えでどれだけ跳ぶかを tick ごとに見るための確かめ用。
    const BossSwordPose& sword = boss.swordPose;
    char message[640]{};
    std::snprintf(message, sizeof(message),
                  "Trace tick=%llu state=%d phase=%d move=%d stateFrame=%d ground=(%.3f,%.3f,%.3f) yaw=%.3f "
                  "height=%.3f float=%.3f crouch=%.3f stun=%.3f stunCount=%d health=%.3f fluid=%d sink=%.3f hidden=%d "
                  "clones=%d chargeVisual=%.3f grip=(%.3f,%.3f,%.3f) sword=(%.4f,%.4f,%.4f) twoHand=%.2f "
                  "bossPhase=%d shiftLeft=%d maxHealth=%.3f invulnerable=%d",
                  static_cast<unsigned long long>(boss.tick), static_cast<int>(boss.state),
                  static_cast<int>(boss.phase), boss.move, boss.stateFrame, static_cast<double>(boss.ground.x),
                  static_cast<double>(boss.ground.y), static_cast<double>(boss.ground.z), static_cast<double>(boss.yaw),
                  static_cast<double>(boss.height), static_cast<double>(boss.floatHeight),
                  static_cast<double>(boss.crouch), static_cast<double>(boss.stun), boss.stunCount,
                  health ? static_cast<double>(DamageableRules::GetHealth(*health)) : -1.0,
                  static_cast<int>(boss.fluidAccepted), static_cast<double>(boss.sink), static_cast<int>(boss.hidden),
                  boss.activeClones, static_cast<double>(boss.chargeVisual), static_cast<double>(sword.grip.x),
                  static_cast<double>(sword.grip.y), static_cast<double>(sword.grip.z),
                  static_cast<double>(sword.direction.x), static_cast<double>(sword.direction.y),
                  static_cast<double>(sword.direction.z), static_cast<double>(sword.twoHand), boss.lastPhaseNumber,
                  boss.phaseShiftLeft, health ? static_cast<double>(DamageableRules::GetMaxHealth(*health)) : -1.0,
                  health ? static_cast<int>(health->invulnerable) : -1);
    BossLog::Write(message);
}

} // namespace

void BossSystem::OnWorldAttached(HIKARI::WorldView&) { treeBrain_.Attach(); }

void BossSystem::FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame) || frame.fixedDt <= 0.0f)
        return;
    // 吸い込み中のヒョウタン（飛んで来た斬撃・ナイフを吸える）。煙がもう入らなければ（満杯）、吸えずに当たる。
    // 捕まえられるかは、ヒョウタンの空きから、口へ引き込み中の斬撃・ナイフが入りきった時に足す煙を先に引いて決める
    // （1 本で満杯になる時は、続けて来た次の 1 本が当たる。引き込み中の分は満杯になっても最後まで入る）。
    // 満杯でも吸える設定（Gourd.absorbWhenFull）なら、煙は増えないが斬撃は吸い取れる（前の扱い）。
    // ボスがプレイヤーの様子として見るのは、煙が入るかどうかに関わらず吸い込みの構えをしているか。
    bool inhaling = false;
    float catchRoom = 0.0f;
    // 捕まえた斬撃・ナイフを引き込む先（最初のヒョウタンの口）。
    Vec3 mouth{};
    bool hasMouth = false;
    // ボスが見るプレイヤーの道具（最初のヒョウタンと砲撃）。
    BossGearSight gear;
    world.ForEachComponent<GourdComponent>(HIKARI::MakeComponentTypeId(kGourdType), [&](HIKARI::ObjectView,
                                                                                        GourdComponent& gourd) {
        if (!hasMouth) {
            mouth = GourdRules::GetMouthPosition(gourd);
            hasMouth = true;
            const float capacity = GourdRules::GetCapacity(gourd);
            gear.hasGourd = true;
            gear.smokePercent = capacity > 0.0f ? GourdRules::GetSmoke(gourd) / capacity * 100.0f : 0.0f;
            gear.capacity = capacity;
            gear.gourdFull = !GourdRules::HasRoomToAbsorb(gourd);
        }
        if (!GourdRules::IsAbsorbing(gourd))
            return;
        inhaling = true;
        catchRoom = std::max(catchRoom, GourdRules::GetCatchRoom(gourd));
    });
    gear.absorbing = inhaling;
    SeeShot(world, gear);
    world.ForEachComponent<BossComponent>(
        HIKARI::MakeComponentTypeId(kBossType),
        [&](HIKARI::ObjectView, BossComponent& boss) { catchRoom -= BossWaves::GetPullingSmoke(boss); });
    BossCatchHandoff caught;
    // 口へ引き込んでいる途中の斬撃・ナイフの数（ある間は、ヒョウタンが口の吸う力と見た目を保つ）。
    int pulling = 0;
    world.ForEachComponent<BossComponent>(
        HIKARI::MakeComponentTypeId(kBossType), [&](HIKARI::ObjectView object, BossComponent& boss) {
            if (!boss.initialized)
                Initialize(world, object, boss);
            // 技のクリップの刀の骨（c-7b1a09）を、技の規則が使う前に読んでおく（クリップごとに初めての時だけ）。
            BossAnimBody::PrepareSword(world, boss);
            ++boss.tick;
            boss.clockSeconds += frame.fixedDt;
            boss.floatClock += frame.fixedDt;
            boss.swordSmoke = false;
            boss.chargeVisual = 0.0f;
            boss.bodySmokeRate = 0.0f;
            boss.glintFrames = std::max(0, boss.glintFrames - 1);
            // 参照はクエリのコールバックの中だけで使うので、プレイヤー → 同じ物体の Damageable の順に入れ子で処理する。
            auto body = [&](PlayerComponent* player, Vec3 playerCenter, DamageableComponent* health) {
                BossFrame bossFrame{world,    boss,       player, playerCenter, frame.fixedDt,
                                    inhaling, &catchRoom, mouth,  hasMouth};
                BossObserver::Observe(world, boss, player, playerCenter, gear, frame.fixedDt);
                // ヒョウタンの読み（c-14392d）：空に近い間の時間を数える。
                BossGourdRead::Observe(boss);
                // 兜が向く先（見た目だけが読む。規則は使わない）。
                boss.bodyState.lookTarget = playerCenter;
                boss.bodyState.lookTargetSeen = player != nullptr;
                if (player != nullptr)
                    BossIntro::Face(boss, playerCenter);
                BossIntro::Step(world, boss);
                BossHealthBars::Step(boss);
                BossPhaseShift::Step(boss);
                // 演出の間（登場・フェーズの変わり目）は、プレイヤーの入力を止めて無敵にする（毎 tick 入れ直す。
                // PlayerSystem が tick ごとに減らす）。変わり目の閃光の後の tick に、プレイヤーを押し出す。
                if (player != nullptr && (BossIntro::Cinematic(boss) || BossPhaseShift::Cinematic(boss)))
                    player->cinematicHold = std::max(player->cinematicHold, kCinematicHoldTicks);
                if (player != nullptr)
                    BossPhaseShift::Push(boss, *player, playerCenter);
                boss.shownBar = BossIntro::BarShown(boss) ? 1.0f : 0.0f;
                boss.shownCinematic = BossIntro::Cinematic(boss) ? 1.0f : 0.0f;
                if (BossIntro::Started(boss)) {
                    if (auto* memory = dynamic_cast<ISessionMemory*>(world.GetGameSession()))
                        memory->GetMemory().bossIntroSeen = true;
                }
                BossMoveExecutor::StepFloat(boss);
                BossBrainContext context{boss, object.GetHandle()};
                if (player != nullptr)
                    context.playerDistance = FlatLength(Sub(playerCenter, boss.ground));
                context.phaseNumber = boss.lastPhaseNumber;
                // 「HP60%以下」などは 2 本の条の合計に対する残り（1 本目 600・2 本目 900 なら 60% ちょうどでフェーズ 2）。
                if (health != nullptr && boss.healthBarsReady)
                    context.healthPercent = BossHealthBars::TotalPercent(boss, *health);
                // この tick に読む命中は、前の tick の終わり（＝この tick の始め）の状態の間に当たったもの。後隙だったなら
                // その技が咎められた（体を動かすと後隙が終わって待機へ戻ることがあるので、先に覚える）。
                const int recoveryRow =
                    boss.state == BossState::Act && boss.phase == BossActPhase::Post ? boss.move : -1;
                StepState(bossFrame, BossTestBrain::Wants(boss) ? static_cast<IBossBrain&>(testBrain_) : treeBrain_,
                          context);
                // 受けたダメージ（気絶値・フェーズ・撃破）は体を動かした後に読む（試作と同じ順で、気絶の長さが 1 F ずれない）。
                if (health != nullptr)
                    BossObserver::NotePunished(boss, recoveryRow, SyncHealth(bossFrame, object, *health));
                // 崩れ・気絶・撃破で引爆が打ち切られたら、溜めを燃える地面から下ろす。
                BossDetonate::Sync(bossFrame);
                // 天降り斬り（フェーズ 2）の着地の火は、技が終わった後も、爆ぜるまで進める。
                BossSkyfall::StepBurst(bossFrame);
                // 炎走りの前線は、放った技が終わった後・打ち切られた後（気絶の間）も、走り切るまで進める。
                BossFlameRun::Step(bossFrame);
                // ボスが動いた跡にも煙が残る（bossResidueStep ごとに 1 回ぶん。吸い込める）。
                // 空中を動いた分は数えない（天降り斬りで落ちる先の地面に、落ちる前から煙が出てしまう）。
                if (boss.height < kTrailMaxHeight)
                    boss.bodyTrail += FlatLength(Sub(boss.ground, boss.lastBodyPosition));
                boss.lastBodyPosition = boss.ground;
                if (boss.bodyTrail >= boss.bossResidueStep) {
                    boss.bodyTrail = 0.0f;
                    boss.bodySmokeRate = boss.bossResidue / frame.fixedDt;
                }
                BossWaves::Step(bossFrame);
                BossWaves::StepDeposits(boss);
                // 規則の剣の向きは、この tick の状態が決まった後に 1 度だけ求める（体の傾き・見た目・記録が読む）。
                // StepState の直後ではなくここに置く：被弾の処理（気絶・撃破）が状態を変えうるので、それを済ませた
                // 向きが、これまで ApplyTransform が求めていた向きと同じになる。
                boss.swordPose = BossPose::Target(boss);
                ApplyTransform(object, boss, frame.fixedDt);
                Trace(boss, health);
            };
            auto withHealth = [&](PlayerComponent* player, Vec3 playerCenter) {
                bool done = false;
                ForEachOnObject<DamageableComponent>(world, kDamageableType, object,
                                                     [&](HIKARI::ObjectView, DamageableComponent& health) {
                                                         if (done)
                                                             return;
                                                         done = true;
                                                         body(player, playerCenter, &health);
                                                     });
                if (!done)
                    body(player, playerCenter, nullptr);
            };
            // プレイヤーは 1 人。いなくても体と斬撃は動く（当たる相手がいないだけ）。
            bool ran = false;
            world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                                    [&](HIKARI::ObjectView playerObject, PlayerComponent& player) {
                                                        HIKARI::GameProjectTransform transform{};
                                                        if (ran || !playerObject.GetWorldTransform(transform))
                                                            return;
                                                        ran = true;
                                                        withHealth(&player, transform.position);
                                                        BossClones::Step(world, object, boss, &player,
                                                                         transform.position);
                                                    });
            if (!ran) {
                withHealth(nullptr, {});
                BossClones::Step(world, object, boss, nullptr, {});
            }
            // 吸い取った斬撃・ナイフの渡し分を、ボスごとに集めて空にする（渡すのは下でまとめて 1 回）。
            caught.smoke += boss.caught.smoke;
            caught.count += boss.caught.count;
            caught.fireCount += boss.caught.fireCount;
            boss.caught = {};
            for (const auto& wave : boss.waves)
                pulling += wave.active && wave.captured ? 1 : 0;
        });
    if (caught.smoke <= 0.0f && caught.count == 0 && pulling == 0)
        return;
    world.ForEachComponent<GourdComponent>(
        HIKARI::MakeComponentTypeId(kGourdType), [&](HIKARI::ObjectView, GourdComponent& gourd) {
            // 引き込みの間と、入りきった直後は、吸い込みをやめていても口が飲み込み続ける。
            GourdRules::NotifySwallowing(gourd);
            if (caught.smoke <= 0.0f && caught.count == 0)
                return;
            // 吸った合図は、ヒョウタン自身が次の描画フレームで見せる（ボスはカメラに触れない）。
            GourdRules::NotifyCatch(gourd, caught.count);
            // 火の刃を吸い取ったら火弾になる（捕まえた時に空きがあった分。入りきった時に満杯になっていても）。
            GourdRules::NotifyFireCatch(gourd, caught.fireCount);
            const float added = GourdRules::AddSmoke(gourd, caught.smoke);
            char message[192]{};
            std::snprintf(message, sizeof(message), "AbsorbSmoke amount=%.3f added=%.3f smoke=%.3f caught=%d fire=%d",
                          static_cast<double>(caught.smoke), static_cast<double>(added),
                          static_cast<double>(GourdRules::GetSmoke(gourd)), caught.count, caught.fireCount);
            BossLog::Write(message);
            caught = {};
        });
}

void BossSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    // 編集中（Play の外）：登場の演出の資産のプレビューだけ（c-e0c500。ほかの見た目・規則は動かさない）。
    if (world.GetGameSession() == nullptr) {
        BossBodyVisuals::UpdatePreviews(world);
        return;
    }
    if (!IsPlaying(world, frame))
        return;
    size_t demand = 0;
    world.ForEachComponent<BossComponent>(HIKARI::MakeComponentTypeId(kBossType),
                                          [&](HIKARI::ObjectView object, BossComponent& boss) {
                                              if (!boss.initialized)
                                                  return;
                                              BossVisuals::Update(world, object, boss, frame.gameDt,
                                                                  frame.fixedInterpolationAlpha);
                                              BossBodyVisuals::Update(world, object, boss, frame.gameDt,
                                                                      frame.fixedInterpolationAlpha);
                                              // フェーズ 2 の場の色・火の粉・焦げた輪（外形を出さないフレームも進める。c-857ae3）。
                                              BossPhaseTwoField::Update(world, boss, frame.gameDt);
                                              // 霞分身の外形（分身の物体は BossClones と同じく部品の型で回る）。
                                              world.ForEachComponent<BossCloneComponent>(
                                                  HIKARI::MakeComponentTypeId(kBossCloneType),
                                                  [&](HIKARI::ObjectView cloneObject, BossCloneComponent& clone) {
                                                      BossBodyVisuals::UpdateClone(world, cloneObject, boss, clone,
                                                                                   frame.gameDt);
                                                  });
                                              BossFeedback::Update(world, object, boss, frame);
                                              BossIntroEffects::Feedback(world, object, boss, frame.unscaledDt);
                                              // 登場の演出：飛ばす入力と、演出のカメラ（プレイヤーのカメラへ渡す）。
                                              if (BossIntro::Cinematic(boss) && !boss.intro.skipAction.empty() &&
                                                  GameSession::ReadAction(world.GetInput(), boss.intro.skipAction)
                                                      .pressed)
                                                  boss.introState.skipRequested = true;
                                              // フェーズの変わり目の演出：飛ばす入力（項目で切れる）。
                                              if (BossPhaseShift::Cinematic(boss) && boss.shift.skippable &&
                                                  !boss.shift.skipAction.empty() &&
                                                  GameSession::ReadAction(world.GetInput(), boss.shift.skipAction)
                                                      .pressed)
                                                  boss.shiftState.skipRequested = true;
                                              world.ForEachComponent<PlayerComponent>(
                                                  HIKARI::MakeComponentTypeId(kPlayerType),
                                                  [&](HIKARI::ObjectView, PlayerComponent& player) {
                                                      BossIntroCamera::Update(boss, player);
                                                      BossIntroCamera::UpdateShift(boss, player);
                                                  });
                                              demand += BossRules::GetFluidDomainDemand(boss);
                                          });
    // 斬撃のエフェクトが使う流体域の分だけ、砲撃の炎の枠を譲ってもらう（毎フレーム入れ直す）。
    if (demand > 0) {
        world.ForEachComponent<GourdShotComponent>(
            HIKARI::MakeComponentTypeId(kGourdShotType),
            [&](HIKARI::ObjectView, GourdShotComponent& shot) { shot.reservedDomains = demand; });
    }
}

void BossSystem::PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    float smokePerVolume = kFallbackSmokePerVolume;
    bool found = false;
    world.ForEachComponent<GourdComponent>(HIKARI::MakeComponentTypeId(kGourdType),
                                           [&](HIKARI::ObjectView, GourdComponent& gourd) {
                                               if (!found) {
                                                   smokePerVolume = GourdRules::GetSmokePerVolume(gourd);
                                                   found = true;
                                               }
                                           });
    world.ForEachComponent<BossComponent>(HIKARI::MakeComponentTypeId(kBossType),
                                          [&](HIKARI::ObjectView object, BossComponent& boss) {
                                              if (!boss.initialized)
                                                  return;
                                              BossSmoke::Render(world, object, boss, smokePerVolume);
                                              BossIntroEffects::Render(world, object, boss, frame.gameDt);
                                          });
}

void BossSystem::OnWorldDetached(HIKARI::WorldView& world) {
    world.ForEachComponent<BossComponent>(HIKARI::MakeComponentTypeId(kBossType),
                                          [&](HIKARI::ObjectView, BossComponent& boss) {
                                              BossVisuals::Stop(world, boss);
                                              BossBodyVisuals::Stop(world, boss);
                                              BossPhaseTwoField::Stop(world, boss);
                                              BossWaves::Clear(boss);
                                          });
}

} // namespace Project_TD2_2
