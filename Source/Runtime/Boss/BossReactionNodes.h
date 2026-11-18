#pragma once

#include "BossBehaviorNodes.h"

namespace Project_TD2_2 {

// 行動ツリーの、プレイヤーへの反応の節：
//   条件：プレイヤーが吸い込み中／プレイヤーが回復中／プレイヤーが火の中／プレイヤーがローリングした直後／距離／
//         燃えている面積／プレイヤーが撃とうとしている／ローリングの癖／この技は咎められている／背後にいる時間／
//         遠くにい続けている／プレイヤーの残り
//   門：反応の間隔（間隔が過ぎていて、確率に当たったら反応の遅れを待って通す）／燃えている面積に比例した確率
//       （中立の技選びの中で使う）
// どの反応をどの技で咎めるかは木に書く（条件 → 反応の間隔 → 技を実行 の Sequence）。コードには反応ごとの決まりを持たない。
// プレイヤーの様子は BossObserver が固定 tick ごとに boss.watch へ書いたものを読む。
class BossReactionNodes final {
public:
    // 節の種類を登録する。user は節の関数に渡る BossBehaviorNodes。
    static bool Register(HIKARI::BEHAVIOR::NodeRegistration& registration, void* user);
    // 技を実行の節が技を頼めた：直前の反応の間隔が通っていれば、その節の間隔をいまから始める。
    static void NoteStarted(BossThought& thought);
    // 技を実行の節が技を頼めなかった：直前の反応の間隔が通った分を取り消す。
    static void NoteFailed(BossThought& thought);
    // プレイヤーに当たった時に呼ぶ：いまの技が反応の枝の技なら、その反応の「当てた時刻」を覚える。
    static void NoteHit(BossComponent& boss);
};

} // namespace Project_TD2_2
