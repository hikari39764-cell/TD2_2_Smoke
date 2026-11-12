#pragma once

#include "BossFrame.h"

namespace Project_TD2_2 {

// 引爆（型 detonate）：前隙で溜め、攻撃中に足元から外へ前線を広げて、前線が通った燃えている地面を爆ぜさせる。
// 前線が通ったセルを消す・プレイヤーが近いかを見る規則は燃える地面の側（BurnFieldRules::Explode）にあり、
// ここは時間の進め方と、プレイヤーへ当てること（ほかの攻撃と同じ BossWaves::HitPlayer）だけを持つ。
class BossDetonate final {
public:
    // 前隙の 1 tick。progress は前隙の進み（0〜1）。溜めの進みを燃える地面へ渡す。
    static void Charge(BossFrame& frame, float progress);
    // 攻撃中に入った tick（判定区間の始め）：前線の中心を決め、燃えている所を数える。
    static void Begin(BossFrame& frame);
    // 攻撃中の 1 tick：前線を斬撃速度で広げ、通ったセルを爆ぜさせる。火を通り過ぎたら、攻撃中をこの tick で終える。
    static void Step(BossFrame& frame, const BossMove& move);
    // 毎固定 tick、崩れ・気絶の判定の後：溜めをやめた（崩された・打ち切られた・爆ぜ始めた）なら溜めを 0 に戻し、
    // 爆ぜている途中で打ち切られた引爆を終える（残った火はそのまま残す）。
    static void Sync(BossFrame& frame);
};

} // namespace Project_TD2_2
