#include "BossMoveExecutor.h"

#include <algorithm>
#include <cmath>

#include "BossMath.h"
#include "BossPose.h"
#include "BossSpecialMoves.h"
#include "BossWaves.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 体の動きをなめらかにする追従の割合（1 tick あたり）。試作の値。
constexpr float kVelocityBlend = 0.04f;
constexpr float kIdleCrouchBlend = 0.2f;
constexpr float kIdleHeightBlend = 0.3f;
constexpr float kDashCrouchBlend = 0.15f;
constexpr float kFloatAttackBlend = 0.1f;
constexpr float kFloatMoveBlend = 0.035f;
constexpr float kFloatIdleBlend = 0.02f;
// 待機中の横歩き：24F を過ぎたら 30F ごとに 30% で向きを変える。
constexpr int kStrafeFlipStart = 24;
constexpr int kStrafeFlipEvery = 30;
constexpr float kStrafeFlipChance = 0.3f;
// 浮く高さを選び直す間隔 (F)：50〜130。
constexpr int kFloatRetargetMin = 50;
constexpr int kFloatRetargetSpread = 80;
// 瞬間突進：構えの前の瞬間移動は前隙の 1/4 まで、構えは前隙の半分まで。
constexpr float kDashTeleportShare = 0.25f;
constexpr float kDashKamaeShare = 0.5f;
constexpr float kDashCrouchSpeed = 1.5f;
// 上段：前隙の最後の 15% で踏み込む。
constexpr float kOverheadLungeStart = 0.85f;

const BossMove& CurrentMove(const BossComponent& boss) { return boss.table.GetMoves()[static_cast<size_t>(boss.move)]; }

Vec3 PlayerGround(const BossFrame& frame) { return {frame.playerCenter.x, frame.boss.ground.y, frame.playerCenter.z}; }

void Log(const BossComponent& boss, const char* event, const char* extra = "") {
    const BossMove* move =
        boss.move >= 0 && static_cast<size_t>(boss.move) < boss.table.GetMoves().size() ? &CurrentMove(boss) : nullptr;
    char message[384]{};
    std::snprintf(message, sizeof(message), "%s tick=%llu move=%s chain=%d stateFrame=%d %s", event,
                  static_cast<unsigned long long>(boss.tick), move ? move->name.c_str() : "-", boss.chain,
                  boss.stateFrame, extra);
    BossLog::Write(message);
}

// 構え：剣が止まって霞が剣先に集まる → kamaeFrames F 後に振る。
void Glint(BossComponent& boss) {
    boss.glintFrames = std::max(1, boss.kamaeSmokeFrames);
    Log(boss, "Kamae");
}

Vec3 AimDirection(const BossFrame& frame, Vec3 from) {
    const Vec3 forward = Forward(frame.boss.yaw);
    if (frame.player == nullptr)
        return forward;
    const float horizontal = std::max(1.0f, FlatLength(Sub(frame.playerCenter, from)));
    const Vec3 aim{forward.x * horizontal, frame.playerCenter.y + frame.boss.playerAimHeight - from.y,
                   forward.z * horizontal};
    return Normalize(aim, forward);
}

