#pragma once

#include "BossFrame.h"
#include "BossPose.h"

namespace Project_TD2_2 {

// 跳躍→X字2段・回転→霞の輪の体の動き（試作の preMotion／actMotion／postMotion のうち、この 2 つの型の分）と、
// 霞歩の屈む・跳ぶ・着地に沈む（中身は BossKasumiStep）。剣の振りと斬撃は実行器の共通の流れ（BossMoveExecutor）が出す。
class BossSpecialMoves final {
public:
    // 前隙の 1 tick。progress は前隙の進み（0〜1）。
    static void Pre(BossFrame& frame, const BossMove& move, float progress);
    // 攻撃中の 1 tick。
    static void Act(BossFrame& frame, const BossMove& move, const BossSegmentProgress& g, float progress);
    // 後隙の 1 tick。progress は後隙の進み（0〜1）。
    static void Post(BossComponent& boss, const BossMove& move, float progress);
};

} // namespace Project_TD2_2
