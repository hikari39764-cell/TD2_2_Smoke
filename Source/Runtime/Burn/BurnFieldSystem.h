#pragma once

#include "HIKARI/Project/FrameContext.h"
#include "HIKARI/Project/WorldView.h"

namespace Project_TD2_2::BurnFieldSystem {

// 燃える地面の規則：火の残り時間を減らす、火の上に立つプレイヤーの HP を減らす、吸い込みの扇の火を消して煙にする。
// 同じ tick のボス（斬撃が火を塗る）の後に動く。描画フレームでは、燃えているセルを火の流体として見せる（BurnFieldVisuals）。
// 旧 IGameSystem の各段の中身。段の名前は旧いまま。HIKARI_SYSTEM（GameLoop.cpp の GameLoop）が、旧い実行順
// （Flow 80・PlayerDebug 90・Player 100・CloudMount 102・GourdShot 105・Gourd 110・GourdFeedback 115・物理 120・Boss 125・
// BurnField 130）と同じ並びで呼ぶ。
void FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame);
void PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame);

} // namespace Project_TD2_2::BurnFieldSystem
