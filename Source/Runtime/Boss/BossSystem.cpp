#include "BossSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

#include "HIKARI/Project/ProjectDataView.h"
#include "HIKARI/Runtime/ComponentTypeId.h"
#include "BossClones.h"
#include "BossDetonate.h"
#include "BossFeedback.h"
#include "BossMath.h"
#include "BossMoveExecutor.h"
#include "BossObserver.h"
#include "BossPose.h"
#include "BossRules.h"
#include "BossSelection.h"
#include "BossSmoke.h"
#include "BossVisuals.h"
#include "BossWaves.h"
#include "../Combat/DamageableRules.h"
#include "../Gourd/GourdRules.h"
#include "../Gourd/GourdShotComponent.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 姿勢をなめらかにする割合（1 tick あたり）。攻撃中は速く追う（試作の描画と同じ）。
constexpr float kPoseBlend = 0.32f;
constexpr float kPoseBlendFast = 0.7f;
constexpr float kLeanBlend = 0.1f;
constexpr float kStunCrouch = 0.85f;
constexpr float kStunCrouchBlend = 0.12f;
// 剣の上下（度）がこの値のとき体はまっすぐ。上げると後ろへ、振り下ろすと前へ傾く。
constexpr float kPoseLeanNeutral = 30.0f;
// これより速い位置の変化（瞬間移動）では、動きの傾きを付けない (m/秒)。
constexpr float kLeanSpeedLimit = 30.0f;
// 体のゆれの周期（rad/秒）。
constexpr float kBobRate = 2.1f;
constexpr float kStunWobbleRate = 1.7f;
constexpr float kStunWobble = 0.05f;
// 煙の量の換算が取れないとき（ヒョウタンがいない）の値。ヒョウタンの既定値と同じ。
constexpr float kFallbackSmokePerVolume = 4.0f;
// ヒョウタンの空きがこれより少なければ、斬撃・ナイフは吸えない（試作の 0.01）。
constexpr float kAbsorbRoomEpsilon = 0.01f;

bool IsPlaying(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    return world.GetGameSession() != nullptr && !frame.paused;
}

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
    TextSink types, moves;
    world.GetProjectData().ReadText(boss.typesPath, types);
    world.GetProjectData().ReadText(boss.movesPath, moves);
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
    if (types.text == boss.loadedTypesText && moves.text == boss.loadedMovesText)
        return;
    std::vector<std::string> warnings;
    BossMoveTable table;
    table.LoadTypes(types.text, warnings);
    table.LoadMoves(moves.text, TimingOf(boss), warnings);
    boss.table = std::move(table);
    boss.loadedTypesText = types.text;
    boss.loadedMovesText = moves.text;
    boss.tableLoaded = true;
    // 行が変わったので、行動ツリーの節と表の突き合わせ（節の無い始動技・表に無い名前）をやり直す。
    boss.brain.scoredRows = 0;
    boss.brain.coverageChecked = false;
    boss.brain.unknownCount = 0;
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
    char message[160]{};
    std::snprintf(message, sizeof(message), "StunGain tick=%llu amount=%.3f stun=%.3f max=%.3f",
                  static_cast<unsigned long long>(boss.tick), static_cast<double>(amount),
                  static_cast<double>(boss.stun), static_cast<double>(boss.stunMax));
    BossLog::Write(message);
    if (boss.stun >= boss.stunMax)
        EnterStun(boss);
}

