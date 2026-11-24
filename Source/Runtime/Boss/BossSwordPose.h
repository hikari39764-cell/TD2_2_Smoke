#pragma once

#include "HIKARI/Project/ValueTypes.h"

namespace Project_TD2_2 {

// 規則の剣の構え（体の向きの局所。足元が原点、+X が左、+Z が前）。当たりの線は付け根 grip から刀の向き direction へ
// 伸びる（見える刀の握りの中央は、grip から刀の向きへ Boss.bodySwordGripAlong 先）。腰の落とし・前傾・胴の捻り・
// 両手の重みは、見た目（体・鎧・左手）がこの構えに付いて行くための値で、当たりには効かない。
// 既定値は、Boss.swordShoulder の既定値の所で剣を下ろした構え（BossPose の kRest）。
struct BossSwordPose {
    HIKARI::GameProjectFloat3 grip{-0.855f, 2.372f, 0.186f};
    HIKARI::GameProjectFloat3 direction{-0.0594f, -0.9397f, 0.3368f};
    // 斬る向き（刃を向ける先。刀に直交する長さ 1。振りの型の間だけ。0 は決まっていない）。
    HIKARI::GameProjectFloat3 edge{};
    // 腰を落とす量 (m)・前傾 (度)・胴の捻り（度。+ で左へ回す）・左手で柄を握る重み (0〜1)。
    float crouch = 0.0f;
    float lean = 0.0f;
    float twist = 0.0f;
    float twoHand = 0.0f;
    // 当たりの線を剣先の先へ伸ばす長さ (m。霧をまとった刀。動捕の両手で握った振り。c-40f6c1)。
    float reach = 0.0f;
};

} // namespace Project_TD2_2
