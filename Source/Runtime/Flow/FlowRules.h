#pragma once

#include "FlowComponent.h"

namespace Project_TD2_2 {

// 1 tick の間に見えたもの。値は物体変数と物体の位置から集める（部品の中身は読まない）。
struct FlowObservation {
    bool playerPresent = false;
    bool playerDead = false;
    // プレイヤーの世界の高さ (m)。
    float playerHeight = 0.0f;
    // 筋斗雲に乗っている（飛んでいる間は、始めの高さより下へ行っても落ちたと見ない）。
    bool playerRiding = false;
    // プレイヤーとボスの水平の距離 (m)。どちらかがいなければ負。
    float distanceToBoss = -1.0f;
    bool bossPresent = false;
    float bossHealth = 0.0f;
    float bossMaxHealth = 0.0f;
};

// 流れの規則。参照は呼び出し中だけ使う。
class FlowRules final {
public:
    // 1 固定 tick 進める。段階が変わった時だけ、その理由（ログ用の短い名前）を返す。変わらなければ nullptr。
    static const char* Step(FlowComponent& flow, const FlowObservation& observation);
    // 負け・勝ちの段階で、やり直す時間になったか。
    static bool IsRestartDue(const FlowComponent& flow, float fixedDt);
    // 負け・勝ちの段階の画面の色の濃さ（0〜1。段階に入ってからの時間で決まる）。それ以外の段階は 0。
    static float GetFade(const FlowComponent& flow, float fixedDt);
    static int SecondsToTicks(float seconds, float fixedDt);
};

} // namespace Project_TD2_2
