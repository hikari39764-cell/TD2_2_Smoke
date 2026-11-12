#pragma once

#include "GourdShotComponent.h"

namespace Project_TD2_2 {

class GourdShotVisuals final {
  public:
    // 規則側（固定 tick）から、一度だけ鳴らすエフェクトを予約する。direction は砲口の資産の +X を向ける世界方向。
    static void QueueBurst(GourdShotComponent& shot, bool muzzle, HIKARI::GameProjectFloat3 position,
                           HIKARI::GameProjectFloat3 direction, float scale);
    static void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot,
                       uint32_t& sourceCount);
    static void Render(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot,
                       uint32_t& fluidCount);
    static void Stop(HIKARI::WorldView& world, GourdShotComponent& shot);
};

} // namespace Project_TD2_2
