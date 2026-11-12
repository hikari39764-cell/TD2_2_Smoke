#pragma once

#include "HIKARI/Project/GameSystem.h"

namespace Project_TD2_2 {

// 燃える地面の規則：火の残り時間を減らす、火の上に立つプレイヤーの HP を減らす、吸い込みの扇の火を消して煙にする。
// 同じ tick のボス（斬撃が火を塗る）の後に動く。描画フレームでは、燃えているセルを火の流体として見せる（BurnFieldVisuals）。
class BurnFieldSystem final : public HIKARI::IGameSystem {
public:
    void FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
};

} // namespace Project_TD2_2
