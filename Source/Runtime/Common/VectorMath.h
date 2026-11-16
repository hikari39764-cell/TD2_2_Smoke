#pragma once

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/ValueTypes.h"

namespace Project_TD2_2::VectorMath {

using Vec3 = HIKARI::GameProjectFloat3;

// 角度の定数（各ファイルで書き方の違う同じ値を持っていたものをまとめた。どの書き方も float では同じ値）。
inline constexpr float kPi = 3.14159265358979323846f;
inline constexpr float kTau = 2.0f * kPi;
inline constexpr float kRadiansPerDegree = kPi / 180.0f;

inline Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 Difference(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 Scale(Vec3 a, float value) { return {a.x * value, a.y * value, a.z * value}; }
inline float Dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 Cross(Vec3 a, Vec3 b) { return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x}; }
inline float Length(Vec3 a) { return std::sqrt(Dot(a, a)); }
inline float Lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float Clamp01(float value) { return std::clamp(value, 0.0f, 1.0f); }
// 0〜1 の外は抑え、両端でゆるむ 3 次の曲線。
inline float Smoothstep(float x) {
    const float t = Clamp01(x);
    return t * t * (3.0f - 2.0f * t);
}

inline Vec3 Normalize(Vec3 vector, Vec3 fallback) {
    const float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
    return length > 0.0f ? Scale(vector, 1.0f / length) : fallback;
}

// 局所の +Y を単位方向へ向ける最短回転。ヒョウタンの口と、エフェクト資産の「前」が +Y。
inline HIKARI::GameProjectQuaternion RotationFromYAxis(Vec3 direction) {
    // 真下だけは回転軸が決まらないので明示する。
    if (direction.y <= -1.0f)
        return {1.0f, 0.0f, 0.0f, 0.0f};
    const float w = 1.0f + direction.y;
    const float length = std::sqrt(direction.z * direction.z + direction.x * direction.x + w * w);
    return {direction.z / length, 0.0f, -direction.x / length, w / length};
}

// 局所の +X を単位方向へ向ける最短回転。流体のエフェクト資産（弾・砲口・口の域）の「前」が +X。
inline HIKARI::GameProjectQuaternion RotationFromXAxis(Vec3 direction) {
    const float w = 1.0f + direction.x;
    const float length = std::sqrt(direction.y * direction.y + direction.z * direction.z + w * w);
    // 真後ろだけは回転軸が決まらないので明示する。
    if (length < 0.0001f)
        return {0.0f, 1.0f, 0.0f, 0.0f};
    return {0.0f, -direction.z / length, direction.y / length, w / length};
}

} // namespace Project_TD2_2::VectorMath
