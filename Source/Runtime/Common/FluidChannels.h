#pragma once

#include <cstdint>

#include "HIKARI/Project/FluidTypes.h"

namespace Project_TD2_2::FluidChannels {

// 流体の通り道（FluidView へ出す時の channels）。受ける域の側は、エフェクト資産の流体域の層の channel で決める。
// 場の霧（ArenaGroundFog）は Default、場の火（ArenaFireField）は Channel1 で受ける。
inline constexpr uint32_t kFog = HIKARI::kGameProjectFluidDefaultChannel;
inline constexpr uint32_t kFire = 1u << 1u;

} // namespace Project_TD2_2::FluidChannels
