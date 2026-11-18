#pragma once

#include <algorithm>
#include <cmath>

#include "BossBodyRig.h"
#include "../Boss/BossMath.h"
#include "../Boss/BossPose.h"

// 外形（刀・手）の計算で共通に使う小さな道具。見た目だけが使い、規則は使わない。
namespace Project_TD2_2::BossBodyMath {

using namespace BossMath;

// ばねを 1 刻みで進める角の上限（角速度 × 刻み）。粗いと、陽に進めるばねが暴れる。刻みの数の上限。
inline constexpr float kMaxSpringStep = 0.5f;
inline constexpr int kMaxSubsteps = 64;
// 割る数・時間の下限（0 で割らない）。
inline constexpr float kTiny = 1e-4f;

inline Vec3 ClampLength(Vec3 v, float maximum) {
    const float length = Length(v);
    return length > maximum && length > kTiny ? Mul(v, maximum / length) : v;
}

// 時間 seconds で追う時の、1 フレームで縮める割合。
inline float Follow(float dt, float seconds) { return 1.0f - std::exp(-dt / std::max(kTiny, seconds)); }

// axis に直交する向き。直交する成分が無ければ、axis に直交する何かの向きを返す。
inline Vec3 Perpendicular(Vec3 v, Vec3 axis) {
    const Vec3 fallback = std::abs(axis.y) < 0.9f ? Vec3{0.0f, -1.0f, 0.0f} : Vec3{0.0f, 0.0f, 1.0f};
    return Normalize(ProjectOut(v, axis), Normalize(ProjectOut(fallback, axis)));
}

// 局所 +Z を direction、+Y を spine（direction に直交する成分）へ向ける回転。
inline Quat AimZ(Vec3 direction, Vec3 spine) {
    const Vec3 side = Normalize(Cross(spine, direction), {1.0f, 0.0f, 0.0f});
    return BasisRotation(side, Cross(direction, side));
}

// 手首に付く籠手の筒：筒は手首から肘の向き（局所 -Z）へ伸び、腕の甲（局所 +Y）は手の甲の側を向く。
// 筒の先が肩（shoulder）から cuffShoulderClearance より近くなる時は、筒の長さだけを詰める（太さはそのまま）。
inline BossBodyPlacement Cuff(Vec3 wrist, Vec3 elbow, Vec3 back, float scale, const BossBodySettings& s,
                              Vec3 shoulder) {
    BossBodyPlacement cuff;
    cuff.position = wrist;
    cuff.rotation = AimZ(Mul(Normalize(elbow), -1.0f), back);
    cuff.scale = scale;
    const float full = s.cuffModelLength * scale;
    if (s.cuffShoulderClearance > 0.0f && full > 0.0f) {
        const float room = Length(Sub(shoulder, wrist)) - s.cuffShoulderClearance * s.bodyScale;
        cuff.lengthScale = std::clamp(room / full, std::clamp(s.cuffMinLength, 0.0f, 1.0f), 1.0f);
    }
    return cuff;
}

// 体の局所の位置（1 倍の体で書いた値）を、外形の体の大きさ（Boss.bodyScale）へ。
inline Vec3 BodyPoint(const BossBodySettings& s, Vec3 v) { return Mul(v, s.bodyScale); }

// 位置のばね（StepSway と同じ形）。短い刻みに分けて進める。
inline void SpringPoint(Vec3& position, Vec3& velocity, Vec3 target, float frequency, float damping, float dt) {
    const float omega = 2.0f * kPi * frequency;
    const int steps = std::clamp(static_cast<int>(std::ceil(dt * omega / kMaxSpringStep)), 1, kMaxSubsteps);
    const float h = dt / static_cast<float>(steps);
    for (int i = 0; i < steps; ++i) {
        const Vec3 pull = Sub(Mul(Sub(target, position), omega * omega), Mul(velocity, 2.0f * damping * omega));
        velocity = Add(velocity, Mul(pull, h));
        position = Add(position, Mul(velocity, h));
    }
}

inline const BossMove* CurrentMove(const BossComponent& boss) {
    const auto& moves = boss.table.GetMoves();
    if (boss.state != BossState::Act || boss.move < 0 || static_cast<size_t>(boss.move) >= moves.size())
        return nullptr;
    return &moves[static_cast<size_t>(boss.move)];
}

// 待機の構え（下段）：息に合わせて握りと剣先が少し上下する。局所（体の向きの枠。足元が原点）。
inline void IdleKamae(const BossBodyMotionSettings& s, float clock, Vec3& direction, Vec3& grip) {
    const float breath = std::sin(2.0f * kPi * s.breathFrequency * clock);
    const Vec3 base = Normalize(s.idleDirection);
    const Vec3 up = Perpendicular({0.0f, 1.0f, 0.0f}, base);
    direction = ExpMap(base, Mul(up, s.breathDegrees * kRadiansPerDegree * breath));
    grip = Add(s.idleGrip, Vec3{0.0f, s.breathMeters * breath, 0.0f});
}

// 胴の捻りの目標：刀の横の向き（局所 x）に比例し、上限で止める。
inline float TwistTarget(const BossBodyMotionSettings& s, Vec3 direction) {
    const float limit = s.twistMaxDegrees * kRadiansPerDegree;
    return std::clamp(direction.x * s.twistDegrees * kRadiansPerDegree, -limit, limit);
}

// 見える刀の、剣の付け根から剣先までの長さ。当たりの線（剣の長さ＋剣先の霞）より先へは伸ばさない。
inline float VisibleSwordLength(const BossComponent& boss) {
    const float authored = boss.body.swordVisibleLength > 0.0f ? boss.body.swordVisibleLength : boss.swordLength;
    return std::min(authored, boss.swordLength + boss.swordHazeLength);
}

// 見える刀の、握りの中央から剣先までの長さ。
inline float VisibleBladeReach(const BossComponent& boss) {
    return std::max(0.01f, VisibleSwordLength(boss) - boss.body.swordGripAlong);
}

// 握り grip から direction へ reach 伸びた剣先が、高さ floor より下へ入る時は、水平の向きを保ったまま剣先が floor に
// 乗る所まで向きを起こす。真下を向いていて水平の向きが決まらない時は forward を使う。
inline Vec3 KeepTipAbove(Vec3 grip, Vec3 direction, float reach, float floor, Vec3 forward) {
    if (reach <= kTiny || grip.y + direction.y * reach >= floor)
        return direction;
    const float y = std::clamp((floor - grip.y) / reach, -1.0f, 1.0f);
    const Vec3 flat = Normalize(Vec3{direction.x, 0.0f, direction.z}, Normalize(Vec3{forward.x, 0.0f, forward.z}));
    const float horizontal = std::sqrt(std::max(0.0f, 1.0f - y * y));
    return {flat.x * horizontal, y, flat.z * horizontal};
}

// 刀の模型の拡大（見える刀の長さに合わせる）と、手の模型の拡大。
inline float SwordScale(const BossComponent& boss) {
    return VisibleBladeReach(boss) / std::max(0.01f, boss.body.swordModelLength);
}
inline float HandScale(const BossComponent& boss) { return SwordScale(boss) * boss.body.handScale; }

// 局所（体の向きの枠。足元が原点）の点・向きを世界へ移す。lock で「体の物体の変換（傾き・屈み込み）」と
// 「規則の線の枠（足元と体の向きだけ）」を混ぜる。規則の線の枠も、外形を補間でずらした分だけずらす（体と刀を離さない）。
struct MotionFrame {
    const BossComponent& boss;
    const HIKARI::GameProjectTransform& body;
    float lock;

    Vec3 Point(Vec3 local) const {
        const Vec3 shift = boss.bodyState.presentShift;
        const Vec3 root{boss.ground.x + shift.x, boss.ground.y + BossPose::RootHeight(boss) + shift.y,
                        boss.ground.z + shift.z};
        // 規則の枠は体ごとの回転（回し蹴り・体の回転の技）も含める（刀の当たりと同じ所。c-b1824c）。
        const Vec3 rule = Add(root, RotateY(local, boss.yaw + boss.spin));
        const Vec3 centered{local.x * body.scale.x, (local.y - boss.bodyHalfHeight) * body.scale.y,
                            local.z * body.scale.z};
        return Lerp(Add(body.position, Rotate(body.rotation, centered)), rule, lock);
    }
    Vec3 Direction(Vec3 local) const {
        return Normalize(Lerp(Rotate(body.rotation, local), RotateY(local, boss.yaw + boss.spin), lock), local);
    }
};

} // namespace Project_TD2_2::BossBodyMath
