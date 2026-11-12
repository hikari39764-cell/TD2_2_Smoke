#pragma once

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/ValueTypes.h"

// ボスの計算に使う小さなベクトル演算。向き（yaw）は +Z が 0、+X が 90 度（試作と同じ。右は −X）。
namespace Project_TD2_2::BossMath {

using Vec3 = HIKARI::GameProjectFloat3;
using Quat = HIKARI::GameProjectQuaternion;

inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kRadiansPerDegree = kPi / 180.0f;

inline Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 Sub(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 Mul(Vec3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }
inline float FlatLength(Vec3 a) { return std::sqrt(a.x * a.x + a.z * a.z); }
inline Vec3 Lerp(Vec3 a, Vec3 b, float t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
}
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline Vec3 Normalize(Vec3 a, Vec3 fallback = {0.0f, 0.0f, 1.0f}) {
    const float length = Length(a);
    return length > 1e-6f ? Mul(a, 1.0f / length) : fallback;
}
inline Vec3 FlatNormalize(Vec3 a, Vec3 fallback = {0.0f, 0.0f, 1.0f}) { return Normalize({a.x, 0.0f, a.z}, fallback); }
inline Vec3 Forward(float yaw) { return {std::sin(yaw), 0.0f, std::cos(yaw)}; }
inline float YawOf(Vec3 direction) { return std::atan2(direction.x, direction.z); }
inline float WrapPi(float angle) {
    angle = std::fmod(angle + kPi, 2.0f * kPi);
    if (angle < 0.0f)
        angle += 2.0f * kPi;
    return angle - kPi;
}
// y 軸まわりの回転（試作の Euler 'YXZ' の Y と同じ向き）。
inline Vec3 RotateY(Vec3 v, float angle) {
    const float c = std::cos(angle), s = std::sin(angle);
    return {v.x * c + v.z * s, v.y, -v.x * s + v.z * c};
}
inline float Smooth(float u) {
    u = std::clamp(u, 0.0f, 1.0f);
    return u * u * (3.0f - 2.0f * u);
}

inline Quat Multiply(Quat a, Quat b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
// 向き yaw、前後の傾き lean（正で前へ）、横の傾き sway の順に回す（Euler 'YXZ'）。
inline Quat YawLeanSway(float yaw, float lean, float sway) {
    const Quat qy{0.0f, std::sin(yaw * 0.5f), 0.0f, std::cos(yaw * 0.5f)};
    const Quat qx{std::sin(lean * 0.5f), 0.0f, 0.0f, std::cos(lean * 0.5f)};
    const Quat qz{0.0f, 0.0f, std::sin(sway * 0.5f), std::cos(sway * 0.5f)};
    return Multiply(Multiply(qy, qx), qz);
}
// 局所 +X を forward、+Y を side（forward に直交する成分）へ向ける回転。斬撃のエフェクトの向きに使う。
inline Quat BasisRotation(Vec3 forward, Vec3 side) {
    const Vec3 x = Normalize(forward);
    const Vec3 z =
        Normalize(Cross(x, side), std::abs(x.y) < 0.9f ? Cross(x, {0.0f, 1.0f, 0.0f}) : Vec3{0.0f, 0.0f, 1.0f});
    const Vec3 y = Cross(z, x);
    const float trace = x.x + y.y + z.z;
    if (trace > 0.0f) {
        const float s = std::sqrt(trace + 1.0f) * 2.0f;
        return {(y.z - z.y) / s, (z.x - x.z) / s, (x.y - y.x) / s, 0.25f * s};
    }
    if (x.x > y.y && x.x > z.z) {
        const float s = std::sqrt(1.0f + x.x - y.y - z.z) * 2.0f;
        return {0.25f * s, (y.x + x.y) / s, (z.x + x.z) / s, (y.z - z.y) / s};
    }
    if (y.y > z.z) {
        const float s = std::sqrt(1.0f + y.y - x.x - z.z) * 2.0f;
        return {(y.x + x.y) / s, 0.25f * s, (z.y + y.z) / s, (z.x - x.z) / s};
    }
    const float s = std::sqrt(1.0f + z.z - x.x - y.y) * 2.0f;
    return {(z.x + x.z) / s, (z.y + y.z) / s, 0.25f * s, (x.y - y.x) / s};
}
inline Vec3 Rotate(Quat q, Vec3 v) {
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 t = Mul(Cross(u, v), 2.0f);
    return Add(Add(v, Mul(t, q.w)), Cross(u, t));
}

// 2 本の線分の最短距離（斬撃の弧・刃とプレイヤーの体の軸）。ほぼ平行な長い線分（縦の斬撃と立った体）でも
// 打ち消しで崩れないよう、倍精度で解く。
inline float SegmentDistance(Vec3 p1, Vec3 q1, Vec3 p2, Vec3 q2) {
    struct D3 {
        double x, y, z;
    };
    const auto sub = [](D3 u, D3 v) { return D3{u.x - v.x, u.y - v.y, u.z - v.z}; };
    const auto dot = [](D3 u, D3 v) { return u.x * v.x + u.y * v.y + u.z * v.z; };
    const auto of = [](Vec3 v) { return D3{v.x, v.y, v.z}; };
    const D3 a1 = of(p1), a2 = of(p2);
    const D3 d1 = sub(of(q1), a1), d2 = sub(of(q2), a2), r = sub(a1, a2);
    const double a = dot(d1, d1), e = dot(d2, d2), f = dot(d2, r);
    double s = 0.0, t = 0.0;
    if (a <= 1e-12 && e <= 1e-12)
        return static_cast<float>(std::sqrt(dot(r, r)));
    if (a <= 1e-12) {
        t = std::clamp(f / e, 0.0, 1.0);
    } else {
        const double c = dot(d1, r);
        if (e <= 1e-12) {
            s = std::clamp(-c / a, 0.0, 1.0);
        } else {
            const double b = dot(d1, d2), denominator = a * e - b * b;
            s = denominator > 1e-12 ? std::clamp((b * f - c * e) / denominator, 0.0, 1.0) : 0.0;
            t = (b * s + f) / e;
            if (t < 0.0) {
                t = 0.0;
                s = std::clamp(-c / a, 0.0, 1.0);
            } else if (t > 1.0) {
                t = 1.0;
                s = std::clamp((b - c) / a, 0.0, 1.0);
            }
        }
    }
    const D3 gap{r.x + d1.x * s - d2.x * t, r.y + d1.y * s - d2.y * t, r.z + d1.z * s - d2.z * t};
    return static_cast<float>(std::sqrt(dot(gap, gap)));
}

} // namespace Project_TD2_2::BossMath
