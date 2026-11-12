#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BurnFieldComponent.h"

namespace Project_TD2_2 {

// 燃えているセルを、場の火の流体域（ArenaFireField。通り道は Channel1）の炎として見せる。規則は変えない。
// 燃料の注入（FluidView の Fuel）は球で、1 つの域が受け取れる数に限りがある。そこで少数の「筆」が、毎フレーム
// 「いちばん長く燃料をもらっていない燃えているセル」へ飛び、そこへ 1 フレームぶんの燃料を置く。置いた燃料は
// 火の域の中で燃え続けるので、同じ所へ 0.1〜0.3 秒ごとに戻れば炎は途切れない。筆 1 本が 1 秒に配れるのは約 120 m²。
// 筆の足元では、場の霧（Default）を吸収だけの力で焼く。
class BurnFieldVisuals final {
public:
    // 描画フレームごとに呼ぶ。seconds はそのフレームのゲーム時間。
    static void Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BurnFieldComponent& field, float seconds);
};

} // namespace Project_TD2_2
