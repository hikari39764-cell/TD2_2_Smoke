#pragma once

#include <algorithm>
#include <cmath>

#include "BossBodyMath.h"

// 演出（登場 c-429242・フェーズの変わり目 c-8dd296）で、外形の置き場所を上書きする時に共通に使う道具：回転、刀の姿勢
// （握り・剣先・棟）、3 つの姿勢を通る刀の道、体の動き（胴の捻り・腰の傾き・腰を落とす）、両手持ちの左手。
// 「基準」は、足元 origin と向き yaw を持つもの（登場の状態・変わり目の状態）。見た目だけが使い、規則は使わない。
namespace Project_TD2_2::BossPoseKit {

using namespace BossBodyMath;

inline Quat AroundY(float radians) { return {0.0f, std::sin(radians * 0.5f), 0.0f, std::cos(radians * 0.5f)}; }

// 向きを近い側で混ぜる（正規化した線形補間）。
inline Quat Blend(Quat a, Quat b, float t) {
    t = std::clamp(t, 0.0f, 1.0f);
    const float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    const float sign = dot < 0.0f ? -1.0f : 1.0f;
    Quat q{a.x + (b.x * sign - a.x) * t, a.y + (b.y * sign - a.y) * t, a.z + (b.z * sign - a.z) * t,
           a.w + (b.w * sign - a.w) * t};
    const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (length < kTiny)
        return b;
    return {q.x / length, q.y / length, q.z / length, q.w / length};
}

inline float Unit(float value) { return std::clamp(value, 0.0f, 1.0f); }

// 刀の姿勢（世界）：握りの位置、剣先の向き、棟の向き。
struct Held {
    Vec3 grip{};
    Vec3 tip{0.0f, -1.0f, 0.0f};
    Vec3 spine{0.0f, 0.0f, 1.0f};
};

inline BossBodyPlacement Place(const Held& held, float scale) {
    BossBodyPlacement placement;
    placement.position = held.grip;
    placement.rotation = AimZ(Normalize(held.tip), Perpendicular(held.spine, Normalize(held.tip)));
    placement.scale = scale;
    return placement;
}

// 体の動き：胴の捻り (rad。正が左)、前への傾き (rad)、腰を落とす割合、左手を柄へ添える重み。
struct Motion {
    float twist = 0.0f;
    float lean = 0.0f;
    float sink = 0.0f;
    float twoHand = 0.0f;
};

// v を、軸 axis（単位の向き）のまわりに angle (rad) 回す。
inline Vec3 RotateAbout(Vec3 v, Vec3 axis, float angle) {
    const float c = std::cos(angle), s = std::sin(angle);
    return Add(Add(Mul(v, c), Mul(Cross(axis, v), s)), Mul(axis, Dot(axis, v) * (1.0f - c)));
}

// 刀の道の上の姿勢：構え → 中ほど → 終わりの 3 つを通る、折れ目の無い弧（e は 0〜1 の外でもよい）。
inline Held Sweep(const Held& wind, const Held& mid, const Held& end, float e) {
    const float a = 2.0f * (e - 0.5f) * (e - 1.0f), b = -4.0f * e * (e - 1.0f), c = 2.0f * e * (e - 0.5f);
    Held held = mid;
    held.grip = Add(Add(Mul(wind.grip, a), Mul(mid.grip, b)), Mul(end.grip, c));
    held.tip = Normalize(Add(Add(Mul(wind.tip, a), Mul(mid.tip, b)), Mul(end.tip, c)), mid.tip);
    return held;
}

} // namespace Project_TD2_2::BossPoseKit
