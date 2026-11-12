#pragma once

#include "HIKARI/Project/GameSystem.h"

namespace Project_TD2_2 {

class DamageableSystem final : public HIKARI::IGameSystem {
  public:
    void LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void PostRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void OnWorldDetached(HIKARI::WorldView& world) override;
};

} // namespace Project_TD2_2