// 崩せる大技：前隙（溜めを含む）の間に表の「崩せる被弾数」だけ当てられたら、技を打ち切って気絶させる。
// 当たりは Damageable が受け付けた命中（無敵で弾かれたものは数えない）。崩れた気絶も気絶の回数に数える。
void CheckBreak(BossComponent& boss, uint64_t freshHits) {
    if (freshHits == 0 || boss.state != BossState::Act || boss.phase != BossActPhase::Pre)
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

// HP の持ち主は Damageable。受けた量から気絶値をため、フェーズと死亡を決める。
void SyncHealth(BossFrame& frame, HIKARI::ObjectView object, DamageableComponent& health) {
    BossComponent& boss = frame.boss;
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
    CheckBreak(boss, BossSelection::NoteHits(boss, DamageableRules::GetHitCount(health)));
    const double base = DamageableRules::GetBaseDamageTotal(health);
    const double delta = base - boss.seenBaseDamage;
    boss.seenBaseDamage = base;
    if (delta > 0.0)
        AddStun(boss, static_cast<float>(delta) / std::max(0.01f, boss.stunReferenceDamage) * boss.stunPerShot);
    // しばらく当てないと気絶値が減る。
    if (boss.state != BossState::Stunned && boss.stun > 0.0f &&
        boss.clockSeconds - boss.stunHitSeconds > boss.stunDecayDelaySeconds)
        boss.stun = std::max(0.0f, boss.stun - boss.stunDecayPerSecond * frame.dt);
    const int phase = BossRules::GetPhaseNumber(boss, health);
    if (phase != boss.lastPhaseNumber) {
        char message[160]{};
        std::snprintf(message, sizeof(message), "PhaseChange tick=%llu phase=%d health=%.3f",
                      static_cast<unsigned long long>(boss.tick), phase,
                      static_cast<double>(DamageableRules::GetHealth(health)));
        BossLog::Write(message);
        boss.lastPhaseNumber = phase;
    }
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
    DamageableRules::SetIncomingDamageScale(health, boss.state == BossState::Stunned ? boss.stunDamageScale : 1.0f);
    // 霞に隠れている間は当たらない（体は地面の下へ沈めるが、沈みきるまでの数 F も無敵にする）。
    if (boss.hidden != boss.hiddenApplied) {
        health.invulnerable = boss.hidden;
        boss.hiddenApplied = boss.hidden;
    }
}

void StartRequested(BossFrame& frame, int chain) {
    BossComponent& boss = frame.boss;
    LoadTable(frame.world, boss);
    const int index = boss.requestedMove;
    BossRules::CancelRequest(boss);
    const BossMove* move = BossRules::GetMove(boss, index);
    if (move == nullptr || !move->executable)
        return;
    BossMoveExecutor::Start(frame, index, chain);
    BossSelection::NoteStarted(boss, index, chain);
}

void StepState(BossFrame& frame, IBossBrain& brain, BossBrainContext& context) {
    BossComponent& boss = frame.boss;
    switch (boss.state) {
    case BossState::Dead: boss.crouch = Lerp(boss.crouch, 1.0f, kStunCrouchBlend); return;
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
        BossMoveExecutor::StepIdle(frame);
        if (boss.requestedMove < 0)
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
            else if (context.recoveryDone)
                BossMoveExecutor::EnterIdle(boss);
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
    const float forwardSpeed = velocity.x * std::sin(boss.yaw) + velocity.z * std::cos(boss.yaw);
    const float sideSpeed = velocity.x * std::cos(boss.yaw) - velocity.z * std::sin(boss.yaw);
    boss.leanForward =
        Lerp(boss.leanForward,
             speedGate * std::clamp(forwardSpeed * boss.leanFromSpeed, -boss.leanMaxRadians, boss.leanMaxRadians),
             kLeanBlend);
    boss.leanSide = Lerp(
        boss.leanSide,
        speedGate * std::clamp(-sideSpeed * boss.leanFromSpeed, -boss.leanMaxRadians, boss.leanMaxRadians), kLeanBlend);
    const BossSwordPose target = BossPose::Target(boss);
    const float blend = boss.state == BossState::Act && boss.phase == BossActPhase::Act ? kPoseBlendFast : kPoseBlend;
    boss.poseX = Lerp(boss.poseX, target.x, blend);
    boss.poseY = Lerp(boss.poseY, target.y, blend);
    float lean = boss.leanForward + boss.poseLeanDegrees * kRadiansPerDegree * (boss.poseX - kPoseLeanNeutral) / 100.0f;
    const float sway = boss.leanSide - boss.poseSwayDegrees * kRadiansPerDegree * boss.poseY / 100.0f;
    if (boss.state == BossState::Stunned)
        lean += boss.stunLeanRadians + std::sin(boss.floatClock * kStunWobbleRate) * kStunWobble;
    if (boss.state == BossState::Dead)
        lean += boss.stunLeanRadians * 2.0f;
    const float squash = 1.0f - std::clamp(boss.crouch, 0.0f, 1.0f) * boss.crouchSquash;
    const float bob = std::sin(boss.floatClock * kBobRate) * boss.floatBob;
    const Vec3 root{boss.ground.x, boss.ground.y + BossPose::RootHeight(boss) + bob, boss.ground.z};
    HIKARI::GameProjectTransform transform{};
    transform.rotation = YawLeanSway(boss.yaw + boss.spin, lean, sway);
    // 足元を支点に傾ける（中心で回すと足が反対へ流れる）。
    transform.position = Add(root, Rotate(transform.rotation, {0.0f, boss.bodyHalfHeight * squash, 0.0f}));
    transform.scale = {1.0f, squash, 1.0f};
    object.SetWorldTransform(transform);
}

void Trace(const BossComponent& boss, const DamageableComponent* health) {
    if (boss.traceEveryFrames <= 0 || boss.tick % static_cast<uint64_t>(boss.traceEveryFrames) != 0)
        return;
    char message[448]{};
    std::snprintf(message, sizeof(message),
                  "Trace tick=%llu state=%d phase=%d move=%d stateFrame=%d ground=(%.3f,%.3f,%.3f) yaw=%.3f "
                  "height=%.3f float=%.3f crouch=%.3f stun=%.3f stunCount=%d health=%.3f fluid=%d sink=%.3f hidden=%d "
                  "clones=%d",
                  static_cast<unsigned long long>(boss.tick), static_cast<int>(boss.state),
                  static_cast<int>(boss.phase), boss.move, boss.stateFrame, static_cast<double>(boss.ground.x),
                  static_cast<double>(boss.ground.y), static_cast<double>(boss.ground.z), static_cast<double>(boss.yaw),
                  static_cast<double>(boss.height), static_cast<double>(boss.floatHeight),
                  static_cast<double>(boss.crouch), static_cast<double>(boss.stun), boss.stunCount,
                  health ? static_cast<double>(DamageableRules::GetHealth(*health)) : -1.0,
                  static_cast<int>(boss.fluidAccepted), static_cast<double>(boss.sink), static_cast<int>(boss.hidden),
                  boss.activeClones);
    BossLog::Write(message);
}

} // namespace

void BossSystem::OnWorldAttached(HIKARI::WorldView&) { treeBrain_.Attach(); }

void BossSystem::FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame) || frame.fixedDt <= 0.0f)
        return;
    // 吸い込み中のヒョウタン（飛んで来た斬撃・ナイフを吸える）。満杯でも吸う設定（Gourd.absorbWhenFull）なら、煙は増えないが
    // 斬撃は吸い取れる（砲撃の煙のコストを 0 にした場面ではいつも満杯で、吸い込みが何もできなくなるため）。
    // 切なら、煙がもう入らなければ吸えずに当たる（試作と同じ）。
    // ボスがプレイヤーの様子として見るのは、煙が入るかどうかに関わらず吸い込みの構えをしているか。
    bool absorbing = false, inhaling = false;
    world.ForEachComponent<GourdComponent>(HIKARI::MakeComponentTypeId(kGourdType), [&](HIKARI::ObjectView,
                                                                                        GourdComponent& gourd) {
        inhaling = inhaling || GourdRules::IsAbsorbing(gourd);
        absorbing = absorbing ||
                    (GourdRules::IsAbsorbing(gourd) &&
                     (gourd.absorbWhenFull ||
                      GourdRules::GetCapacity(gourd) - GourdRules::GetSmoke(gourd) > kAbsorbRoomEpsilon));
    });
    float absorbed = 0.0f;
    int caught = 0;
    world.ForEachComponent<BossComponent>(
        HIKARI::MakeComponentTypeId(kBossType), [&](HIKARI::ObjectView object, BossComponent& boss) {
            if (!boss.initialized)
                Initialize(world, object, boss);
            ++boss.tick;
            boss.clockSeconds += frame.fixedDt;
            boss.floatClock += frame.fixedDt;
            boss.swordSmoke = false;
            boss.bodySmokeRate = 0.0f;
            boss.glintFrames = std::max(0, boss.glintFrames - 1);
            // 参照はクエリのコールバックの中だけで使うので、プレイヤー → 同じ物体の Damageable の順に入れ子で処理する。
            auto body = [&](PlayerComponent* player, Vec3 playerCenter, DamageableComponent* health) {
                BossFrame bossFrame{world, boss, player, playerCenter, frame.fixedDt, absorbing};
                BossObserver::Observe(world, boss, player, playerCenter, inhaling);
                BossMoveExecutor::StepFloat(boss);
                BossBrainContext context{boss, object.GetHandle()};
                if (player != nullptr)
                    context.playerDistance = FlatLength(Sub(playerCenter, boss.ground));
                context.phaseNumber = boss.lastPhaseNumber;
                if (health != nullptr && DamageableRules::GetMaxHealth(*health) > 0.0f)
                    context.healthPercent = static_cast<double>(DamageableRules::GetHealth(*health)) /
                                            static_cast<double>(DamageableRules::GetMaxHealth(*health)) * 100.0;
                StepState(bossFrame, BossTestBrain::Wants(boss) ? static_cast<IBossBrain&>(testBrain_) : treeBrain_,
                          context);
                // 受けたダメージ（気絶値・フェーズ・撃破）は体を動かした後に読む（試作と同じ順で、気絶の長さが 1 F ずれない）。
                if (health != nullptr)
                    SyncHealth(bossFrame, object, *health);
                // 崩れ・気絶・撃破で引爆が打ち切られたら、溜めを燃える地面から下ろす。
                BossDetonate::Sync(bossFrame);
                // ボスが動いた跡にも煙が残る（bossResidueStep ごとに 1 回ぶん。吸い込める）。
                boss.bodyTrail += FlatLength(Sub(boss.ground, boss.lastBodyPosition));
                boss.lastBodyPosition = boss.ground;
                if (boss.bodyTrail >= boss.bossResidueStep) {
                    boss.bodyTrail = 0.0f;
                    boss.bodySmokeRate = boss.bossResidue / frame.fixedDt;
                }
                BossWaves::Step(bossFrame);
                BossWaves::StepDeposits(boss);
                ApplyTransform(object, boss, frame.fixedDt);
                Trace(boss, health);
            };
            auto withHealth = [&](PlayerComponent* player, Vec3 playerCenter) {
                bool done = false;
                world.ForEachComponent<DamageableComponent>(
                    HIKARI::MakeComponentTypeId(kDamageableType),
                    [&](HIKARI::ObjectView healthObject, DamageableComponent& health) {
                        if (!done && healthObject.GetHandle().value == object.GetHandle().value) {
                            done = true;
                            body(player, playerCenter, &health);
                        }
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
            absorbed += boss.absorbedSmoke;
            boss.absorbedSmoke = 0.0f;
            caught += boss.absorbedWavesPending;
            boss.absorbedWavesPending = 0;
        });
    if (absorbed <= 0.0f && caught == 0)
        return;
    world.ForEachComponent<GourdComponent>(
        HIKARI::MakeComponentTypeId(kGourdType), [&](HIKARI::ObjectView, GourdComponent& gourd) {
            // 吸った合図は、ヒョウタン自身が次の描画フレームで見せる（ボスはカメラに触れない）。
            GourdRules::NotifyCatch(gourd, caught);
            caught = 0;
            const float added = GourdRules::AddSmoke(gourd, absorbed);
            char message[160]{};
            std::snprintf(message, sizeof(message), "AbsorbSmoke amount=%.3f added=%.3f smoke=%.3f",
                          static_cast<double>(absorbed), static_cast<double>(added),
                          static_cast<double>(GourdRules::GetSmoke(gourd)));
            BossLog::Write(message);
            absorbed = 0.0f;
        });
}

void BossSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    size_t demand = 0;
    world.ForEachComponent<BossComponent>(HIKARI::MakeComponentTypeId(kBossType),
                                          [&](HIKARI::ObjectView object, BossComponent& boss) {
                                              if (!boss.initialized)
                                                  return;
                                              BossVisuals::Update(world, object, boss, frame.gameDt);
                                              BossFeedback::Update(world, object, boss, frame);
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
                                              if (boss.initialized)
                                                  BossSmoke::Render(world, object, boss, smokePerVolume);
                                          });
}

void BossSystem::OnWorldDetached(HIKARI::WorldView& world) {
    world.ForEachComponent<BossComponent>(HIKARI::MakeComponentTypeId(kBossType),
                                          [&](HIKARI::ObjectView, BossComponent& boss) {
                                              BossVisuals::Stop(world, boss);
                                              BossWaves::Clear(boss);
                                          });
}

} // namespace Project_TD2_2
