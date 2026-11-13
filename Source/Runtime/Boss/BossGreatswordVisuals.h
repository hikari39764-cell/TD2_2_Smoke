#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"

namespace Project_TD2_2 {

// 霞の大剣の見た目：大剣の模型の輪郭へ流体を流し込むエフェクトを、握りの位置と剣の向きに合わせて持って行き、
// 薙ぎ終わった瞬間に剣が通った扇の場の霧を外へ押す。規則が書く boss.greatsword を読むだけ（規則は変えない）。
// 状態は boss.greatswordVisual に持つ。
class BossGreatswordVisuals final {
public:
    static void Update(HIKARI::WorldView& world, BossComponent& boss, float seconds);
    static void Stop(HIKARI::WorldView& world, BossComponent& boss);
};

} // namespace Project_TD2_2
