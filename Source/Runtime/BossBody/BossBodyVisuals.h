#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossBodyComponent.h"
#include "../Boss/BossComponent.h"
#include "../Boss/BossIntroComponent.h"
#include "../Boss/BossPhaseShiftComponent.h"

namespace Project_TD2_2 {

// ボスの体の見た目：骨格の体（BossAnimBody）を流し、体に付く効果（胴の煙・火の舌・刀光・手の光・目）を、その骨の
// 置き場所に出す。
class BossBodyVisuals final {
public:
    // alpha は FrameContext::fixedInterpolationAlpha（体を、前の tick とこの tick の間のこの割合の所に置く）。look は同じ物体の
    // 体の見た目の部品（無ければ nullptr。体も体に付く効果も出さない）。intro・phaseShift は同じ物体の登場・変わり目の
    // 演出の部品（無ければ nullptr。その演出をしない）。
    static void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, Boss& boss,
                       BossBody* look, BossIntro* intro, BossPhaseShift* phaseShift,
                       float seconds, float alpha);
    // 固定 tick の終わりに、体の物体へ置いた位置を覚える（BossSystem が置いた後に呼ぶ。描画のフレームの補間が読む）。
    static void NoteTick(Boss& boss, HIKARI::GameProjectFloat3 position);
    // 編集器のプレビュー（Play の外。c-e0c500）：演出ワークスペースが登場の演出の資産を流している間、役者を演技の姿にする。
    static void UpdatePreviews(HIKARI::WorldView& world);
    // 出し続けているエフェクト（胴の煙・変わり目の火の輪）を止める。
    static void Stop(HIKARI::WorldView& world, Boss& boss, BossBody* look,
                     BossPhaseShift* phaseShift);
};

} // namespace Project_TD2_2
