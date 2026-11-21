#pragma once

#include "HIKARI/Project/WorldView.h"
#include "../Boss/BossCloneComponent.h"
#include "../Boss/BossComponent.h"

namespace Project_TD2_2 {

// ボスの外形の模型を出す。置き場所は BossBodyRig が決め、ここは模型のノードを 1 つずつ、表示専用の模型として
// 毎フレーム頼むだけ（物体・当たり・動画は作らない。頼まなかったフレームは消える）。
class BossBodyVisuals final {
public:
    // alpha は FrameContext::fixedInterpolationAlpha（外形を、前の tick とこの tick の間のこの割合の所に置く）。
    static void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds,
                       float alpha);
    // 固定 tick の終わりに、体の物体へ置いた位置を覚える（BossSystem が置いた後に呼ぶ）。
    static void NoteTick(BossComponent& boss, HIKARI::GameProjectFloat3 position);
    // 霞分身 1 体にも、出ている間は同じ外形を待機の構えで出す（設定は本体の Boss.body。胴の煙は出さない）。
    static void UpdateClone(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                            BossCloneComponent& clone, float seconds);
    // 編集器のプレビュー（Play の外。c-e0c500）：演出ワークスペースが登場の演出の資産を流している間、役者を演技の姿にする。
    static void UpdatePreviews(HIKARI::WorldView& world);
    // 出し続けているエフェクト（胴の煙）を止める。
    static void Stop(HIKARI::WorldView& world, BossComponent& boss);
};

} // namespace Project_TD2_2