// 連撃の左右移動：一撃ごとに左右交互へ動く。構えに入るところが一番速く、振る瞬間に向けて減速する。
void SideMove(BossFrame& frame, const BossMove& move) {
    BossComponent& boss = frame.boss;
    if (move.type != BossMoveType::String || !(move.side > 0.0f) || move.segments.empty())
        return;
    const auto swingOf = [&](const BossSegment& segment) {
        return std::max(1, std::min(segment.length, boss.swingInFrames + boss.swingFrames));
    };
    int firstHit = 0;
    while (firstHit < static_cast<int>(move.segments.size()) && !move.segments[static_cast<size_t>(firstHit)].hit)
        ++firstHit;
    if (firstHit >= static_cast<int>(move.segments.size()))
        return;
    int strike = 0, tw = 0, length = 1;
    if (boss.phase == BossActPhase::Pre) {
        tw = boss.stateFrame;
        length = move.pre + swingOf(move.segments[static_cast<size_t>(firstHit)]);
    } else if (boss.phase == BossActPhase::Act) {
        const auto g = BossPose::Progress(boss, move);
        const BossSegment& segment = move.segments[static_cast<size_t>(g.segment)];
        strike = segment.strike;
        if (g.hit) {
            const int before =
                g.segment == firstHit ? move.pre : move.segments[static_cast<size_t>(g.segment - 1)].length;
            tw = before + g.tIn;
            length = before + g.swingLength;
        } else {
            if (static_cast<size_t>(g.segment + 1) >= move.segments.size())
                return;
            tw = g.tIn;
            length = segment.length + swingOf(move.segments[static_cast<size_t>(g.segment + 1)]);
        }
    } else {
        return;
    }
    if (tw < 1 || tw > length)
        return;
    const auto ease = [](float u) { return 1.0f - std::pow(1.0f - std::clamp(u, 0.0f, 1.0f), 3.0f); };
    const float d = move.side * (ease(static_cast<float>(tw) / length) - ease(static_cast<float>(tw - 1) / length));
    const Vec3 toPlayer = FlatNormalize(Sub(PlayerGround(frame), boss.ground));
    const BossStrikeDirection direction = BossMoveTable::Strike(move, strike).direction;
    // 横の一撃は左へ、縦の一撃は右へ（プレイヤーから見た左右）。袈裟などは左右交互。
    const float sign = direction == BossStrikeDirection::Horizontal ? -1.0f
                       : direction == BossStrikeDirection::Vertical ? 1.0f
                                                                    : (strike % 2 ? -1.0f : 1.0f) * boss.sideSign;
    boss.ground = Add(boss.ground, Mul(Vec3{toPlayer.z, 0.0f, -toPlayer.x}, d * sign));
}

void PreMotion(BossFrame& frame, const BossMove& move, float progress, Vec3 toPlayer, float distance) {
    BossComponent& boss = frame.boss;
    const int t = boss.stateFrame;
    const bool inKamae = move.pre - t < std::min(boss.kamaeFrames, move.pre);
    // 前隙の間はずっとプレイヤーを向く。
    BossMoveExecutor::FaceRate(frame, move.type == BossMoveType::Overhead ? boss.overheadTurnRate : boss.preTurnRate);
    const Vec3 forward = Forward(boss.yaw);
    switch (move.type) {
    case BossMoveType::Dash: {
        boss.crouch = std::min(1.0f, progress * kDashCrouchSpeed);
        const float kamae = std::min(static_cast<float>(boss.kamaeFrames), move.pre * kDashKamaeShare);
        const float teleport = std::min(static_cast<float>(boss.dashTeleportFrames), move.pre * kDashTeleportShare);
        const float start = static_cast<float>(move.pre) - kamae - teleport;
        if (static_cast<float>(t) >= start && frame.player != nullptr) {
            // 瞬間移動を終えてから構える。行き先はプレイヤーの手前。
            if (!boss.dashStarted) {
                boss.dashStarted = true;
                boss.dashFrom = boss.ground;
                boss.dashTo = Sub(PlayerGround(frame), Mul(toPlayer, boss.dashStopDistance));
                BossMoveExecutor::ClampToArena(boss, boss.dashTo);
                char extra[96]{};
                std::snprintf(extra, sizeof(extra), "distance=%.3f",
                              static_cast<double>(FlatLength(Sub(boss.dashTo, boss.dashFrom))));
                Log(boss, "Teleport", extra);
            }
            const float k = std::min(1.0f, (static_cast<float>(t) - start) / std::max(1.0f, teleport));
            boss.ground = Lerp(boss.dashFrom, boss.dashTo, 1.0f - std::pow(1.0f - k, 3.0f));
            boss.yaw = YawOf(Sub(PlayerGround(frame), boss.ground));
        }
        break;
    }
    case BossMoveType::SlashR:
    case BossMoveType::SlashL:
    case BossMoveType::SlashH:
    case BossMoveType::SlashV:
        if (!inKamae)
            boss.ground = Add(boss.ground, Mul(forward, boss.slashApproachSpeed * frame.dt));
        break;
    case BossMoveType::Overhead:
        if (progress > kOverheadLungeStart)
            boss.ground = Add(boss.ground, Mul(forward, boss.overheadLungeDistance /
                                                            std::max(1.0f, move.pre * (1.0f - kOverheadLungeStart))));
        break;
    case BossMoveType::String:
        if (distance > boss.stringApproachStop && !inKamae)
            boss.ground = Add(boss.ground, Mul(forward, boss.stringApproachSpeed * frame.dt));
        break;
    default: BossSpecialMoves::Pre(frame, move, progress); break;
    }
}

