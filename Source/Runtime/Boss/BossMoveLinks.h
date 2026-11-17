#pragma once

#include <string>
#include <vector>

#include "BossMoveTable.h"

namespace Project_TD2_2 {

// 技の表の行どうしのつなぎ（コンボ先・構えの途中の変化）。表を読み終えた後に、名前を行の番号へ直す。
class BossMoveLinks final {
public:
    // コンボ先の名前を行につなぐ：完全一致 → 無ければ名前を含む行（自分は除く。合う行は全部）。「/」「、」「・」で
    // 複数書ける（試作と同じ）。「名前:重み」（全角の「：」も可）と書くと、その行の重み（コンボする時に、出せる候補の中から
    // 重みに比例した確率で選ぶ）。書いていなければ 1。
    static void LinkCombos(std::vector<BossMove>& moves, std::vector<std::string>& warnings);
    // 構えの途中の変化：変化先の行を、前隙だけ短くした行として末尾へ足し、元の行の switchMove に入れる。足した行は
    // 行動ツリーの技選びに出ない（コンボ用）。溜めの揺らぎ・崩せる被弾数・変化は持たない（切り替わった後は一気に振る）。
    static void AddSwitches(std::vector<BossMove>& moves, std::vector<std::string>& warnings);
};

} // namespace Project_TD2_2
