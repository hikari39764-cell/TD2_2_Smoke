#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BurnFieldComponent.h"

namespace Project_TD2_2 {

// 燃えているセルを、場の火の流体域（ArenaFireField。通り道は Channel1）の炎として見せる。規則は変えない。
// 燃料の注入（FluidView の Fuel）は球で、1 つの域が受け取れる数に限りがある。そこで少数の「燃料の点」を、
// 燃えているセルのかたまりの上に置き、同じ場所へ毎フレーム燃料を入れ続ける（エフェクト資産の源と同じ、つながって
// 燃える炎になる。場所を毎フレーム変えると、炎が 1 フレームごとに別の所へ出て明滅する）。
// 点は受け持つセルの重心へゆっくり寄り、火が広がって点が足りなくなると、1 点が受け持つ半径を広げる。
// 点の足元では、場の霧（Default）を吸収だけの力で焼く。引爆で爆ぜた塊・当たった所の塊も、ここで流体にする。
class BurnFieldVisuals final {
public:
    // 描画フレームごとに呼ぶ。seconds はそのフレームのゲーム時間。
    static void Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BurnFieldComponent& field, float seconds);
};

} // namespace Project_TD2_2