void ActMotion(BossFrame& frame, const BossMove& move, float progress) {
    BossComponent& boss = frame.boss;
    if (move.type == BossMoveType::String && !BossPose::Progress(boss, move).hit)
        BossMoveExecutor::FaceRate(frame, boss.stringGapTurnRate);
    if (BossMoveTable::IsMoveType(move.type)) {
        const Vec3 offset = Sub(PlayerGround(frame), boss.ground);
        const float distance = FlatLength(offset);
        const Vec3 toPlayer = FlatNormalize(offset);
        BossMoveExecutor::FaceRate(frame, boss.idleTurnRate);
        const Vec3 tangent = Mul(Vec3{toPlayer.z, 0.0f, -toPlayer.x}, boss.strafe);
        const bool sideways = move.type == BossMoveType::WalkSide ||
                              (move.type == BossMoveType::WalkIn && distance < boss.walkInStopDistance);
        if (sideways || move.type == BossMoveType::WalkIn) {
            const Vec3 want =
                sideways ? Mul(tangent, boss.walkSpeed) : Mul(toPlayer, boss.walkSpeed * boss.walkInSpeedScale);
            boss.velocity = Lerp(boss.velocity, want, kVelocityBlend);
            boss.ground = Add(boss.ground, Mul(boss.velocity, frame.dt));
        } else if (move.type == BossMoveType::Backstep) {
            const float u = std::clamp(progress, 0.0f, 1.0f);
            const float e = 1.0f - (1.0f - u) * (1.0f - u);
            boss.ground = Add(boss.stepFrom, Mul(toPlayer, -boss.backstepDistance * e));
            boss.height = boss.backstepHeight * std::sin(u * kPi);
        }
    }
    if (move.type == BossMoveType::Dash)
        boss.crouch = Lerp(boss.crouch, 0.0f, kDashCrouchBlend);
}

// 踏み込み：振っている間にプレイヤーへ近づく（距離は表の「踏み込み m」）。近すぎるときは止まる。
void StepIn(BossFrame& frame, const BossStrikeData& strike, const BossSegmentProgress& g) {
    BossComponent& boss = frame.boss;
    // 霞隠れは踏み込まない（現れてから突っ込む動きは BossSpecialMoves）。
    if (!(strike.step > 0.0f) || g.tIn > g.swingLength || g.tIn < 1 || CurrentMove(boss).type == BossMoveType::Ambush)
        return;
    float d = strike.step * (Smooth(static_cast<float>(g.tIn) / g.swingLength) -
                             Smooth(static_cast<float>(g.tIn - 1) / g.swingLength));
    const float room = FlatLength(Sub(PlayerGround(frame), boss.ground)) -
                       (boss.bodyRadius + boss.playerHitRadius + boss.stepInClearance);
    d = std::min(d, std::max(0.0f, room));
    if (d > 0.0f)
        boss.ground = Add(boss.ground, Mul(Forward(boss.yaw), d));
}

// 剣を振り抜いた瞬間：剣先が描いた霞の弧から、ボスが向いている方向へまっすぐ斬撃を飛ばす（斬撃は追尾しない）。
void Release(BossFrame& frame, const BossSwing& swing) {
    BossComponent& boss = frame.boss;
    if (swing.samples < 2)
        return;
    const Vec3 forward = Forward(boss.yaw);
    const float averageHeight = swing.heightSum / static_cast<float>(swing.samples);
    Vec3 from = Add(boss.ground, Mul(forward, boss.bodyRadius + boss.waveForward));
    from.y = boss.ground.y + std::clamp(averageHeight, boss.waveMinHeight, boss.waveMaxHeight + boss.height);
    const Vec3 direction = AimDirection(frame, from);
    Vec3 across = Sub(swing.lastOuter, swing.firstOuter);
    across = Sub(across, Mul(direction, Dot(across, direction)));
    if (Dot(across, across) < 1e-4f)
        across = Cross(direction, {0.0f, 1.0f, 0.0f});
    BossSlashLaunch launch;
    launch.from = from;
    launch.direction = direction;
    launch.across = Normalize(across, {1.0f, 0.0f, 0.0f});
    launch.startWidth = Length(Sub(swing.lastOuter, swing.firstOuter));
    launch.noHit = swing.done;
    launch.move = boss.move;
    launch.strike = swing.strike;
    BossWaves::SpawnSlash(frame, launch);
}

