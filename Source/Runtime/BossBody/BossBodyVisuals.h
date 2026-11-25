#pragma once

#include "HIKARI/Project/WorldView.h"
#include "../Boss/BossComponent.h"

namespace Project_TD2_2 {

// ボスの体の見た目：骨格の体（BossAnimBody）を流し、体に付く効果（胴の煙・火の舌・刀光・手の光・目）を、その骨の
// 置き場所に出す。
class BossBodyVisuals final {
public:
    // alpha は FrameContext::fixedInterpolationAlpha（外形を、前の tick とこの tick の間のこの割合の所に置く）。
    static void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds,
                       float alpha);
    // 固定 tick の終わりに、体の物体へ置いた位置を覚える（BossSystem が置いた後に呼ぶ）。
    static void NoteTick(BossComponent& boss, HIKARI::GameProjectFloat3 position);
    // 編集器のプレビュー（Play の外。c-e0c500）：演出ワークスペースが登場の演出の資産を流している間、役者を演技の姿にする。
    static void UpdatePreviews(HIKARI::WorldView& world);
    // 出し続けているエフェクト（胴の煙）を止める。
    static void Stop(HIKARI::WorldView& world, BossComponent& boss);
};

} // namespace Project_TD2_2
