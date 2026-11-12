#pragma once

#include <cmath>

#include "HIKARI/Project/ValueTypes.h"

namespace Project_TD2_2::VectorMath {

using Vec3 = HIKARI::GameProjectFloat3;

inline Vec3 Add(Vec3 a, Vec3 b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
inline Vec3 Difference(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
inline Vec3 Scale(Vec3 a, float value) { return {a.x * value, a.y * value, a.z * value}; }

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
