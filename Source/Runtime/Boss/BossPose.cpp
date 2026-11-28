#include "BossPose.h"

#include <algorithm>
#include <cmath>

#include "BossMath.h"
#include "../Common/FieldValues.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 角度で書いた剣の向き（度）。x は前後の傾き（正で下）、y は左右（正でボスの左）。
struct Angles {
    float x = 0.0f;
    float y = 0.0f;
};

// 休めの構え（骨格の体の刀をまだ読めていない間だけ使う）。
constexpr Angles kRest{70.0f, -10.0f};
// 剣の根元（剣の長さに対する割合）と、霞の内側の始まり。
constexpr float kSwordBaseShare = 0.14f;
constexpr float kSwordInnerShare = 0.65f;

Vec3 DirectionOf(Angles a) {
    const float px = a.x * kRadiansPerDegree, py = a.y * kRadiansPerDegree;
    return {std::cos(px) * std::sin(py), -std::sin(px), std::cos(px) * std::cos(py)};
}

// 角度で書いた構え：肩の付け根から、その向きへ剣を出す。
BossSwordPose FromAngles(const Boss& boss, Angles a) {
    BossSwordPose pose;
    pose.grip = ToGP(boss.swordShoulder);
    pose.direction = DirectionOf(a);
    return pose;
}

// 0〜1 の進み u を、peak で速さが最も大きくなる 0〜1 の進みへ。速さは途中で途切れない（u を peak で速さが最大になる
// ように曲げてから、両端で速さ 0 の 5 次の曲線に通す）。
float PeakCurve(float u, float peak) {
    const float x = std::pow(std::clamp(u, 0.0f, 1.0f), std::log(0.5f) / std::log(std::clamp(peak, 0.1f, 0.9f)));
    return Smootherstep(x);
}

// 振りの進み：ゆっくり動き始めて加速し、swingPeakShare で最も速く、振り終わりへ減速して止まる。
float SwingProgress(const Boss& boss, int tIn, int length) {
    return PeakCurve(static_cast<float>(tIn) / static_cast<float>(std::max(1, length)),
                     std::clamp(boss.swingPeakShare, 0.5f, 0.9f));
}

int SegmentLength(const Boss& boss, const BossMove& move, size_t index) {
    if (BossMoveTable::IsMoveType(move.type) && index == 0)
        return std::max(1, boss.actFrames);
    return move.segments[index].length;
}

} // namespace

BossSegmentProgress BossPose::Progress(const Boss& boss, const BossMove& move) {
    BossSegmentProgress result;
    int accumulated = 0, hitIndex = 0;
    for (size_t k = 0; k < move.segments.size(); ++k) {
        const BossSegment& segment = move.segments[k];
        const int length = SegmentLength(boss, move, k);
        if (boss.stateFrame <= accumulated + length) {
            result.segment = static_cast<int>(k);
            result.hitIndex = hitIndex;
            result.tIn = boss.stateFrame - accumulated;
            // クリップの刀で当てる技は、当たりの窓（区間の表）の長さ全部が振り（クリップの刀が窓の間ずっと動く）。
            result.swingLength = BossAnimBody::ClipMove(boss, move)
                                     ? std::max(1, length)
                                     : std::max(1, std::min(length, boss.swingInFrames + boss.swingFrames));
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

BossSwordPose BossPose::Rest(const Boss& boss) { return FromAngles(boss, kRest); }

BossSwordPose BossPose::Target(const Boss& boss) {
    // 規則の剣の構え（c-b4e76a・c-164cab）：クリップの刀で当たる技はクリップの刀（当たり・斬撃の面・刀の煙）、ほかの
    // 間は待機のクリップの始めの刀。どちらも固定 tick の中で読んだクリップから決める（描画のフレームの骨は読まない）。
    // 待機のクリップを読めていない間だけ休めの構え。技の型ごとの程序化の姿勢は持たない。
    if (BossSwordPose clip; BossAnimBody::ClipSwordPose(boss, clip))
        return clip;
    if (BossSwordPose idle; BossAnimBody::IdleSwordPose(boss, idle))
        return idle;
    return Rest(boss);
}

float BossPose::RootHeight(const Boss& boss) { return boss.height + boss.floatHeight; }

BossSwordLine BossPose::SwordLine(const Boss& boss, const BossSwordPose& pose) {
    const Vec3 root{boss.ground.x, boss.ground.y + RootHeight(boss), boss.ground.z};
    return {Add(root, RotateY(pose.grip, boss.yaw)), RotateY(Normalize(pose.direction, {0.0f, -1.0f, 0.0f}), boss.yaw)};
}

BossSwordPoints BossPose::SwordPoints(const Boss& boss, const BossSwordPose& pose) {
    BossSwordLine line = SwordLine(boss, pose);
    // SwordLine は yaw だけで回すので、体の向きとの差（体の回転・狙いのずれのねじり）を足元のまわりに回す。
    const float turn = BodyYaw(boss) - boss.yaw;
    if (turn != 0.0f) {
        const Vec3 center{boss.ground.x, line.root.y, boss.ground.z};
        line.root = Add(center, RotateY(Sub(line.root, center), turn));
        line.direction = RotateY(line.direction, turn);
    }
    BossSwordPoints points;
    points.base = Add(line.root, Mul(line.direction, boss.swordLength * kSwordBaseShare));
    points.inner = Add(line.root, Mul(line.direction, boss.swordLength * kSwordInnerShare));
    points.outer = Add(line.root, Mul(line.direction, boss.swordLength + boss.swordHazeLength + pose.reach));
    return points;
}

float BossPose::BodyYaw(const Boss& boss) { return boss.yaw + boss.spin + boss.aimTwist; }

float BossPose::SwingShare(const Boss& boss, int tIn, int length) {
    return std::clamp(SwingProgress(boss, std::clamp(tIn, 0, std::max(1, length)), length), 0.0f, 1.0f);
}

} // namespace Project_TD2_2
