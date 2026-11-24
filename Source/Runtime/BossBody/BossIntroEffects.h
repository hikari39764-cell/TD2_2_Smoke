#pragma once

#include "HIKARI/Project/WorldView.h"
#include "../Boss/BossComponent.h"

namespace Project_TD2_2 {

// 登場の効果（c-e0c500）：演出の資産（Sequence）の事件段を効果の拍として受け、小さな効果の部品を鳴らす。部品は
// BossIntroState の頼みの口へ書くだけで、出すのは Feedback（全画面・揺れ・停止・閃光）と BossBodyAccents（気配・衝撃の輪・
// 刀身の光・目の閃き）。フェーズの変わり目の演出（BossPhaseShiftVisuals）も同じ口を使う。地面や画面への予告は出さない。
namespace BossIntroEffects {

// 効果の拍（演出の資産の事件段。名前は clip.name、種類は kind）：入った時・中にいる間（毎フレーム。段の中の秒と
// 段の長さ）・出た時。長さ 0 の段は、入った時と出た時が同じフレームに来る。
void OnCueStart(BossComponent& boss, const BossIntroCue& cue);
void OnCue(BossComponent& boss, const BossIntroCue& cue, float localSeconds, float durationSeconds);
void OnCueEnd(BossComponent& boss, const BossIntroCue& cue);
// 登場を飛ばした時に 1 回（拍ごとの「出た時」は呼ばない）。
void OnSkip(BossComponent& boss);

// 外形を置く所（BossIntroVisuals::ApplyPose）で、描画のフレームごとに呼ぶ：毎フレームの頼み（気配・刃口・刀光）を
// 0 に戻し、遅れて出る衝撃の輪の時計を進め、演技の時計 showSeconds で効果の拍を進める。
void Update(BossComponent& boss, float showSeconds, float seconds);
// 流体への提出の口（PreRender で呼ぶ。登場の効果で流体を使う時はここから出す）。
void Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds);
// 頼まれた手応え（カメラの揺れ・時間の停止・振動・閃光・白黒の閃き）を出す（描画のフレームごと。LateUpdate）。
void Feedback(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float unscaledSeconds);

// 効果の部品（どれも頼みの口へ書くだけ。次の Feedback・BossBodyAccents が出す）。
// 白黒の閃き：強さ（0〜1）・長さ（実時間の秒。反転・白黒・戻りの割合は項目のまま）。0 以下は項目の既定。
void MonoFlash(BossComponent& boss, float strength = 0.0f, float seconds = 0.0f);
// カメラの揺れ：強さ（1 で項目の揺れ幅。1 以上で画角を広げて振動も出す）。
void Shake(BossComponent& boss, float strength);
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
void BladeSwing(BossComponent& boss);
// 目の閃き（0 以下は introEyesFlare）。
void EyeFlare(BossComponent& boss, float strength = 0.0f);

} // namespace BossIntroEffects
} // namespace Project_TD2_2
