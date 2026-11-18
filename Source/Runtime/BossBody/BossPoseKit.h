#pragma once

#include <algorithm>
#include <cmath>

#include "BossBodyMath.h"

// 演出（登場 c-429242・フェーズの変わり目 c-8dd296）で、外形の置き場所を上書きする時に共通に使う道具：回転、刀の姿勢
// （握り・剣先・棟）、3 つの姿勢を通る刀の道、体の動き（胴の捻り・腰の傾き・腰を落とす）、両手持ちの左手。
// 「基準」は、足元 origin と向き yaw を持つもの（登場の状態・変わり目の状態）。見た目だけが使い、規則は使わない。
namespace Project_TD2_2::BossPoseKit {

using namespace BossBodyMath;

inline Quat AroundX(float radians) { return {std::sin(radians * 0.5f), 0.0f, 0.0f, std::cos(radians * 0.5f)}; }
inline Quat AroundY(float radians) { return {0.0f, std::sin(radians * 0.5f), 0.0f, std::cos(radians * 0.5f)}; }
inline Quat AroundZ(float radians) { return {0.0f, 0.0f, std::sin(radians * 0.5f), std::cos(radians * 0.5f)}; }
inline Quat AroundAxis(Vec3 axis, float radians) {
    const Vec3 n = Normalize(axis, {0.0f, 1.0f, 0.0f});
    const float s = std::sin(radians * 0.5f);
    return {n.x * s, n.y * s, n.z * s, std::cos(radians * 0.5f)};
}

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

inline Held Mix(const Held& a, const Held& b, float t) {
    Held held;
    held.grip = Lerp(a.grip, b.grip, t);
    held.tip = Slerp(Normalize(a.tip), Normalize(b.tip), t);
    held.spine = Normalize(Lerp(a.spine, b.spine, Unit(t)), b.spine);
    return held;
}

inline BossBodyPlacement Place(const Held& held, float scale) {
    BossBodyPlacement placement;
    placement.position = held.grip;
    placement.rotation = AimZ(Normalize(held.tip), Perpendicular(held.spine, Normalize(held.tip)));
    placement.scale = scale;
    return placement;
}

inline Held HeldOf(const BossBodyPlacement& placement) {
    return {placement.position, Rotate(placement.rotation, {0.0f, 0.0f, 1.0f}),
            Rotate(placement.rotation, {0.0f, 1.0f, 0.0f})};
}

// 体の物体の変換で、体の局所（足元が原点）の点を世界へ（BossBodyRig の BodyFrame と同じ）。
inline Vec3 BodyToWorld(const BossComponent& boss, const HIKARI::GameProjectTransform& body, Vec3 local) {
    const Vec3 centered{local.x * body.scale.x, (local.y - boss.bodyHalfHeight) * body.scale.y, local.z * body.scale.z};
    return Add(body.position, Rotate(body.rotation, centered));
}

// 体の動き：胴の捻り (rad。正が左)、前への傾き (rad)、腰を落とす割合、左手を柄へ添える重み。
struct Motion {
    float twist = 0.0f;
    float lean = 0.0f;
    float sink = 0.0f;
    float twoHand = 0.0f;
};

inline Motion MixMotion(const Motion& a, const Motion& b, float t) {
    return {Lerp(a.twist, b.twist, t), Lerp(a.lean, b.lean, t), Lerp(a.sink, b.sink, t), Lerp(a.twoHand, b.twoHand, t)};
}

// 斬り上げの進み（0〜1。動き出しで一気に動き、行き過ぎて戻る）。
inline float RipAlong(float t, float overshoot) {
    t = Unit(t);
    const float rest = 1.0f - t;
    return 1.0f - rest * rest * rest + overshoot * std::sin(kPi * t) * t;
}

// v を、軸 axis（単位の向き）のまわりに angle (rad) 回す。
inline Vec3 RotateAbout(Vec3 v, Vec3 axis, float angle) {
    const float c = std::cos(angle), s = std::sin(angle);
    return Add(Add(Mul(v, c), Mul(Cross(axis, v), s)), Mul(axis, Dot(axis, v) * (1.0f - c)));
}

inline Quat AboutAxis(Vec3 axis, float angle) {
    const float s = std::sin(angle * 0.5f);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(angle * 0.5f)};
}

// 体の動きの、前への傾きの軸（捻った後の体の右の向き。正の回転で上が前へ倒れる）。基準 frame は origin と yaw を持つ。
template <class Frame>
Vec3 LeanAxis(const Frame& frame, float twist) {
    return Normalize(Cross({0.0f, 1.0f, 0.0f}, RotateY(Forward(frame.yaw), twist)), {1.0f, 0.0f, 0.0f});
}

