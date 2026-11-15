#pragma once

#include "BossBodyRig.h"

namespace Project_TD2_2 {

// 左手（刀を持たない手）の手振り（カード c-f80152）。技の型ごとの手振りを表（kGestureTable）から引き、
// 位置はばね、向きは追いかけで滑らかに動かす。形（開く・拳・刀印）は決めた時刻に差し替える。
// 両手持ち・刀身をなぞる手振りは、見せる刀（BossBodySword が進めた握り・向き・刃）に手を付ける。
// 見た目だけで、規則（当たり・F 数）は読むだけ。
class BossBodyLeftHand final {
public:
    // 刀の見せる姿勢を進めた後に呼ぶ。snap なら、ばねを進めずに目標へそろえる。
    static void Step(BossComponent& boss, float dt, bool snap);
    // 左手と左の籠手の置き場所・形を決める。
    static void Solve(const BossComponent& boss, const HIKARI::GameProjectTransform& body, BossBodyPose& pose);
};

} // namespace Project_TD2_2
