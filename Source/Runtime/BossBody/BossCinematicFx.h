#pragma once

#include "HIKARI/Project/WorldView.h"
#include "../Boss/BossComponent.h"

namespace Project_TD2_2 {

// 演出の効果の部品（c-eb3b08 で登場の効果 BossIntroEffects から分けた）：登場の拍（BossIntroEffects）とフェーズの変わり目の
// 演出（BossPhaseShiftVisuals）が、ボスの部品の頼みの口（BossComponent::cinematicFxState）へ書き、Feedback（全画面・揺れ・
// 停止・閃光）と BossBodyAccents（気配・衝撃の輪・刀身の光・目の閃き）が出す。値の既定は BossComponent::cinematicFx。
// 登場の部品が無くても動く。地面や画面への予告は出さない。
namespace BossCinematicFx {

// 描画のフレームの初めに、毎フレームの頼み（気配・刃口の光・刀光・目の光）を 0 に戻す（BossBodyVisuals が、登場と
// 変わり目の演出が書く前に呼ぶ）。
void BeginFrame(BossComponent& boss);
// 頼まれた手応え（カメラの揺れ・時間の停止・振動・閃光・白黒の閃き・全画面の効果・続けて出す揺れ）を出す（描画の
// フレームごと。LateUpdate。登場の効果の BossIntroEffects::Feedback の後に呼ぶ）。
void Feedback(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float unscaledSeconds);

// 効果の部品（どれも頼みの口へ書くだけ。次の Feedback・BossBodyAccents が出す）。
// 白黒の閃き：強さ（0〜1）・長さ（実時間の秒。反転・白黒・戻りの割合は項目のまま）。0 以下は項目の既定。
void MonoFlash(BossComponent& boss, float strength = 0.0f, float seconds = 0.0f);
// カメラの揺れ：強さ（1 で項目の揺れ幅。1 以上で画角を広げて振動も出す）。
void Shake(BossComponent& boss, float strength);
// 続けて出す画面の揺れ（このフレームだけ。拍が毎フレーム呼ぶ）。Shake は 1 回ごとに揺れの位相が 0 へ戻るので、
// 毎フレーム頼むと揺れずに片側へ寄るだけになる。こちらは位相を自分で進める（Feedback）。
void KeepRumble(BossComponent& boss, float strength);
// 時間を止める (F。間の時間の速さは introHitStopScale)・スロー (F。introDrawSlowScale)。
void HitStop(BossComponent& boss, int frames);
void Slow(BossComponent& boss, int frames);
// 閃光のエフェクト（BossMistBurst）を 1 回：置く所（世界）と大きさ（0 以下は項目の既定）。
void MistBurst(BossComponent& boss, HIKARI::GameProjectFloat3 at, float scale = 0.0f);
// 体を包む気配の強さ（このフレームだけ。毎フレーム頼む）。
void Aura(BossComponent& boss, float strength);
// 地面の衝撃の輪（slot 0・1。置く所・大きさ・出すまでの秒）。
void Shock(BossComponent& boss, int slot, HIKARI::GameProjectFloat3 at, float size, float delaySeconds = 0.0f);
// 刀身を走る光（1 回）・刃口の光の強さと刀光（このフレームだけ）。
void Glint(BossComponent& boss, float brightness);
void BladeEdge(BossComponent& boss, float strength);
// 目の閃き（0 以下は introEyesFlare）。
void EyeFlare(BossComponent& boss, float strength = 0.0f);

} // namespace BossCinematicFx
} // namespace Project_TD2_2
