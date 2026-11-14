#pragma once

#include "BossFrame.h"

namespace Project_TD2_2 {

// 炎走り（型 flameRun）：前隙で剣を後ろへ引きずって溜め（プレイヤーの方を向き続ける）、攻撃中に斬り上げるたびに、
// ボスの足元から前へ地を走る炎の前線を 1 本ずつ放つ。前線は扇に並び（1 本目を放つ時の向きが真ん中）、左の端から右へ
// 順に放つ。前線は表の斬撃速度でまっすぐ走り、通った所にいるプレイヤーに 1 本につき 1 回当て、後ろに「線」の火を残す。
// 前線の様子は boss.flameRun に毎 tick 書く（エフェクトが読む）。地面や画面の予告は出さない（剣の構えで読ませる）。
class BossFlameRun final {
public:
    // 技を始めた tick（どの型でも呼ぶ）。炎走りなら、いまのフェーズで放つ数を決め、その分だけ攻撃中を延ばす
    // （boss.actFrames ＝ 表の攻撃中＋（数−1）×flameRunIntervalFrames）。走っている前線は止めない。
    static void Begin(BossComponent& boss, const BossMove& move);
    // 攻撃中の 1 tick：放つ tick なら次の前線を放つ。1 本目を放つ tick に扇の真ん中の向きを決める。
    static void Act(BossFrame& frame, const BossMove& move);
    // 技が打ち切られた（気絶・崩れ・撃破）：まだ放っていない前線はもう放たない。放った前線は走り切る。
    static void Cut(BossComponent& boss);
    // 走っている前線を 1 tick 進めて当て、火を塗る。技の外・気絶の間も進めるので、BossSystem が毎 tick 呼ぶ。
    static void Step(BossFrame& frame);
};

} // namespace Project_TD2_2
