#pragma once

#include "HIKARI/Project/GameSystem.h"

namespace Project_TD2_2 {

// 筋斗雲：入力を読み、乗る・降りるを切り替え、騎乗中の運動を運動学の移動として出す（規則は CloudMountRules）。
// プレイヤー（100）の後に動く：PreFixedUpdate で乗る・降りるを決め、同じ tick の PlayerSystem の移動を止める／戻す。
class CloudMountSystem final : public HIKARI::IGameSystem {
public:
    void Update(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void PreFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
};

} // namespace Project_TD2_2
