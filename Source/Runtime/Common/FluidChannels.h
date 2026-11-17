#pragma once

#include <cstdint>

#include "HIKARI/Project/FluidTypes.h"

namespace Project_TD2_2::FluidChannels {

// 流体の通り道（FluidView へ出す時の channels）。受ける域の側は、エフェクト資産の流体域の層の channel で決める。
// 場の霧（ArenaGroundFog）は Default、場の火（ArenaFireField）は Channel1 で受ける。筋斗雲（NimbusCloud）は Channel2 で
// 受け、いまはここから出すものは無い（体の障害物・技の力に押されない）。
inline constexpr uint32_t kFog = HIKARI::kGameProjectFluidDefaultChannel;
inline constexpr uint32_t kFire = 1u << 1u;
// ボスの飛ぶ斬撃のエフェクトの域（BossSlashWave・BossSlashSmoke の Blade Domain）は Channel3 で受ける。場の霧・場の火へ
// 出す注入や力は届かない（残り煙で刃の形が崩れない・口の吸入が斬撃の霧を「吸った霧」に数えない）。届くのは、捕まえた
// 斬撃を飲み込む口の力（GourdAbsorbField::SubmitSwallow）だけ。
inline constexpr uint32_t kSlash = 1u << 3u;

} // namespace Project_TD2_2::FluidChannels
