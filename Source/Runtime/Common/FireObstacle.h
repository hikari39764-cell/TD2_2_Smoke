#pragma once

#include <algorithm>
#include <cstdint>

#include "FluidChannels.h"
#include "HIKARI/Project/FluidView.h"

namespace Project_TD2_2::FireObstacle {

// 場の火（Channel1）の域にだけ効く、立っているカプセルの障害物。火の中を通ると、炎が体をよけて割れ、体の動きに
// 引かれる。場の霧の側はオブジェクトの FluidObstacleComponent が受け持つ（シーンの部品は Default にしか届かない）。
// radius が 0 以下なら出さない。高さは足元から頭まで（オブジェクトの位置がその中心）。
inline bool Submit(HIKARI::WorldView& world, HIKARI::ObjectView object, uint64_t instanceId, float radius,
                   float height) {
    HIKARI::GameProjectTransform transform{};
    if (!(radius > 0.0f) || !object.GetWorldTransform(transform))
        return true;
    HIKARI::GameProjectFluidInteraction obstacle{};
    obstacle.kind = HIKARI::GameProjectFluidInteractionKind::Obstacle;
    obstacle.shape = HIKARI::GameProjectFluidShape::Capsule;
    obstacle.position = transform.position;
    obstacle.extents = {radius, std::max(0.0f, height * 0.5f - radius), 0.0f};
    obstacle.channels = FluidChannels::kFire;
    return world.GetFluid().Submit(object.GetHandle(), instanceId, obstacle).Accepted();
}

} // namespace Project_TD2_2::FireObstacle
