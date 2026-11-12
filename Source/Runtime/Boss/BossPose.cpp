#include "BossPose.h"

#include <algorithm>
#include <cmath>
#include <vector>

#include "BossMath.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 剣の姿勢（度）。試作の POSE と同じ値。剣のモデルを入れるまでは、剣の当たりと霞の位置だけに効く。
constexpr BossSwordPose kRest{70.0f, -10.0f};
constexpr BossSwordPose kUp{-110.0f, 0.0f};
constexpr BossSwordPose kUpLeft{-55.0f, 45.0f};
constexpr BossSwordPose kUpRight{-55.0f, -45.0f};
constexpr BossSwordPose kDownLeft{55.0f, 50.0f};
constexpr BossSwordPose kDownRight{55.0f, -50.0f};
constexpr BossSwordPose kBack{30.0f, -150.0f};
constexpr BossSwordPose kSide{30.0f, 85.0f};
constexpr BossSwordPose kVerticalEnd{80.0f, 0.0f};
constexpr BossSwordPose kStunned{105.0f, -25.0f};
constexpr BossSwordPose kKnifeStart{-30.0f, -120.0f};
constexpr BossSwordPose kKnifeEnd{30.0f, 40.0f};
// 回し蹴りの間、剣を横に構えたまま回る。分身を出す間は剣をまっすぐ前へ（試作の held と [0, 0]）。
constexpr BossSwordPose kHeld{20.0f, -80.0f};
constexpr BossSwordPose kForward{0.0f, 0.0f};
// 振り上げた構えの引き（反対へ 22% 引く）、振り出しの割合、フェイントで引き戻す長さ (F)。
constexpr float kCockedPull = 0.22f;
constexpr float kSwingInShare = 0.16f;
constexpr int kFeintPullbackFrames = 7;
// 剣の根元（剣の長さに対する割合）と、霞の内側の始まり。
constexpr float kSwordBaseShare = 0.14f;
constexpr float kSwordInnerShare = 0.65f;

BossSwordPose Sweep(BossSwordPose a, BossSwordPose c, float k) {
    const float e = Smooth(k);
    return {Lerp(a.x, c.x, e), Lerp(a.y, c.y, e)};
}

// 振り上げた構え：体の後ろへは回さず、プレイヤーから剣が見える高さ・横に掲げる。
BossSwordPose Cocked(BossSwordPose a, BossSwordPose c) {
    float x = a.x + (a.x - c.x) * kCockedPull;
    const float y = a.y + (a.y - c.y) * kCockedPull;
    if (std::abs(a.y) > 60.0f)
        x = std::min(x, a.x - 25.0f);
    return {std::clamp(x, -100.0f, 60.0f), std::clamp(y, -105.0f, 105.0f)};
}

// 振り：最初の振り出しはゆっくり動き始め（振り出しが見える）、残りで一気に振り抜く。
BossSwordPose Swing(const BossComponent& boss, BossSwordPose start, BossSwordPose end, int tIn, int length) {
    const BossSwordPose cocked = Cocked(start, end);
    const int swingIn = std::min(boss.swingInFrames, std::max(0, length - 2));
    float e = 0.0f;
    if (tIn <= swingIn) {
        const float u = static_cast<float>(tIn) / static_cast<float>(std::max(1, swingIn));
        e = kSwingInShare * u * u;
    } else {
        const float u = std::clamp(
            static_cast<float>(tIn - swingIn) / static_cast<float>(std::max(1, length - swingIn)), 0.0f, 1.0f);
        e = std::min(1.0f, kSwingInShare + (1.0f - kSwingInShare) *
                                               (2.0f * kSwingInShare * u + (1.0f - 2.0f * kSwingInShare) * u * u));
    }
    return {Lerp(cocked.x, end.x, e), Lerp(cocked.y, end.y, e)};
}

