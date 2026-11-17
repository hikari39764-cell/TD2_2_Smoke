#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"

namespace Project_TD2_2 {

// ボスの霞のエフェクト（流体注入の資産）を出す・動かす・止める。規則（固定 tick）が決めた斬撃と剣の位置を見せるだけ。
class BossVisuals final {
public:
    // alpha は FrameContext::fixedInterpolationAlpha（引き込まれる斬撃・ナイフを、tick の間を補間した位置に置く）。
    static void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds,
                       float alpha);
    static void Stop(HIKARI::WorldView& world, BossComponent& boss);
};

} // namespace Project_TD2_2
