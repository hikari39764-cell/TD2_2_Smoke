#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossBodyComponent.h"
#include "../Boss/BossComponent.h"
#include "../Boss/BossIntroComponent.h"

namespace Project_TD2_2 {

// 登場の効果（c-e0c500）：演出の資産（Sequence）の事件段を効果の拍として受け、演出の効果の部品（BossCinematicFx）を
// 鳴らす。部品はボスの部品の頼みの口（Boss::cinematicFxState）へ書くだけで、出すのは BossCinematicFx::Feedback
// （全画面・揺れ・停止・閃光）と BossBodyAccents（気配・衝撃の輪・刀身の光・目の閃き）。フェーズの変わり目の演出
// （BossPhaseShiftVisuals）も同じ口を使う（c-eb3b08 で口を登場からボスの部品へ移した）。地面や画面への予告は出さない。
namespace BossIntroEffects {

// 効果の拍（演出の資産の事件段。名前は clip.name、種類は kind）：入った時・中にいる間（毎フレーム。段の中の秒と
// 段の長さ）・出た時。長さ 0 の段は、入った時と出た時が同じフレームに来る。
void OnCueStart(Boss& boss, BossIntro& intro, const BossIntroCue& cue);
void OnCue(Boss& boss, BossIntro& intro, const BossIntroCue& cue, float localSeconds,
           float durationSeconds);
void OnCueEnd(Boss& boss, BossIntro& intro, const BossIntroCue& cue);
// 登場を飛ばした時に 1 回（拍ごとの「出た時」は呼ばない）。
void OnSkip(BossIntro& intro);

// 体に付く効果の置き場所を決める所（BossIntroVisuals::ApplyPose）で、描画のフレームごとに呼ぶ（毎フレームの頼みは、
// その前に BossCinematicFx::BeginFrame が 0 に戻す）：目の光を決め、遅れて出る衝撃の輪の時計を進め、演技の時計
// showSeconds で効果の拍を進める。
void Update(Boss& boss, BossIntro& intro, float showSeconds, float seconds);
// 流体への提出の口（PreRender で呼ぶ。登場の効果で流体を使う時はここから出す）。body は体の見た目の設定（目の色・刀の長さ。
// 体の部品が無ければ既定の設定）。
void Render(HIKARI::WorldView& world, HIKARI::ObjectView object, Boss& boss, const BossBody& body,
            BossIntro& intro, float seconds);
// 登場だけの手応え（描画のフレームごと。LateUpdate。BossCinematicFx::Feedback の前に呼ぶ）：抜刀の衝撃の輪・剣先の星・
// 見得の張りつめを効果の口へ書き、演出のカメラの姿勢を控え、演出の動きのぼかしを頼む。
void Feedback(HIKARI::WorldView& world, HIKARI::ObjectView object, Boss& boss, const BossBody& body,
              BossIntro& intro, float unscaledSeconds);

} // namespace BossIntroEffects
} // namespace Project_TD2_2
