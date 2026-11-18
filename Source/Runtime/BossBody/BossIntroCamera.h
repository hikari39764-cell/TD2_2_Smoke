#pragma once

#include "HIKARI/Project/WorldView.h"
#include "../Boss/BossComponent.h"

namespace Project_TD2_2 {

class PlayerComponent;

// 登場の演出のカメラ（カード c-cf4ed6）。カットの表（Data/Boss/kasumi_bushi_intro_camera.csv）を、登場の段の進みに
// 合わせて流し、プレイヤーのカメラ（PlayerCamera）へ「演出の姿勢と、ゲームのカメラへ戻す重み」として渡す。
// 姿勢を出すのは PlayerCamera だけ（最後はゲームのカメラへ切れ目なく滑って戻る）。
namespace BossIntroCamera {

// カットの表を読む（登場を始める前に 1 回。読めなければ演出のカメラを出さない）。
void Load(HIKARI::WorldView& world, BossComponent& boss);
// 登場の間、このフレームの演出の姿勢をプレイヤーへ渡す（描画のフレームごと）。
void Update(const BossComponent& boss, PlayerComponent& player);

} // namespace BossIntroCamera
} // namespace Project_TD2_2
