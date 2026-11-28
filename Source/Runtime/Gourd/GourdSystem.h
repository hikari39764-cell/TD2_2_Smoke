#pragma once

#include "HIKARI/Project/FrameContext.h"
#include "HIKARI/Project/WorldView.h"

namespace Project_TD2_2::GourdSystem {

// 旧 IGameSystem の各段の中身。段の名前は旧いまま。HIKARI_SYSTEM（GameLoop.cpp のシステム）が、旧い実行順
// （Flow 80・PlayerDebug 90・Player 100・CloudMount 102・GourdShot 105・Gourd 110・GourdFeedback 115・物理 120・Boss 125・
// BurnField 130）と同じ並びで呼ぶ。
void Update(HIKARI::WorldView& world, const HIKARI::FrameContext& frame);
void PreFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame);
void PostFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame);
void LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame);
void PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame);
void OnWorldDetached(HIKARI::WorldView& world);

} // namespace Project_TD2_2::GourdSystem
