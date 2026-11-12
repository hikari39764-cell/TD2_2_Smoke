#pragma once

#include "HIKARI/Project/GameSystem.h"
#include "BossBrain.h"

namespace Project_TD2_2 {

// ボスの本体の規則：表の読み込み、HP・フェーズ・気絶、技の実行、斬撃、体の姿勢、場の霧への煙と霞のエフェクト。
class BossSystem final : public HIKARI::IGameSystem {
public:
    void FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void OnWorldDetached(HIKARI::WorldView& world) override;

private:
    // TODO(c-44ba9d): 行動ツリーの brain を作り、コンポーネントの項目で切り替える。いまは確かめ用だけ。
    BossTestBrain testBrain_;
};

} // namespace Project_TD2_2
