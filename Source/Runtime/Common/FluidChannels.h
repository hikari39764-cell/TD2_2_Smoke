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
// 登場の黒い煙（c-e0c500）：舞台の域（BossIntroSmoke の Intro Smoke Domain。コードからだけ煙を入れる）は Channel5、
// 地を走る煙の域（BossIntroStreak。自分の源だけ）は Channel4 で受ける。前の登場の資産（BossIntroCoil・BossIntroHaze）は
// 同じ通り道だが、もう流していない。
inline constexpr uint32_t kIntroSmoke = 1u << 5u;
inline constexpr uint32_t kIntroStreak = 1u << 4u;
// ボスの胴の煙の域（BossBodySmoke の Body Domain）は Channel6 で受ける。届くのは、フェーズ 2 の襟と袖口の火の舌の
// 注入（BossBodyVisuals）だけ（場の霧・場の火・体の障害物・技の力に崩されない。c-857ae3）。
inline constexpr uint32_t kBodyFire = 1u << 6u;
// 登場の目の炎の域（BossEyeFire。兜の前に付いて動く細かい域）は Channel7 で受ける。届くのは BossIntroSmoke の目の炎の
// 注入だけ。
inline constexpr uint32_t kEyeFire = 1u << 7u;

} // namespace Project_TD2_2::FluidChannels
