#pragma once

#include <string_view>

#include "BossFrame.h"

namespace Project_TD2_2 {

// 技が地面に残す火：表の「残す火」「火の幅 m」「火の秒数」の読み方と、どの一撃が・いつ・どこへ塗るか。
// 塗るのは燃える地面の入口（BurnFieldRules）。一撃の番号 strike は 0 から数える（表の「:N」は 1 から）。
class BossBurnMarks final {
public:
    // 「線/池」「交差:2」「輪@フェーズ2」のような列を読み、move.burnMarks へ入れる。読めない項目は行の警告にして入れない。
    // 一撃の数（move.hitCount）を確かめるので、行の区間を読み終えてから呼ぶ。
    static void Read(std::string_view shapes, std::string_view widths, std::string_view seconds, BossMove& move);
    // 一撃 strike に、フェーズ phase で付く shape の火。無ければ nullptr。
    static const BossBurnMark* Find(const BossMove& move, BossBurnShape shape, int strike, int phase);
    // 一撃 strike が、フェーズ phase で何かの火を残すか。
    static bool StrikeLeaves(const BossMove& move, int strike, int phase);
    // 技を始めた：どれかの一撃がいまのフェーズで火を残すなら、技の間ずっと剣が燃えている（boss.bladeLit）。
    static void LightBlade(BossComponent& boss, const BossMove& move);
    // 斬撃・ナイフ・回し蹴りの輪を出した：火の刃か（wave.fire）と、線・輪の塗る幅と秒数を入れる。
    static void ArmWave(const BossComponent& boss, const BossMove& move, BossWave& wave);
    // 一撃を放った（剣を振り抜いた・ナイフを投げた・輪を出した）：交差と池を塗る。霞隠れの池は現れた時に塗る。
    static void Released(BossFrame& frame, const BossMove& move, int strike);
    // 霞隠れで現れた（strike 回目の現れ）：足元に池を塗る。
    static void Appeared(BossFrame& frame, const BossMove& move, int strike);
    // 瞬間突進の瞬間移動を終えた：突進の始まりから終わりまでの床に跡を塗る。
    static void DashLanded(BossFrame& frame, const BossMove& move);
    // 回し蹴りの輪が広がりきった：その半径に帯を塗る（wave.burnWidth が 0 なら塗らない）。
    static void RingEnded(BossFrame& frame, const BossMove& move, const BossWave& wave);
    // 「交差」を待っている斬撃が、狙った所へ届いた・当たった・飛びきった時：そこで爆ぜて、X の火を塗る。
    static void WaveLanded(BossFrame& frame, const BossMove& move, BossWave& wave);
};

} // namespace Project_TD2_2
