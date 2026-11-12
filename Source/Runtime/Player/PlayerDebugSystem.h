#pragma once

#include "HIKARI/Project/GameSystem.h"

namespace Project_TD2_2 {

class PlayerDebugSystem final : public HIKARI::IGameSystem {
public:
    void FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void PostFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
};

} // namespace Project_TD2_2