// 攻撃中：剣を振り抜く間に剣先の霞が斬撃になり、振り抜いた瞬間にそのまま飛ぶ（1 振り＝1 つの斬撃）。
void SwingStep(BossFrame& frame, const BossMove& move, const BossSegmentProgress& g) {
    BossComponent& boss = frame.boss;
    if (g.segment < 0 || static_cast<size_t>(g.segment) >= boss.swings.size())
        return;
    BossSwing& swing = boss.swings[static_cast<size_t>(g.segment)];
    if (!swing.active) {
        swing = {};
        swing.active = true;
        swing.segment = g.segment;
        swing.strike = g.hitIndex;
    }
    if (boss.segmentDone[static_cast<size_t>(g.segment)])
        return;
    const BossSwordPoints sword = BossPose::SwordPoints(boss, BossPose::Target(boss));
    if (swing.samples == 0)
        swing.firstOuter = sword.outer;
    swing.lastOuter = sword.outer;
    swing.heightSum += sword.outer.y - boss.ground.y;
    ++swing.samples;
    // 剣が描く霞（場の霧へ入れる。描画フレームで注入する）。
    boss.swordSmoke = true;
    boss.swordInner = sword.inner;
    boss.swordOuter = sword.outer;
    boss.swordSmokeRate = boss.swingSmoke / static_cast<float>(std::max(1, g.swingLength)) / frame.dt;
    // 振っている剣そのものの当たり：剣が描いている霞の弧と同じ一撃（当たる・抜けるは 1 回だけ）。
    if (!swing.done && BossWaves::Touches(frame, sword.base, sword.outer, boss.thickness)) {
        const BossStrikeData& strike = BossMoveTable::Strike(move, g.hitIndex);
        bool dodged = false;
        BossWaves::HitPlayer(frame, strike.damage, Sub(PlayerGround(frame), boss.ground), strike.blow, boss.move,
                             g.hitIndex, "sword", dodged);
        swing.done = true;
    }
    if (g.tIn >= g.swingLength) {
        boss.segmentDone[static_cast<size_t>(g.segment)] = true;
        Release(frame, swing);
    }
}

} // namespace

void BossMoveExecutor::ClampToArena(const BossComponent& boss, Vec3& point) {
    const Vec3 offset = Sub(point, boss.arenaCenter);
    const float distance = FlatLength(offset);
    if (distance > boss.arenaRadius && distance > 0.0f) {
        point.x = boss.arenaCenter.x + offset.x * boss.arenaRadius / distance;
        point.z = boss.arenaCenter.z + offset.z * boss.arenaRadius / distance;
    }
}

void BossMoveExecutor::FaceRate(BossFrame& frame, float degreesPerSecond) {
    BossComponent& boss = frame.boss;
    if (frame.player == nullptr)
        return;
    const Vec3 offset = Sub(PlayerGround(frame), boss.ground);
    if (FlatLength(offset) < 1e-4f)
        return;
    const float delta = WrapPi(YawOf(offset) - boss.yaw);
    const float limit = degreesPerSecond * kRadiansPerDegree * frame.dt;
    boss.yaw = WrapPi(boss.yaw + std::clamp(delta, -limit, limit));
}

