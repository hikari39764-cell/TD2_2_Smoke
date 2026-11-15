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

inline Vec3 ProjectOut(Vec3 v, Vec3 axis) { return Sub(v, Mul(axis, Dot(v, axis))); }

inline float AngleBetween(Vec3 a, Vec3 b) { return std::atan2(Length(Cross(a, b)), Dot(a, b)); }

// a から b への大円の接ベクトル（長さ＝角度 rad）。真逆の時は向きが決まらないので 0。
inline Vec3 LogMap(Vec3 a, Vec3 b) {
    const Vec3 tangent = ProjectOut(b, a);
    const float s = Length(tangent);
    if (s < 1e-6f)
        return {};
    return Mul(tangent, std::atan2(s, std::clamp(Dot(a, b), -1.0f, 1.0f)) / s);
}

// a から接ベクトル v（長さ＝角度）だけ大円に沿って進んだ向き。
inline Vec3 ExpMap(Vec3 a, Vec3 v) {
    const float angle = Length(v);
    if (angle < 1e-6f)
        return a;
    return Normalize(Add(Mul(a, std::cos(angle)), Mul(v, std::sin(angle) / angle)), a);
}

inline Vec3 Slerp(Vec3 a, Vec3 b, float t) { return ExpMap(a, Mul(LogMap(a, b), t)); }

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
inline BossBodyPlacement Cuff(Vec3 wrist, Vec3 elbow, Vec3 back, float scale) {
    BossBodyPlacement cuff;
    cuff.position = wrist;
    cuff.rotation = AimZ(Mul(Normalize(elbow), -1.0f), back);
    cuff.scale = scale;
    return cuff;
}

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

// 刀の模型の拡大（剣の長さに合わせる）と、手の模型の拡大。
inline float SwordScale(const BossComponent& boss) {
    return std::max(0.01f, boss.swordLength - boss.body.swordGripAlong) / std::max(0.01f, boss.body.swordModelLength);
}
inline float HandScale(const BossComponent& boss) { return SwordScale(boss) * boss.body.handScale; }

// 局所（体の向きの枠。足元が原点）の点・向きを世界へ移す。lock で「体の物体の変換（傾き・屈み込み）」と
// 「規則の線の枠（足元と体の向きだけ）」を混ぜる。
struct MotionFrame {
    const BossComponent& boss;
    const HIKARI::GameProjectTransform& body;
    float lock;

    Vec3 Point(Vec3 local) const {
        const Vec3 root{boss.ground.x, boss.ground.y + BossPose::RootHeight(boss), boss.ground.z};
        const Vec3 rule = Add(root, RotateY(local, boss.yaw));
        const Vec3 centered{local.x * body.scale.x, (local.y - boss.bodyHalfHeight) * body.scale.y,
                            local.z * body.scale.z};
        return Lerp(Add(body.position, Rotate(body.rotation, centered)), rule, lock);
    }
    Vec3 Direction(Vec3 local) const {
        return Normalize(Lerp(Rotate(body.rotation, local), RotateY(local, boss.yaw), lock), local);
    }
};

} // namespace Project_TD2_2::BossBodyMath
