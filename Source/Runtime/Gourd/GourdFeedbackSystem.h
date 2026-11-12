#pragma once

#include "HIKARI/Project/GameSystem.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kGourdFeedbackSystem = "Project_TD2_2.GourdFeedbackSystem";

class GourdFeedbackSystem final : public HIKARI::IGameSystem {
  public:
    void LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void PostRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
};

} // namespace Project_TD2_2
