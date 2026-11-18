#include "BossMoveExecutor.h"

#include <algorithm>
#include <cmath>

#include "BossBurnMarks.h"
#include "BossFlameRun.h"
#include "BossGreatsword.h"
#include "BossKasumiStep.h"
#include "BossMath.h"
#include "BossPose.h"
#include "BossSelection.h"
#include "BossSkyfall.h"
#include "BossSpecialMoves.h"
#include "BossWaves.h"

namespace Project_TD2_2 {
namespace {

// 剣の当たりを、前の tick の剣からいまの剣までの間で何か所に分けて確かめるか（速い振りが体を飛び越えないように）。
constexpr int kSwordSweepSteps = 4;

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
constexpr float kDashCrouchSpeed = 1.5f;
// 上段：前隙の最後の 15% で踏み込む。
constexpr float kOverheadLungeStart = 0.85f;

const BossMove& CurrentMove(const BossComponent& boss) { return boss.table.GetMoves()[static_cast<size_t>(boss.move)]; }

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
    // 追尾を止めている間は、止めた時のプレイヤーの位置へ放つ（高さの狙いも動かさない）。
    const Vec3 target = frame.boss.aimFrozen ? frame.boss.aimPoint : frame.playerCenter;
    const float horizontal = std::max(1.0f, FlatLength(Sub(target, from)));
    const Vec3 aim{forward.x * horizontal, target.y + frame.boss.playerAimHeight - from.y, forward.z * horizontal};
    return Normalize(aim, forward);
}

// 溜めの揺らぎで構えたまま止まっている tick（前隙の windupHoldAt F 目で、まだ止まる F が残っている）。
bool Holding(const BossComponent& boss) {
    return boss.phase == BossActPhase::Pre && boss.windupHold > 0 && boss.stateFrame == boss.windupHoldAt;
}

// いまの tick から、次の一撃を放つ tick まで何 tick あるか（放つ tick は 0）。もう放たない時は -1。
// 剣を振る型は振り抜いた tick（SwingStep の Release）、ほかの型は打つ区間に入った tick（ナイフ・輪・分身）に放つ。
int FramesToRelease(const BossComponent& boss, const BossMove& move) {
    const bool swingType = BossMoveTable::IsSwingType(move.type);
    int accumulated = 0;
    int base = 0, now = boss.stateFrame;
    if (boss.phase == BossActPhase::Pre) {
        // 攻撃中の 0 F 目は前隙が終わる tick。残りの前隙と、まだ止まる溜めの分だけ先になる。
        base = move.pre - boss.stateFrame + (boss.stateFrame <= boss.windupHoldAt ? boss.windupHold : 0);
        now = 0;
    } else if (boss.phase != BossActPhase::Act) {
        return -1;
    }
    for (size_t k = 0; k < move.segments.size(); ++k) {
        const BossSegment& segment = move.segments[k];
        const int length =
            BossMoveTable::IsMoveType(move.type) && k == 0 ? std::max(1, boss.actFrames) : segment.length;
        if (segment.hit) {
            const int release =
                accumulated + (swingType ? std::max(1, std::min(length, boss.swingInFrames + boss.swingFrames)) : 1);
            if (release >= now)
                return base + release - now;
        }
        accumulated += length;
    }
    return -1;
}

// 追尾を止める F：次の一撃を放つまでが表の F より短くなったら、向き直るのをやめ、狙いの点も止める。放ったら戻す。
void UpdateAimFreeze(BossFrame& frame, const BossMove& move) {
    BossComponent& boss = frame.boss;
    const int remaining = move.trackStopFrames > 0 ? FramesToRelease(boss, move) : -1;
    const bool freeze = remaining >= 0 && remaining < move.trackStopFrames;
    if (freeze == boss.aimFrozen)
        return;
    boss.aimFrozen = freeze;
    if (!freeze)
        return;
    boss.aimPoint = frame.player != nullptr ? frame.playerCenter : Add(boss.ground, Forward(boss.yaw));
    char extra[160]{};
    std::snprintf(extra, sizeof(extra), "release=%d yaw=%.4f aim=(%.3f,%.3f,%.3f)", remaining,
                  static_cast<double>(boss.yaw), static_cast<double>(boss.aimPoint.x),
                  static_cast<double>(boss.aimPoint.y), static_cast<double>(boss.aimPoint.z));
    Log(boss, "AimFreeze", extra);
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
    // 溜めの揺らぎで止まっている間も、構えと同じく歩かない。
    const bool inKamae = move.pre - t < std::min(boss.kamaeFrames, move.pre) || Holding(boss);
    // 前隙の間はずっとプレイヤーを向く（追尾を止めている間は FaceRate が向かない）。
    // 引爆は狙わない（足元から全方向へ爆ぜる）ので、溜めの間は立ったまま向きも変えない。
    if (move.type != BossMoveType::Detonate)
        BossMoveExecutor::FaceRate(frame,
                                   move.type == BossMoveType::Overhead ? boss.overheadTurnRate : boss.preTurnRate);
    const Vec3 forward = Forward(boss.yaw);
    switch (move.type) {
    case BossMoveType::Dash: {
        boss.crouch = std::min(1.0f, progress * kDashCrouchSpeed);
        // 構えてから、前隙の終わりの dashSlideFrames F で低く滑って詰める（c-2dd9a2。前は構えの前に 12 F で瞬間移動して
        // いた）。速さは始めと終わりで 0 の滑らかな曲線。行き先はプレイヤーの手前で、もう手前より近ければ動かない
        // （プレイヤーの背後へは回らない）。
        const int slide = std::clamp(boss.dashSlideFrames, 1, std::max(1, move.pre - 1));
        const int start = move.pre - slide;
        if (t >= start && frame.player != nullptr) {
            if (!boss.dashStarted) {
                boss.dashStarted = true;
                boss.dashFrom = boss.ground;
                boss.dashTo = distance > boss.dashStopDistance
                                  ? Sub(PlayerGround(frame), Mul(toPlayer, boss.dashStopDistance))
                                  : boss.ground;
                BossMoveExecutor::ClampToArena(boss, boss.dashTo);
                char extra[96]{};
                std::snprintf(extra, sizeof(extra), "distance=%.3f frames=%d",
                              static_cast<double>(FlatLength(Sub(boss.dashTo, boss.dashFrom))), slide);
                Log(boss, "DashSlide", extra);
            }
            const float k = Share(t - start, slide);
            boss.ground = Lerp(boss.dashFrom, boss.dashTo, Smooth(k));
            if (k >= 1.0f && !boss.dashLanded) {
                boss.dashLanded = true;
                BossBurnMarks::DashLanded(frame, move);
            }
            if (!boss.aimFrozen)
                boss.yaw = YawOf(Sub(PlayerGround(frame), boss.ground));
        }
        break;
    }
    case BossMoveType::SlashR:
    case BossMoveType::SlashL:
    case BossMoveType::SlashH:
    case BossMoveType::SlashHR:
    case BossMoveType::SlashV:
        // 空中で終わった技（跳躍）から続いた時は、始めの comboLandFrames F で落ちて着地する（だんだん速く）。
        if (boss.landFrom > 0.0f) {
            const float k = Share(t, std::max(1, boss.comboLandFrames));
            boss.height = boss.landFrom * (1.0f - k * k);
        }
        if (move.preShift != 0.0f) {
            // 表の「前隙の移動 m」：構えに入るまで（振り上げの間）に、滑らかに前へ・後ろへ動く。
            const int span = std::max(1, move.pre - std::min(boss.kamaeFrames, move.pre - 1));
            if (t <= span) {
                const float d = move.preShift * (Smooth(Share(t, span)) - Smooth(Share(t - 1, span)));
                boss.ground = Add(boss.ground, Mul(forward, d));
                BossMoveExecutor::ClampToArena(boss, boss.ground);
            }
        } else if (!inKamae)
            boss.ground = Add(boss.ground, Mul(forward, boss.slashApproachSpeed * frame.dt));
        break;
    case BossMoveType::Overhead: {
        // 跳び込み（c-b1824c）：前隙の終わりの overheadLeapFrames F で、プレイヤーから overheadLeapStop m の所へ（最長
        // overheadLeapMax m。近すぎれば後ろへ最長 overheadLeapBack m）滑らかに弧を描いて跳び、前隙の終わりに着地する（そこから
        // 斬り下ろす）。行き先は跳ぶ時に決める（突進と同じく dashFrom・dashTo に覚える）。跳ばない時（0 F）は前の作り
        // （終わりの 15 % で踏み込む）。
        const int leap = std::clamp(boss.overheadLeapFrames, 0, std::max(0, move.pre - 1));
        if (leap > 0) {
            const int start = move.pre - leap;
            if (t == start + 1) {
                boss.dashFrom = boss.ground;
                const float length =
                    std::clamp(distance - boss.overheadLeapStop, -boss.overheadLeapBack, boss.overheadLeapMax);
                boss.dashTo = Add(boss.ground, Mul(forward, length));
                BossMoveExecutor::ClampToArena(boss, boss.dashTo);
                char extra[64]{};
                std::snprintf(extra, sizeof(extra), "distance=%.3f", static_cast<double>(length));
                Log(boss, "OverheadLeap", extra);
            }
            if (t > start) {
                const float k = Share(t - start, leap);
                boss.ground = Lerp(boss.dashFrom, boss.dashTo, Smooth(k));
                boss.height = boss.overheadLeapHeight * std::sin(kPi * k);
            }
        } else if (progress > kOverheadLungeStart) {
            boss.ground = Add(boss.ground, Mul(forward, boss.overheadLungeDistance /
                                                            std::max(1.0f, move.pre * (1.0f - kOverheadLungeStart))));
        }
        break;
    }
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
            BossMoveExecutor::StepBack(frame, progress);
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
void Release(BossFrame& frame, BossSwing& swing) {
    BossComponent& boss = frame.boss;
    if (swing.samples < 2 || swing.released)
        return;
    swing.released = true;
    // 当て方が「実」の技・斬撃速度が 0 の一撃は、斬撃を飛ばさない（実体の刀だけで当てる。c-2dd9a2）。
    if (CurrentMove(boss).delivery == BossDelivery::Blade ||
        !(BossMoveTable::Strike(CurrentMove(boss), swing.strike).speed > 0.0f))
        return;
    const Vec3 forward = Forward(boss.yaw);
    const float minimumAhead = boss.bodyRadius + boss.waveForward;
    // 刀に合わせる：刀が正面を斬った部分の記録がある時（刀の線が面を掃いている）だけ。無ければ前の作りで放つ。
    const bool fromBlade = boss.waveFromBlade && swing.cutStarted && swing.reach > -1.0e8f &&
                           Dot(swing.cutNormal, swing.cutNormal) > 1e-6f;
    Vec3 from{};
    if (fromBlade) {
        // 剣先が前へ一番出た時の、刃の上の点。体の中からは出さない。
        from = Lerp(swing.reachBase, swing.reachOuter, std::clamp(boss.waveBladeShare, 0.0f, 1.0f));
        const Vec3 offset = Sub(from, boss.ground);
        const float ahead = offset.x * forward.x + offset.z * forward.z;
        if (ahead < minimumAhead)
            from = Add(from, Mul(forward, minimumAhead - ahead));
        from.y =
            boss.ground.y + std::clamp(from.y - boss.ground.y, boss.waveMinHeight, boss.waveMaxHeight + boss.height);
    } else {
        const float averageHeight = swing.heightSum / static_cast<float>(swing.samples);
        from = Add(boss.ground, Mul(forward, minimumAhead));
        from.y = boss.ground.y + std::clamp(averageHeight, boss.waveMinHeight, boss.waveMaxHeight + boss.height);
    }
    const Vec3 direction = AimDirection(frame, from);
    // 三日月の幅の向き：刀の線が掃いた面の中で、飛ぶ向きに直交する向き（剣先が動いた側を正にする）。
    // 合わせない時は、剣先の最初と最後を結ぶ線を、飛ぶ向きに直交させる。
    const Vec3 travel = fromBlade ? Sub(swing.reachOuter, swing.cutFirstOuter) : Sub(swing.lastOuter, swing.firstOuter);
    const Vec3 chord = Sub(swing.lastOuter, swing.firstOuter);
    Vec3 across = Sub(chord, Mul(direction, Dot(chord, direction)));
    if (fromBlade) {
        Vec3 plane = Normalize(Cross(swing.cutNormal, direction), across);
        if (Dot(plane, travel) < 0.0f)
            plane = Mul(plane, -1.0f);
        // wavePlaneMatch で、前の作りの向き（剣先の最初と最後を結ぶ線）から、刀が斬った面の向きへ寄せる。
        Vec3 loose = Normalize(across, plane);
        if (Dot(loose, plane) < 0.0f)
            loose = Mul(loose, -1.0f);
        across = Lerp(loose, plane, std::clamp(boss.wavePlaneMatch, 0.0f, 1.0f));
    }
    if (Dot(across, across) < 1e-4f)
        across = Cross(direction, {0.0f, 1.0f, 0.0f});
    BossSlashLaunch launch;
    launch.from = from;
    launch.direction = direction;
    launch.across = Normalize(across, {1.0f, 0.0f, 0.0f});
    launch.startWidth = Length(travel);
    launch.noHit = swing.done;
    launch.move = boss.move;
    launch.strike = swing.strike;
    if (boss.traceEveryFrames > 0) {
        // 振った剣の弧（剣先の最初と最後）と、斬撃を放つ所のずれを確かめる記録。
        char message[384]{};
        std::snprintf(
            message, sizeof(message),
            "SlashArc tick=%llu strike=%d samples=%d fromBlade=%d firstOuter=(%.3f,%.3f,%.3f) "
            "lastOuter=(%.3f,%.3f,%.3f) "
            "lastBase=(%.3f,%.3f,%.3f) from=(%.3f,%.3f,%.3f) ground=(%.3f,%.3f,%.3f) forward=(%.3f,%.3f)",
            static_cast<unsigned long long>(boss.tick), swing.strike, swing.samples, static_cast<int>(fromBlade),
            static_cast<double>(swing.firstOuter.x), static_cast<double>(swing.firstOuter.y),
            static_cast<double>(swing.firstOuter.z), static_cast<double>(swing.lastOuter.x),
            static_cast<double>(swing.lastOuter.y), static_cast<double>(swing.lastOuter.z),
            static_cast<double>(swing.lastBase.x), static_cast<double>(swing.lastBase.y),
            static_cast<double>(swing.lastBase.z), static_cast<double>(from.x), static_cast<double>(from.y),
            static_cast<double>(from.z), static_cast<double>(boss.ground.x), static_cast<double>(boss.ground.y),
            static_cast<double>(boss.ground.z), static_cast<double>(forward.x), static_cast<double>(forward.z));
        BossLog::Write(message);
    }
    swing.waveId = BossWaves::SpawnSlash(frame, launch);
    boss.releaseTick = boss.tick;
    ++boss.releaseCount;
    BossBurnMarks::Released(frame, CurrentMove(boss), swing.strike);
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
    // tick の途中の向き：攻撃中の最後の tick は、この後で後隙・次の技へ状態が変わる。tick の終わりに求める swordPose は
    // 変わった後の向きなので、当たりには使えない。
    const BossSwordPoints sword = BossPose::SwordPoints(boss, BossPose::Target(boss));
    const bool first = swing.samples == 0;
    const Vec3 previousBase = first ? sword.base : swing.lastBase;
    const Vec3 previousOuter = first ? sword.outer : swing.lastOuter;
    if (first)
        swing.firstOuter = sword.outer;
    swing.lastOuter = sword.outer;
    swing.lastBase = sword.base;
    swing.heightSum += sword.outer.y - boss.ground.y;
    ++swing.samples;
    // 刀が正面を斬る部分：刀の線が掃いた面と、剣先がボスの前へ一番出た所を覚える。そこを過ぎた tick に斬撃を放つ。
    const Vec3 blade = Normalize(Sub(sword.outer, sword.base), Forward(boss.yaw));
    bool passedFront = false;
    if (static_cast<float>(g.tIn) >= static_cast<float>(g.swingLength) * boss.waveCutShare) {
        if (!swing.cutStarted) {
            swing.cutStarted = true;
            swing.cutFirstOuter = previousOuter;
        }
        if (!first)
            swing.cutNormal = Add(swing.cutNormal, Cross(swing.lastBlade, blade));
        const Vec3 ahead = Forward(boss.yaw);
        const Vec3 tip = Sub(sword.outer, boss.ground);
        const float reach = tip.x * ahead.x + tip.z * ahead.z;
        if (reach > swing.reach) {
            swing.reach = reach;
            swing.reachBase = sword.base;
            swing.reachOuter = sword.outer;
        } else {
            passedFront = true;
        }
    }
    swing.lastBlade = blade;
    // 剣が描く霞（場の霧へ入れる。描画フレームで注入する）。
    boss.swordSmoke = true;
    boss.swordInner = sword.inner;
    boss.swordOuter = sword.outer;
    boss.swordSmokeRate = boss.swingSmoke / static_cast<float>(std::max(1, g.swingLength)) / frame.dt;
    // 振っている剣そのものの当たり：剣が描いている霞の弧と同じ一撃（当たる・抜けるは 1 回だけ）。速い振りは 1 tick に
    // 40 度ほど回り、プレイヤーの体を tick の間で飛び越えるので、前の tick の剣からいまの剣までを分けて確かめる。
    // 当て方が「霞」の技は、刀そのものでは当てない（飛ぶ斬撃だけ。c-2dd9a2）。
    bool touched = false;
    for (int k = 1; k <= kSwordSweepSteps && !swing.done && !touched && move.delivery != BossDelivery::Haze; ++k) {
        const float u = static_cast<float>(k) / static_cast<float>(kSwordSweepSteps);
        touched = BossWaves::Touches(frame, Lerp(previousBase, sword.base, u), Lerp(previousOuter, sword.outer, u),
                                     boss.thickness);
    }
    if (!swing.done && touched) {
        const BossStrikeData& strike = BossMoveTable::Strike(move, g.hitIndex);
        BossWaves::HitPlayer(frame, strike.damage, Sub(PlayerGround(frame), boss.ground), strike.blow, boss.move,
                             g.hitIndex, "sword", PlayerDamageType::Slash, boss.bladeLit);
        swing.done = true;
        // 同じ一撃の斬撃をもう放っていたら、そちらは当たらなくする。
        BossWaves::SpendWave(boss, swing.waveId);
    }
    // 刀に合わせて放つ：剣先が前へ一番出た所を過ぎた tick。振り抜くまでに放っていなければ、振り抜いた tick に放つ。
    if (boss.waveFromBlade && passedFront)
        Release(frame, swing);
    if (g.tIn >= g.swingLength) {
        boss.segmentDone[static_cast<size_t>(g.segment)] = true;
        Release(frame, swing);
    }
}

} // namespace

void BossMoveExecutor::StepBack(BossFrame& frame, float progress) {
    BossComponent& boss = frame.boss;
    const Vec3 toPlayer = FlatNormalize(Sub(PlayerGround(frame), boss.ground));
    const float u = std::clamp(progress, 0.0f, 1.0f);
    boss.ground = Add(boss.stepFrom, Mul(toPlayer, -boss.backstepDistance * EaseOut(u)));
    boss.height = boss.backstepHeight * std::sin(u * kPi);
}

void BossMoveExecutor::ClampToArena(const BossComponent& boss, Vec3& point) {
    const Vec3 offset = Sub(point, boss.arenaCenter);
    const float distance = FlatLength(offset);
    if (distance > boss.arenaRadius && distance > 0.0f) {
        point.x = boss.arenaCenter.x + offset.x * boss.arenaRadius / distance;
        point.z = boss.arenaCenter.z + offset.z * boss.arenaRadius / distance;
    }
}

bool BossMoveExecutor::IsWalking(const BossComponent& boss) {
    if (boss.state != BossState::Act || boss.phase != BossActPhase::Act || boss.move < 0 ||
        static_cast<size_t>(boss.move) >= boss.table.GetMoves().size())
        return false;
    const BossMoveType type = CurrentMove(boss).type;
    return type == BossMoveType::WalkSide || type == BossMoveType::WalkIn;
}

void BossMoveExecutor::FaceRate(BossFrame& frame, float degreesPerSecond) {
    BossComponent& boss = frame.boss;
    // 追尾を止めている間は向き直らない（どの型の、前隙・攻撃中のどの向き直りも）。
    if (frame.player == nullptr || boss.aimFrozen)
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
    // 前隙の構えは、いま見えている剣（前の tick の向き）から始める（コンボ・後隙の切り上げでも途切れない）。
    boss.moveStartPose = boss.swordPose;
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
    boss.dashLanded = false;
    boss.glintFrames = 0;
    boss.spin = 0.0f;
    boss.hidden = false;
    boss.landFrom = boss.height;
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
    }
    // 炎走りは、放つ数（フェーズで変わる）の分だけ攻撃中が延びる。
    BossFlameRun::Begin(boss, move);
    // 後ろへ跳ぶ型（後ろへ下がる・退き溜め）の跳び始め。
    boss.stepFrom = boss.ground;
    boss.retreat = {};
    boss.aimFrozen = false;
    boss.windupHits = 0;
    boss.windupHold = 0;
    boss.switchRolled = false;
    // 構えきる F（構えの合図の F）。前隙が短すぎて止まれない技は揺らがない。突進横なぎは滑り出す前（滑る途中で止まらない）。
    boss.windupHoldAt = move.pre - std::min(boss.kamaeFrames, move.pre - 1);
    if (move.type == BossMoveType::Dash)
        boss.windupHoldAt = std::max(1, move.pre - std::clamp(boss.dashSlideFrames, 1, std::max(1, move.pre - 1)) - 1);
    // 上段溜めは跳び込む前に溜めを揺らがせる（跳ぶ途中で止まらない）。
    if (move.type == BossMoveType::Overhead && boss.overheadLeapFrames > 0)
        boss.windupHoldAt =
            std::max(1, move.pre - std::clamp(boss.overheadLeapFrames, 1, std::max(1, move.pre - 1)) - 1);
    if (!move.windupExtras.empty() && move.pre >= 2) {
        // 見た目の乱数で選ぶ。技選びの乱数や、プレイヤーに見える合図とは結び付かない。
        // 始めた tick を混ぜる：見た目の乱数は種が決まっているので、混ぜないと Play のたびに最初の溜めが同じ長さになる。
        boss.random ^= static_cast<uint32_t>(boss.tick) * 2654435761u;
        const size_t count = move.windupExtras.size();
        const size_t pick = std::min(count - 1, static_cast<size_t>(BossRandom(boss) * static_cast<float>(count)));
        boss.windupHold = std::max(0, move.windupExtras[pick]);
        char message[192]{};
        std::snprintf(message, sizeof(message), "Windup tick=%llu move=%s extra=%d pick=%zu/%zu",
                      static_cast<unsigned long long>(boss.tick), move.name.c_str(), boss.windupHold, pick + 1, count);
        BossLog::Write(message);
    }
    char extra[192]{};
    std::snprintf(extra, sizeof(extra), "type=%s pre=%d extra=%d act=%d post=%d hits=%d",
                  BossMoveTable::TypeKey(move.type), move.pre, boss.windupHold, boss.actFrames, move.post,
                  move.hitCount);
    Log(boss, "MoveStart", extra);
    BossBurnMarks::LightBlade(boss, move);
    BossSkyfall::Begin(frame, move);
    BossGreatsword::Begin(frame, move);
    BossKasumiStep::Begin(frame, move);
}

BossMoveExecutor::StepResult BossMoveExecutor::Step(BossFrame& frame) {
    BossComponent& boss = frame.boss;
    const BossMove& move = CurrentMove(boss);
    // 溜めの揺らぎの間は前隙の F を進めない：体・剣の構え・踏み込みの時間がそこで止まり、構えたまま待つ。
    if (Holding(boss))
        --boss.windupHold;
    else
        ++boss.stateFrame;
    UpdateAimFreeze(frame, move);
    // 縦割りの噴き上がりは、叩きつけた技の後隙や、コンボでつないだ次の技の前隙の間も走る。
    BossGreatsword::StepEruption(frame);
    const Vec3 offset = Sub(PlayerGround(frame), boss.ground);
    const float distance = FlatLength(offset);
    const Vec3 toPlayer = FlatNormalize(offset, Forward(boss.yaw));
    const bool swingType = BossMoveTable::IsSwingType(move.type);
    const int kamae = boss.kamaeFrames;
    StepResult result = StepResult::Running;
    // 構えの途中の変化：構えきる F に 1 回だけ確率を振り、当たれば変化先（前隙の短い行）へ切り替える。いま見えている構え
    // （振りかぶった所）から、変化先の振り上げ・構え・振りが続く。見た目の乱数（技選びの乱数とは別）。
    // 確率は表の「変化確率%」（咎められた技を変化しやすくする項目が入なら、その分を足す：BossSelection::SwitchChance）。
    if (boss.phase == BossActPhase::Pre && !boss.switchRolled && move.switchMove >= 0 &&
        boss.stateFrame >= boss.windupHoldAt) {
        boss.switchRolled = true;
        const float chance = BossSelection::SwitchChance(boss, boss.move);
        if (BossRandom(boss) * 100.0f < chance) {
            char extra[160]{};
            std::snprintf(extra, sizeof(extra), "to=%s chance=%.0f",
                          boss.table.GetMoves()[static_cast<size_t>(move.switchMove)].name.c_str(),
                          static_cast<double>(chance));
            Log(boss, "Switch", extra);
            Start(frame, move.switchMove, boss.chain);
            return StepResult::Running;
        }
    }
    if (boss.phase == BossActPhase::Pre) {
        const float progress = move.pre > 0 ? static_cast<float>(boss.stateFrame) / move.pre : 1.0f;
        // 構え（剣先に霞が集まる）は、溜めの揺らぎで止まった後、振るまでいつもの長さの所で出す。
        if (swingType && move.pre - boss.stateFrame == std::min(kamae, move.pre - 1) && boss.windupHold == 0)
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
                    BossBurnMarks::Released(frame, move, g.hitIndex);
                }
            } else {
                // 追尾するのはボスの振り（向き）。斬撃そのものは追尾しない。
                if (g.tIn <= g.swingLength && strike.homing > 0.0f)
                    FaceRate(frame, strike.homing);
                StepIn(frame, strike, g);
                // 体の回転（表の「体の回転 °」）：振りの速さで体ごと回す。当たりを見る前に回す（刀の当たりも回る）。
                if (move.actSpin != 0.0f && g.hitIndex == 0)
                    boss.spin = move.actSpin * BossPose::SwingShare(boss, g.tIn, g.swingLength);
                SwingStep(frame, move, g);
            }
        }
        const float progress = static_cast<float>(boss.stateFrame) / static_cast<float>(std::max(1, boss.actFrames));
        ActMotion(frame, move, progress);
        BossSpecialMoves::Act(frame, move, g, entered, progress);
        if (boss.stateFrame >= boss.actFrames) {
            // 回りきった体の回転は向きへ移す（後隙・次の技で回転を 0 に戻しても向きが跳ばない）。
            if (move.actSpin != 0.0f) {
                boss.yaw = WrapPi(boss.yaw + boss.spin);
                boss.spin = 0.0f;
            }
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
    boss.lastEndPose = boss.swordPose;
    boss.phase = BossActPhase::Post;
    boss.stateFrame = 0;
    boss.aimFrozen = false;
}

