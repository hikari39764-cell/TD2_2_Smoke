#pragma once

#include "../Boss/BossPhaseShiftComponent.h"

namespace Project_TD2_2 {

struct Player;

// フェーズの変わり目の演出の後の、ゲームのカメラへの渡し（c-271cba。登場の BossIntroCamera と同じやり方）。渡しの段の間、
// 演出の資産が最後に見せていたカメラの姿勢から、ゲームのカメラへ滑らかに混ぜる重みをプレイヤーのカメラへ渡す。
namespace BossPhaseShiftCamera {

// 渡しの段の間、このフレームの渡しの姿勢をプレイヤーへ渡す（描画のフレームごと）。
void Update(const BossPhaseShiftComponent& shift, Player& player);

} // namespace BossPhaseShiftCamera
} // namespace Project_TD2_2