// 振り上げ→構え。剣を振る方向と反対へ大きく引いて、構えの間は止める。
// フェイント（遅）は、いつものテンポで「振り出し」まで見せてから引き戻し、溜めて（小さく震えて）待つ。
BossSwordPose Windup(const BossComponent& boss, BossSwordPose previous, BossSwordPose start, BossSwordPose end, int tIn,
                     int length, bool feint) {
    const BossSwordPose cocked = Cocked(start, end);
    const int base = feint ? std::max(1, length - boss.feintFrames) : length;
    const int kamae = std::min(boss.kamaeFrames, base - 1);
    const int raiseEnd = std::max(1, base - kamae);
    const int raiseStart = std::max(0, raiseEnd - boss.raiseFrames);
    if (tIn < base || !feint) {
        if (tIn < raiseStart)
            return previous;
        if (tIn < raiseEnd)
            return Sweep(previous, cocked,
                         static_cast<float>(tIn - raiseStart) / static_cast<float>(std::max(1, raiseEnd - raiseStart)));
        return cocked;
    }
    const int t = tIn - base;
    const int swingIn = boss.swingInFrames;
    const BossSwordPose fake = Swing(boss, start, end, swingIn, swingIn + boss.swingFrames);
    if (t <= swingIn)
        return Swing(boss, start, end, t, swingIn + boss.swingFrames);
    if (t <= swingIn + kFeintPullbackFrames)
        return Sweep(fake, cocked, static_cast<float>(t - swingIn) / static_cast<float>(kFeintPullbackFrames));
    return {cocked.x + std::sin(static_cast<float>(t) * 0.8f) * 2.0f, cocked.y};
}

struct StrikePose {
    BossSwordPose start;
    BossSwordPose end;
};

std::vector<StrikePose> StrikePoses(const BossMove& move) {
    std::vector<StrikePose> poses;
    bool side = false;
    for (const auto& strike : move.strikes) {
        switch (strike.direction) {
        case BossStrikeDirection::Horizontal:
            poses.push_back(side ? StrikePose{kSide, kBack} : StrikePose{kBack, kSide});
            side = !side;
            break;
        case BossStrikeDirection::Vertical: poses.push_back({kUp, kVerticalEnd}); break;
        case BossStrikeDirection::Kesa: poses.push_back({kUpLeft, kDownRight}); break;
        case BossStrikeDirection::ReverseKesa: poses.push_back({kUpRight, kDownLeft}); break;
        }
    }
    if (poses.empty())
        poses.push_back({kBack, kSide});
    return poses;
}

int SegmentLength(const BossComponent& boss, const BossMove& move, size_t index) {
    if (BossMoveTable::IsMoveType(move.type) && index == 0)
        return std::max(1, boss.actFrames);
    return move.segments[index].length;
}

} // namespace

BossSegmentProgress BossPose::Progress(const BossComponent& boss, const BossMove& move) {
    BossSegmentProgress result;
    int accumulated = 0, hitIndex = 0;
    for (size_t k = 0; k < move.segments.size(); ++k) {
        const BossSegment& segment = move.segments[k];
        const int length = SegmentLength(boss, move, k);
        if (boss.stateFrame <= accumulated + length) {
            result.segment = static_cast<int>(k);
            result.hitIndex = hitIndex;
            result.tIn = boss.stateFrame - accumulated;
            result.swingLength = std::max(1, std::min(length, boss.swingInFrames + boss.swingFrames));
            result.hit = segment.hit;
            result.progress = segment.hit ? static_cast<float>(result.tIn) / static_cast<float>(result.swingLength)
                                          : static_cast<float>(result.tIn) / static_cast<float>(length);
            return result;
        }
        if (segment.hit)
            ++hitIndex;
        accumulated += length;
    }
    result.segment = std::max(0, static_cast<int>(move.segments.size()) - 1);
    result.hitIndex = hitIndex;
    result.progress = 1.0f;
    return result;
}

