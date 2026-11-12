#pragma once

#include "HIKARI/Project/WorldView.h"

namespace Project_TD2_2 {

class FeedbackCamera final {
  public:
    static uint64_t Resolve(HIKARI::WorldView& world, uint64_t preferredSceneObjectId = 0);
};

} // namespace Project_TD2_2
