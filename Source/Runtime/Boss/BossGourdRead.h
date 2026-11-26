#pragma once

#include "HIKARI/Project/PropertyFields.h"

namespace Project_TD2_2 {

struct BossFrame;
struct BossWave;

// ヒョウタンの読み（c-14392d）：同じ側へ転がる癖があれば、横の剣気をその側へ寄せる。入・切と強さはボスの項目
// （調整ガイド g-56cc52）。満杯・空に近い・吸われ続けた・一拍止めの読みは、姿勢で技を選ぶ節と一緒に消した（c-69db00）。
struct BossGourdReadSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 全体の入・切。
    bool enabled = true;
    // 横の剣気の寄せ：最近 leanRolls 回のローリングのうち（leanMinRolls 回以上数えてから）、ボスから見て左か右が
    // leanAtLeastPercent % 以上なら、横の剣気の向きをその側へ leanMeters m（プレイヤーの所で。leanMaxDegrees 度まで）寄せる。
    bool leanEnabled = true;
    int leanRolls = 6;
    int leanMinRolls = 3;
    float leanAtLeastPercent = 60.0f;
    float leanMeters = 2.0f;
    float leanMaxDegrees = 15.0f;
};

class BossGourdRead final {
public:
    // 斬撃を放った：横の剣気なら、ローリングの癖の側へ向きを寄せる。
    static void LeanSlash(const BossFrame& frame, BossWave& wave);
};

} // namespace Project_TD2_2