BossSwordPose BossPose::Target(const BossComponent& boss) {
    if (boss.state == BossState::Stunned || boss.state == BossState::Dead)
        return kStunned;
    const auto& moves = boss.table.GetMoves();
    if (boss.state != BossState::Act || boss.move < 0 || static_cast<size_t>(boss.move) >= moves.size())
        return kRest;
    const BossMove& move = moves[static_cast<size_t>(boss.move)];
    const BossActPhase phase = boss.phase;
    const int t = boss.stateFrame;
    auto sw = [&](BossSwordPose a, BossSwordPose c) {
        if (phase == BossActPhase::Pre)
            return Windup(boss, kRest, a, c, t, move.pre, false);
        if (phase == BossActPhase::Post)
            return c;
        const auto g = Progress(boss, move);
        return g.hit ? Swing(boss, a, c, g.tIn, g.swingLength) : c;
    };
    switch (move.type) {
    case BossMoveType::Dash: return phase == BossActPhase::Post ? kRest : sw(kBack, kSide);
    case BossMoveType::SlashR: return sw(kUpLeft, kDownRight);
    case BossMoveType::SlashL: return sw(kUpRight, kDownLeft);
    case BossMoveType::SlashH: return sw(kBack, kSide);
    case BossMoveType::Overhead:
    case BossMoveType::SlashV: return sw(kUp, kVerticalEnd);
    case BossMoveType::Jump: {
        // 1 撃目は左上→右下、2 撃目は右上→左下（X 字）。間の短い区間で 2 撃目の構えへ引く。
        if (phase == BossActPhase::Pre)
            return Windup(boss, kRest, kUpLeft, kDownRight, t, move.pre, false);
        if (phase == BossActPhase::Post)
            return kRest;
        const auto g = Progress(boss, move);
        if (g.hitIndex % 2 == 0)
            return g.hit ? Swing(boss, kUpLeft, kDownRight, g.tIn, g.swingLength)
                         : Sweep(kDownRight, Cocked(kUpRight, kDownLeft), g.progress);
        return Swing(boss, kUpRight, kDownLeft, g.tIn, g.swingLength);
    }
    case BossMoveType::Kick: return kHeld;
    case BossMoveType::Clone: return kForward;
    case BossMoveType::Ambush: {
        if (phase != BossActPhase::Act)
            return kRest;
        const auto g = Progress(boss, move);
        const BossSegment& segment = move.segments[static_cast<size_t>(g.segment)];
        if (g.hit)
            return Swing(boss, kBack, kSide, g.tIn, g.swingLength);
        if (segment.kind != BossSegmentKind::Appear)
            return kRest;
        // 現れて振り上げ→構え（最後の 1 回はフェイント）。
        return Windup(boss, kRest, kBack, kSide, g.tIn, segment.length, segment.feint);
    }
    case BossMoveType::Knife:
        if (phase == BossActPhase::Pre)
            return kKnifeStart;
        if (phase == BossActPhase::Act)
            return Sweep(kKnifeStart, kKnifeEnd, Progress(boss, move).progress * 2.0f);
        return kRest;
    case BossMoveType::String: {
        const auto poses = StrikePoses(move);
        if (phase == BossActPhase::Pre)
            return Windup(boss, kRest, poses[0].start, poses[0].end, t, move.pre,
                          !move.strikes.empty() && move.strikes[0].feint);
        if (phase == BossActPhase::Post)
            return poses.back().end;
        const auto g = Progress(boss, move);
        const BossSegment& segment = move.segments[static_cast<size_t>(g.segment)];
        const size_t k = std::min(static_cast<size_t>(std::max(0, segment.strike)), poses.size() - 1);
        if (g.hit)
            return Swing(boss, poses[k].start, poses[k].end, g.tIn, g.swingLength);
        return Windup(boss, poses[k > 0 ? k - 1 : 0].end, poses[k].start, poses[k].end, g.tIn, segment.length,
                      segment.feint);
    }
    default: return kRest;
    }
}

float BossPose::RootHeight(const BossComponent& boss) { return boss.height + boss.floatHeight - boss.sink; }

BossSwordPoints BossPose::SwordPoints(const BossComponent& boss, BossSwordPose pose) {
    const float px = pose.x * kRadiansPerDegree, py = pose.y * kRadiansPerDegree;
    const Vec3 local{std::cos(px) * std::sin(py), -std::sin(px), std::cos(px) * std::cos(py)};
    const Vec3 direction = RotateY(local, boss.yaw);
    const Vec3 root{boss.ground.x, boss.ground.y + RootHeight(boss), boss.ground.z};
    const Vec3 shoulder = Add(root, RotateY(boss.swordShoulder, boss.yaw));
    BossSwordPoints points;
    points.base = Add(shoulder, Mul(direction, boss.swordLength * kSwordBaseShare));
    points.inner = Add(shoulder, Mul(direction, boss.swordLength * kSwordInnerShare));
    points.outer = Add(shoulder, Mul(direction, boss.swordLength + boss.swordHazeLength));
    return points;
}

} // namespace Project_TD2_2