void BossMoveExecutor::EnterIdle(BossComponent& boss) {
    boss.lastEndPose = boss.swordPose;
    boss.state = BossState::Idle;
    boss.bladeLit = false;
    boss.aimFrozen = false;
    boss.windupHold = 0;
    BossSkyfall::Reset(boss, "end");
    BossGreatsword::Reset(boss, "end");
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
    // 打ち切った時の向きから、気絶の構えへ移る（気絶・撃破で 1 tick に跳ばない）。
    boss.lastEndPose = boss.swordPose;
    for (auto& swing : boss.swings)
        swing = {};
    boss.segmentDone.clear();
    boss.move = -1;
    boss.chain = 0;
    boss.aimFrozen = false;
    boss.windupHold = 0;
    boss.dashStarted = false;
    boss.dashLanded = false;
    boss.bladeLit = false;
    BossSpecialMoves::EndRetreatCharge(boss, "broken");
    // 空中（天降り斬り・跳躍）で打ち切られたら、その場で地面へ戻す。様子のログは落とす前の高さを残す。
    BossSkyfall::Reset(boss, "interrupted");
    BossSkyfall::CutBurst(boss);
    BossGreatsword::Reset(boss, "interrupted");
    BossFlameRun::Cut(boss);
    boss.height = 0.0f;
    boss.sink = 0.0f;
    // 回っている途中で打ち切られたら、その向きのまま止める（見える体が回転の分だけ跳ばない）。
    boss.yaw = WrapPi(boss.yaw + boss.spin);
    boss.spin = 0.0f;
    boss.hidden = false;
    boss.swordSmoke = false;
    boss.glintFrames = 0;
    boss.velocity = {};
}

} // namespace Project_TD2_2
