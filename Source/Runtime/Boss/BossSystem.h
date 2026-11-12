#pragma once

#include "HIKARI/Project/GameSystem.h"
#include "BossBrain.h"
#include "BossTreeBrain.h"

namespace Project_TD2_2 {

// ボスの本体の規則：表の読み込み、HP・フェーズ・気絶、技の実行、斬撃、体の姿勢、場の霧への煙と霞のエフェクト。
// どの技を出すかは brain に聞く：ふだんは行動ツリー、確かめ用の項目が入っているボスだけ確かめ用。
class BossSystem final : public HIKARI::IGameSystem {
public:
    void OnWorldAttached(HIKARI::WorldView& world) override;
    void FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void OnWorldDetached(HIKARI::WorldView& world) override;

private:
    BossTestBrain testBrain_;
    BossTreeBrain treeBrain_;
};

} // namespace Project_TD2_2
