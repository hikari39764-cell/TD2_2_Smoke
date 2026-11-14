#pragma once

#include "HIKARI/Project/GameSystem.h"

#include "FlowComponent.h"

namespace Project_TD2_2 {

// 1 戦の流れ：開始（近づく・最初の一撃）まで待つ → 戦い → 負け（HP 0・落下）か勝ち（ボス HP 0）→ シーンを読み直す。
// シーンに Flow 部品が置いてあればそれを（値の調整用）、無ければ既定値の部品で、プレイヤーとボスの両方がいる場面だけで動く。
// 値は物体変数（Player.*・Boss.*）から読み、段階は世界変数 Fight.State に出す。HUD もここから画面へ出す。
class FlowSystem final : public HIKARI::IGameSystem {
public:
    void OnWorldAttached(HIKARI::WorldView& world) override;
    void FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override;
    void OnWorldDetached(HIKARI::WorldView& world) override;

private:
    // シーンに Flow 部品が無い時に使う、既定値の流れ。世界につき 1 局ぶんで、世界に付くたびに初めから。
    FlowComponent fallback_;
};

} // namespace Project_TD2_2
