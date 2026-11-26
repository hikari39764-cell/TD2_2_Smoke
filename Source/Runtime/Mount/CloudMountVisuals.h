#pragma once

#include "HIKARI/Project/FrameContext.h"
#include "HIKARI/Project/GameComponent.h"

namespace Project_TD2_2 {

struct CloudMount;
struct Player;

// 筋斗雲の見た目。足元に付いて動く流体の雲のエフェクトを出し、騎乗の段階と飛行の値（mount.flight）から、毎描画
// フレームに変換（足元・向き・旋回の傾き・止まっている時の揺れ）と入力（体の雲の量・尾の量・吹き出し）を送る。
// 規則（CloudMountRules）の値は読むだけで変えない。値は CloudMount の「雲の見た目」。
namespace CloudMountVisuals {

// 描画フレームごとに 1 回呼ぶ（LateUpdate）。body は CloudMount を付けた物体（プレイヤー）。
void Update(HIKARI::WorldView& world, HIKARI::ObjectView body, CloudMount& mount, const Player& player,
            const HIKARI::FrameContext& frame);

} // namespace CloudMountVisuals

} // namespace Project_TD2_2
