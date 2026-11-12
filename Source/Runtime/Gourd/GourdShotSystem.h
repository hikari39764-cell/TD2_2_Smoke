#pragma once

#include "HIKARI/Project/GameSystem.h"

namespace Project_TD2_2 {

class GourdShotSystem final : public HIKARI::IGameSystem {
public:
    void Update(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void PreFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void OnWorldDetached(HIKARI::WorldView& world) override;
};

} // namespace Project_TD2_2
