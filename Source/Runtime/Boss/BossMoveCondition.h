#pragma once

#include <string_view>

namespace Project_TD2_2 {

// 表の「発生条件」を読んだ結果。読み方は試作の parseCond と同じ：
// 「フェーズ2以上」「フェーズ1以下」「フェーズ2のみ」「HP60%以下」「HP30%以上」「距離8m以内」「距離12m以上」「連続攻撃」。
// いくつか書いてあれば、全部を満たす時に出せる。
struct BossMoveCondition {
    // 負は「条件なし」。
    int phaseAtLeast = -1;
    int phaseAtMost = -1;
    int phaseOnly = -1;
    double healthAtMost = -1.0;
    double healthAtLeast = -1.0;
    double distanceWithin = -1.0;
    double distanceBeyond = -1.0;
    // 連続攻撃された時に出す技（割り込みの候補）。いつ出すかは行動ツリーが決める。
    bool pressure = false;
    // 何か書いてあるのに、読める条件が 1 つも無かった。「常に」として扱う。
    bool unreadable = false;
};

// 条件を比べる相手：いまのフェーズ、HP（%）、プレイヤーまでの水平距離（m。プレイヤーがいなければ負）。
// HP は倍精度で持つ（単精度だと 900 / 1500 × 100 が 60 をわずかに超え、ちょうど 60% の条件が外れる）。
struct BossSituation {
    int phase = 1;
    double healthPercent = 100.0;
    float distance = -1.0f;
};

BossMoveCondition ParseBossMoveCondition(std::string_view text);
// フェーズ・HP・距離の条件を全部満たすか（連続攻撃は見ない）。
bool BossMoveConditionHolds(const BossMoveCondition& condition, const BossSituation& situation);

} // namespace Project_TD2_2
