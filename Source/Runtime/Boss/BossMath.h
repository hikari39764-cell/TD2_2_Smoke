#pragma once

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/ValueTypes.h"
#include "../Common/VectorMath.h"

// ボスの計算に使う小さなベクトル演算。向き（yaw）は +Z が 0、+X が 90 度（試作と同じ。右は −X）。
// 共通の演算（足す・内積・回転など）は Common/VectorMath.h のものを使い、ここにはボスだけが使うものを置く。
// Sub・Mul・Smooth はボスのコードで使っている名前のまま、VectorMath の Difference・Scale・Smoothstep へ渡す。
namespace Project_TD2_2::BossMath {

using Vec3 = HIKARI::GameProjectFloat3;
using Quat = HIKARI::GameProjectQuaternion;

using VectorMath::kPi;
using VectorMath::kRadiansPerDegree;
using VectorMath::kTau;

using VectorMath::Add;
using VectorMath::Conjugate;
using VectorMath::Cross;
using VectorMath::Dot;
using VectorMath::Length;
using VectorMath::Lerp;
using VectorMath::Multiply;
using VectorMath::Nlerp;
using VectorMath::Rotate;
using VectorMath::RotateY;
inline Vec3 Sub(Vec3 a, Vec3 b) { return VectorMath::Difference(a, b); }
inline Vec3 Mul(Vec3 a, float s) { return VectorMath::Scale(a, s); }
inline float FlatLength(Vec3 a) { return std::sqrt(a.x * a.x + a.z * a.z); }
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
inline float Smooth(float u) { return VectorMath::Smoothstep(u); }
// 速く動き出して最後にゆっくり止まる（跳び上がり・後ろへ跳ぶ）。
inline float EaseOut(float u) { return 1.0f - (1.0f - u) * (1.0f - u); }
// frames F のうち frame F 目までの進み（0〜1）。frames が 0 以下でも割らない。
inline float Share(int frame, int frames) {
    return std::clamp(static_cast<float>(frame) / static_cast<float>(std::max(1, frames)), 0.0f, 1.0f);
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

// 向き（長さ 1 のベクトル）の計算。規則の剣の構えの混ぜ合わせと、外形の刀のばねが使う。
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

// 体の局所（足元が原点）の点を混ぜる：縦の軸のまわりに回して寄せ、軸からの距離と高さは直線で寄せる（手の位置を
// 直線で寄せると、体の右と左の間で胴の中を通る）。軸にほとんど乗っている点は直線で寄せる。
inline Vec3 LerpAroundY(Vec3 a, Vec3 b, float t) {
    constexpr float kMinRadius = 0.05f;
    const float ra = std::sqrt(a.x * a.x + a.z * a.z), rb = std::sqrt(b.x * b.x + b.z * b.z);
    if (ra < kMinRadius || rb < kMinRadius)
        return Lerp(a, b, t);
    const float from = std::atan2(a.x, a.z);
    const float angle = from + WrapPi(std::atan2(b.x, b.z) - from) * t;
    const float radius = Lerp(ra, rb, t);
    return {std::sin(angle) * radius, Lerp(a.y, b.y, t), std::cos(angle) * radius};
}

// v を、軸 axis（長さは問わない）のまわりに angle (rad) 回す（右手の向き）。
inline Vec3 RotateAbout(Vec3 v, Vec3 axis, float angle) {
    const Vec3 k = Normalize(axis, {0.0f, 1.0f, 0.0f});
    const float c = std::cos(angle), s = std::sin(angle);
    return Add(Add(Mul(v, c), Mul(Cross(k, v), s)), Mul(k, Dot(k, v) * (1.0f - c)));
}

} // namespace Project_TD2_2::BossMath