void BossMoveExecutor::StepFloat(BossComponent& boss) {
    if (--boss.floatTimer <= 0) {
        boss.floatTarget = Lerp(boss.floatMin, std::max(boss.floatMin, boss.floatMax), BossRandom(boss));
        boss.floatTimer = kFloatRetargetMin + static_cast<int>(BossRandom(boss) * kFloatRetargetSpread);
    }
    const bool walking =
        boss.state == BossState::Act && boss.move >= 0 && BossMoveTable::IsMoveType(CurrentMove(boss).type);
    const bool moving = boss.state == BossState::Idle || walking;
    // 攻撃するときは低く降りて、剣がプレイヤーの高さを通るようにする。
    const bool attacking = (boss.state == BossState::Act && !walking && boss.phase != BossActPhase::Post) ||
                           boss.state == BossState::Stunned;
    const float target =
        attacking ? boss.floatMin : std::clamp(boss.floatTarget, boss.floatMin, std::max(boss.floatMin, boss.floatMax));
    boss.floatHeight += (target - boss.floatHeight) * (attacking ? kFloatAttackBlend
                                                       : moving  ? kFloatMoveBlend
                                                                 : kFloatIdleBlend);
    boss.floatHeight = std::clamp(boss.floatHeight, boss.floatMin, std::max(boss.floatMin, boss.floatMax));
}

void BossMoveExecutor::Start(BossFrame& frame, int moveIndex, int chain) {
    BossComponent& boss = frame.boss;
    const BossMove& move = boss.table.GetMoves()[static_cast<size_t>(moveIndex)];
    if (!BossMoveTable::IsMoveType(move.type))
        boss.velocity = {};
    for (auto& swing : boss.swings)
        swing = {};
    boss.state = BossState::Act;
    boss.phase = BossActPhase::Pre;
    boss.move = moveIndex;
    boss.stateFrame = 0;
    boss.chain = chain;
    boss.segmentDone.assign(move.segments.size(), false);
    boss.lastSegment = -1;
    boss.dashStarted = false;
    boss.glintFrames = 0;
    boss.spin = 0.0f;
    boss.hidden = false;
    boss.sideSign = BossRandom(boss) < 0.5f ? 1.0f : -1.0f;
    boss.actFrames = move.act;
    if (BossMoveTable::IsMoveType(move.type) && !move.segments.empty()) {
        // 移動の長さは出すたびにランダム。
        const BossSegment& segment = move.segments[0];
        boss.actFrames =
            std::max(1, static_cast<int>(std::floor(Lerp(static_cast<float>(segment.rangeMin),
                                                         static_cast<float>(segment.rangeMax), BossRandom(boss)) +
                                                    0.5f)));
        boss.strafe = BossRandom(boss) < 0.5f ? 1.0f : -1.0f;
        boss.stepFrom = boss.ground;
    }
    char extra[160]{};
    std::snprintf(extra, sizeof(extra), "type=%s pre=%d act=%d post=%d hits=%d", BossMoveTable::TypeKey(move.type),
                  move.pre, boss.actFrames, move.post, move.hitCount);
    Log(boss, "MoveStart", extra);
}

