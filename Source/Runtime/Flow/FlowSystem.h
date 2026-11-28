#pragma once

#include "HIKARI/Project/FrameContext.h"
#include "HIKARI/Project/WorldView.h"

#include "FlowComponent.h"

namespace Project_TD2_2::FlowSystem {

// 1 戦の流れ：開始（近づく・最初の一撃）まで待つ → 戦い → 負け（HP 0・落下）か勝ち（ボス HP 0）→ シーンを読み直す。
// シーンに Flow 部品が置いてあればそれを（値の調整用）、無ければ fallback の既定値の流れで、プレイヤーとボスの両方がいる場面だけで
// 動く。値はゲーム全体の値（Params。Player.*・Boss.*）から読み、段階は Fight.State に出す。HUD もここから画面へ出す。
// 旧 IGameSystem の各段の中身。段の名前は旧いまま。HIKARI_SYSTEM（GameLoop.cpp の GameLoop）が、旧い実行順
// （Flow 80・PlayerDebug 90・Player 100・CloudMount 102・GourdShot 105・Gourd 110・GourdFeedback 115・物理 120・Boss 125・
// BurnField 130）と同じ並びで呼ぶ。fallback はシーンに Flow 部品が無い時に使う、世界につき 1 局ぶんの流れ（世界に付くたびに初めから）。
void FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame, Flow& fallback);
void LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame, Flow& fallback);

} // namespace Project_TD2_2::FlowSystem