// 体の部品の世界の点に、体の動き（足元のまわりの捻り → 腰のまわりの前への傾き → 腰を落とす）を掛ける。
// twistShare・leanShare：付いて回る割合。pivot：傾きの軸の、足元からの高さ (m)。
template <class Frame>
Vec3 BendPoint(const Frame& frame, const Motion& m, float pivot, Vec3 point, float twistShare, float leanShare) {
    const float twist = m.twist * twistShare;
    const Vec3 hip{0.0f, pivot, 0.0f};
    Vec3 rel = RotateY(Sub(point, frame.origin), twist);
    rel = Add(hip, RotateAbout(Sub(rel, hip), LeanAxis(frame, twist), m.lean * leanShare));
    rel.y *= 1.0f - m.sink;
    return Add(frame.origin, rel);
}

template <class Frame>
Quat BendTurn(const Frame& frame, const Motion& m, float twistShare, float leanShare) {
    const float twist = m.twist * twistShare;
    return Multiply(AboutAxis(LeanAxis(frame, twist), m.lean * leanShare), AroundY(twist));
}

// 両手で握る左手（BossBodyLeftHand の両手持ちと同じ置き方：右手の後ろの柄頭の側に、正握の拳。親指の側が剣先、手首が
// 左肩の側）。
inline BossBodyPlacement TwoHandLeft(const BossComponent& boss, const BossBodyPlacement& sword, Vec3 shoulder) {
    const BossBodyLeftHandSettings& s = boss.body.leftHand;
    const Vec3 x = Rotate(sword.rotation, {1.0f, 0.0f, 0.0f});
    const Vec3 y = Rotate(sword.rotation, {0.0f, 1.0f, 0.0f});
    const Vec3 z = Rotate(sword.rotation, {0.0f, 0.0f, 1.0f});
    const Vec3 handX = Mul(z, -1.0f);
    Vec3 back = Perpendicular(Add(Add(Mul(x, s.twoHandBack.x), Mul(y, s.twoHandBack.y)), Mul(z, s.twoHandBack.z)), z);
    const Vec3 toShoulder = ProjectOut(Sub(shoulder, sword.position), z);
    if (Length(toShoulder) > kTiny)
        back = Cross(Mul(Normalize(toShoulder), -1.0f), handX);
    const Vec3 fingers = Cross(handX, back);
    const float handScale = HandScale(boss);
    const Vec3 hilt = Sub(sword.position, Mul(z, s.twoHandBehind));
    const Vec3 fist = Add(Add(Mul(handX, s.fistCenter.x), Mul(back, s.fistCenter.y)), Mul(fingers, s.fistCenter.z));
    BossBodyPlacement hand;
    hand.position = Sub(hilt, Mul(fist, handScale));
    hand.rotation = AimZ(fingers, back);
    hand.scale = handScale;
    return hand;
}

// 刀の道の上の姿勢：構え → 中ほど → 終わりの 3 つを通る、折れ目の無い弧（e は 0〜1 の外でもよい）。
inline Held Sweep(const Held& wind, const Held& mid, const Held& end, float e) {
    const float a = 2.0f * (e - 0.5f) * (e - 1.0f), b = -4.0f * e * (e - 1.0f), c = 2.0f * e * (e - 0.5f);
    Held held = mid;
    held.grip = Add(Add(Mul(wind.grip, a), Mul(mid.grip, b)), Mul(end.grip, c));
    held.tip = Normalize(Add(Add(Mul(wind.tip, a), Mul(mid.tip, b)), Mul(end.tip, c)), mid.tip);
    return held;
}

// 刀の道の上の姿勢に、棟の向きを付ける：剣先の進む向き（道の少し前後の差）から決める。lead が 1 なら棟が先、-1 なら
// 刃が先（斬る）。step は差を測る幅、reach は握りから剣先までの長さ。
inline Held SweepEdged(const Held& a, const Held& b, const Held& c, float e, float lead, float step, float reach) {
    Held held = Sweep(a, b, c, e);
    const Held before = Sweep(a, b, c, e - step);
    const Held after = Sweep(a, b, c, e + step);
    const Vec3 travel = Sub(Add(after.grip, Mul(after.tip, reach)), Add(before.grip, Mul(before.tip, reach)));
    held.spine = Mul(Perpendicular(travel, held.tip), lead);
    return held;
}

} // namespace Project_TD2_2::BossPoseKit
