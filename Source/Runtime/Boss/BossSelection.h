#pragma once

#include "BossComponent.h"

namespace Project_TD2_2 {

// 技の選び方の規則（試作「霞の剣客」の neutralWeights・kickMove・コンボ・連続攻撃と同じ式）。
// いつ・どの候補から選ぶかは行動ツリーが決め、木の節がここで重みと覚えを読み書きする。乱数は呼ぶ側が渡す。
class BossSelection final {
public:
    // 実行器が動かせて、発生条件（フェーズ・HP・距離）を満たす。
    static bool CanPerform(const BossComponent& boss, int row, const BossSituation& situation);
    // 中立の技選びの重み：(1 − |距離重み − いまの距離 ÷ 基準の距離|) を下限で切って指数を掛ける。
    // 直前に始動した技は減らし、待機・移動中に撃たれた分だけ突進系を増やす。始動技でない行・出せない行は 0。
    static float NeutralWeight(const BossComponent& boss, int row, const BossSituation& situation);
    // 連続攻撃への割り込みの重み（候補は発生条件に連続攻撃がある行）。recovering は後隙の間で、
    // いまの技のコンボ先に候補があれば、その行だけを 1、ほかを 0 にする。
    static float InterruptWeight(const BossComponent& boss, int row, const BossSituation& situation, bool recovering);
    // 攻撃中が終わった時のコンボ：つなぐ行（つながなければ -1）。roll・pick は 0 以上 1 未満の乱数。
    static int PickCombo(const BossComponent& boss, const BossSituation& situation, float roll, float pick);
    // いまの技のコンボ先に row がある（後隙の割り込みを、コンボの続きとして数える）。
    static bool IsComboOfCurrent(const BossComponent& boss, int row);
    // seconds 秒以内に hits 回以上当てられていて、反応の間隔も過ぎている。
    static bool IsPressured(const BossComponent& boss, int hits, float seconds);
    // 連続攻撃に反応した：数えた命中を忘れ、cooldownSeconds 秒は反応しない。
    static void UsePressure(BossComponent& boss, float cooldownSeconds);
    // ボスが受けた命中の累計（Damageable の数）を渡す。増えた分を、いまの時刻の命中として覚え、増えた数を返す。
    static uint64_t NoteHits(BossComponent& boss, uint64_t totalHits);
    // 技を始めた（実行器が始めた直後に呼ぶ）。chain は何連目か（0 が始動）。
    static void NoteStarted(BossComponent& boss, int row, int chain);
};

} // namespace Project_TD2_2
