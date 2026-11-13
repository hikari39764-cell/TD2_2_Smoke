#pragma once

#include "BossFrame.h"
#include "BossPose.h"

namespace Project_TD2_2 {

// 跳躍→X字2段・回転→霞の輪・霞隠れ→回り込み突進・霞分身の体の動き（試作の preMotion／actMotion／segEnter／
// ambushMotion／postMotion のうち、この 4 つの型の分）と、引爆の時間の進め方（中身は BossDetonate）、
// 天降り斬りの跳ぶ・溜める・落ちる・着地（中身は BossSkyfall）、退き溜めの退く・溜める。
// 剣の振りと斬撃は実行器の共通の流れ（BossMoveExecutor）が出す。
class BossSpecialMoves final {
public:
    // 前隙の 1 tick。progress は前隙の進み（0〜1）。
    static void Pre(BossFrame& frame, const BossMove& move, float progress);
    // 攻撃中の 1 tick。entered はこの tick に新しい区間へ入った（霞隠れの隠れる・現れる）。
    static void Act(BossFrame& frame, const BossMove& move, const BossSegmentProgress& g, bool entered, float progress);
    // 剣を振らない型の判定区間の最初の tick に一度（回し蹴りの輪を放つ、分身を出す、引爆の前線を出す、天降り斬りの着地）。
    static void Trigger(BossFrame& frame, const BossMove& move);
    // 後隙の 1 tick。progress は後隙の進み（0〜1）。
    static void Post(BossComponent& boss, const BossMove& move, float progress);
    // 退き溜めの溜めを終える（state はログの言葉：放った release／打ち切られた broken）。溜めていなければ何もしない。
    static void EndRetreatCharge(BossComponent& boss, const char* state);
    // 霞の爆ぜ：ground（地面の点）に、押す力の無い小さな爆発の見た目を出す。residue なら吸える残り煙（puffResidue）も置く
    // （試作では霞隠れの隠れる・現れると、消えた分身だけが残り煙を置く。分身を出す時は見た目だけ）。
    static void Puff(BossComponent& boss, HIKARI::GameProjectFloat3 ground, bool residue = true);
};

} // namespace Project_TD2_2