BossMoveExecutor::StepResult BossMoveExecutor::Step(BossFrame& frame) {
    BossComponent& boss = frame.boss;
    const BossMove& move = CurrentMove(boss);
    ++boss.stateFrame;
    const Vec3 offset = Sub(PlayerGround(frame), boss.ground);
    const float distance = FlatLength(offset);
    const Vec3 toPlayer = FlatNormalize(offset, Forward(boss.yaw));
    const bool swingType = BossMoveTable::IsSwingType(move.type);
    const int kamae = boss.kamaeFrames;
    StepResult result = StepResult::Running;
    if (boss.phase == BossActPhase::Pre) {
        const float progress = move.pre > 0 ? static_cast<float>(boss.stateFrame) / move.pre : 1.0f;
        if (swingType && move.pre - boss.stateFrame == std::min(kamae, move.pre - 1))
            Glint(boss);
        PreMotion(frame, move, progress, toPlayer, distance);
        SideMove(frame, move);
        if (boss.stateFrame >= move.pre) {
            boss.phase = BossActPhase::Act;
            boss.stateFrame = 0;
            Log(boss, "ActStart");
        }
    } else if (boss.phase == BossActPhase::Act) {
        SideMove(frame, move);
        const auto g = BossPose::Progress(boss, move);
        const bool entered = boss.lastSegment != g.segment;
        boss.lastSegment = g.segment;
        const BossSegment& segment = move.segments[static_cast<size_t>(g.segment)];
        if (!g.hit && swingType && static_cast<size_t>(g.segment + 1) < move.segments.size() &&
            move.segments[static_cast<size_t>(g.segment + 1)].hit &&
            segment.length - g.tIn == std::min(kamae, segment.length - 1))
            Glint(boss);
        if (g.hit) {
            const BossStrikeData& strike = BossMoveTable::Strike(move, g.hitIndex);
            if (!swingType) {
                if (!boss.segmentDone[static_cast<size_t>(g.segment)]) {
                    boss.segmentDone[static_cast<size_t>(g.segment)] = true;
                    if (move.type == BossMoveType::Knife) {
                        const Vec3 forward = Forward(boss.yaw);
                        Vec3 from = Add(boss.ground, Mul(forward, boss.knifeForward));
                        from.y = boss.ground.y + BossPose::RootHeight(boss) + boss.knifeHeight;
                        BossWaves::SpawnKnives(frame, boss.move, from, AimDirection(frame, from));
                    } else {
                        BossSpecialMoves::Trigger(frame, move);
                    }
                }
            } else {
                // 追尾するのはボスの振り（向き）。斬撃そのものは追尾しない。
                if (g.tIn <= g.swingLength && strike.homing > 0.0f)
                    FaceRate(frame, strike.homing);
                StepIn(frame, strike, g);
                SwingStep(frame, move, g);
            }
        }
        const float progress = static_cast<float>(boss.stateFrame) / static_cast<float>(std::max(1, boss.actFrames));
        ActMotion(frame, move, progress);
        BossSpecialMoves::Act(frame, move, g, entered, progress);
        if (boss.stateFrame >= boss.actFrames) {
            Log(boss, "ActEnd");
            result = StepResult::ActFinished;
        }
    } else {
        if (move.type == BossMoveType::Backstep)
            boss.height = 0.0f;
        BossSpecialMoves::Post(boss, move,
                               static_cast<float>(boss.stateFrame) / static_cast<float>(std::max(1, move.post)));
        if (boss.stateFrame >= move.post) {
            Log(boss, "MoveEnd");
            result = StepResult::Finished;
        }
    }
    ClampToArena(boss, boss.ground);
    return result;
}

void BossMoveExecutor::EnterPost(BossComponent& boss) {
    boss.phase = BossActPhase::Post;
    boss.stateFrame = 0;
}

void BossMoveExecutor::EnterIdle(BossComponent& boss) {
    boss.state = BossState::Idle;
    boss.move = -1;
    boss.stateFrame = 0;
    boss.idleFrames = 0;
    boss.chain = 0;
    boss.sink = 0.0f;
    boss.spin = 0.0f;
    boss.hidden = false;
    boss.strafe = BossRandom(boss) < 0.5f ? 1.0f : -1.0f;
}

void BossMoveExecutor::StepIdle(BossFrame& frame) {
    BossComponent& boss = frame.boss;
    ++boss.idleFrames;
    boss.crouch = Lerp(boss.crouch, 0.0f, kIdleCrouchBlend);
    boss.height = Lerp(boss.height, 0.0f, kIdleHeightBlend);
    FaceRate(frame, boss.idleTurnRate);
    Vec3 want{};
    if (!boss.testHoldStill && frame.player != nullptr) {
        // 次の行動までの間は、プレイヤーを見たまま横へ歩く（途中でたまに向きを変える）。
        if (boss.idleFrames > kStrafeFlipStart && boss.idleFrames % kStrafeFlipEvery == 0 &&
            BossRandom(boss) < kStrafeFlipChance)
            boss.strafe = -boss.strafe;
        const Vec3 toPlayer = FlatNormalize(Sub(PlayerGround(frame), boss.ground));
        want = Mul(Vec3{toPlayer.z, 0.0f, -toPlayer.x}, boss.strafe * boss.walkSpeed);
    }
    boss.velocity = Lerp(boss.velocity, want, kVelocityBlend);
    boss.ground = Add(boss.ground, Mul(boss.velocity, frame.dt));
    ClampToArena(boss, boss.ground);
}

void BossMoveExecutor::Interrupt(BossComponent& boss) {
    for (auto& swing : boss.swings)
        swing = {};
    boss.segmentDone.clear();
    boss.move = -1;
    boss.chain = 0;
    boss.dashStarted = false;
    boss.height = 0.0f;
    boss.sink = 0.0f;
    boss.spin = 0.0f;
    boss.hidden = false;
    boss.swordSmoke = false;
    boss.glintFrames = 0;
    boss.velocity = {};
}

} // namespace Project_TD2_2
