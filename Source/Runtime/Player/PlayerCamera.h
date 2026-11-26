#pragma once

#include "HIKARI/Project/GameComponent.h"

namespace Project_TD2_2 {

struct Player;

// プレイヤーの後ろから見るカメラ。姿勢（視点と注視点）をゲーム側で作り、毎フレーム Gameplay Camera へ渡す。
// ねらい：酔わないこと。距離は変えない・上下はほとんど追わない・回る時は出だしも止まり際もなめらかにする。
// 値はすべて Player の項目（「カメラ」「ロックオン」）。外部の要求（PlayerRules::RequestCamera。騎乗中など）が
// あるフレームだけ、追い方と見上げ・見下ろしの限度をその値に差し替え、見せ方（距離・画角・傾き・見せるだけの見下ろし）を
// その値へならす。要求が終わると見せ方と限度の外の見上げをならして戻す。姿勢を出すのはここだけ（外部は値を渡すだけ）。
namespace PlayerCamera {

// 描画フレームごとに 1 回呼ぶ。dt はヒットストップで止まらない時間（秒）、alpha は FrameContext::fixedInterpolationAlpha
// （体を、エンジンが描く補間の位置で追う：PlayerRules::PresentedPosition）。
void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, Player& player, float dt, float alpha);

} // namespace PlayerCamera

} // namespace Project_TD2_2
