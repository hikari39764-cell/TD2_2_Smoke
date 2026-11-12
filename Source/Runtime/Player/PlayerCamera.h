#pragma once

#include "HIKARI/Project/GameComponent.h"

namespace Project_TD2_2 {

class PlayerComponent;

// プレイヤーの後ろから見るカメラ。姿勢（視点と注視点）をゲーム側で作り、毎フレーム Gameplay Camera へ渡す。
// ねらい：酔わないこと。距離は変えない・上下はほとんど追わない・回る時は出だしも止まり際もなめらかにする。
// 値はすべて Player の項目（「カメラ」「ロックオン」）。
namespace PlayerCamera {

// 描画フレームごとに 1 回呼ぶ。dt はヒットストップで止まらない時間（秒）。
void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, PlayerComponent& player, float dt);

} // namespace PlayerCamera

} // namespace Project_TD2_2
