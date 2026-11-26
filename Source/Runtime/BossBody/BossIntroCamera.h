#pragma once

#include "../Boss/BossIntroComponent.h"

namespace Project_TD2_2 {

struct Player;

// 登場の演出の後の、ゲームのカメラへの渡し。演技の後の一息の段の間、演出の資産が最後に見せていたカメラの姿勢から、
// ゲームのカメラへ滑らかに混ぜる重みをプレイヤーのカメラ（PlayerCamera）へ渡す。姿勢を出すのは PlayerCamera だけ。
namespace BossIntroCamera {

// 一息の段の間、このフレームの渡しの姿勢をプレイヤーへ渡す（描画のフレームごと）。
void Update(const BossIntroComponent& intro, Player& player);

} // namespace BossIntroCamera
} // namespace Project_TD2_2
