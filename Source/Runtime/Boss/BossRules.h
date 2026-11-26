#pragma once

#include <string_view>

#include "BossComponent.h"

namespace Project_TD2_2 {

// ほかの機能（行動ツリー c-44ba9d・HUD など）からボスを使う入口。参照はクエリの中だけで使う。
class BossRules final {
public:
    // 表の index 行の技を出すよう頼む。待機中なら次の固定 tick から始める。
    // asCombo は「いまの技の攻撃中が終わった時点で、後隙を出さずに続ける」（コンボ）。
    // 後隙の間に頼むと、後隙を切り上げて始める（asCombo ならコンボの続きとして数える）。
    // 範囲外の行・実行器が動かせない型の行・撃破された後は false。
    static bool RequestMove(BossComponent& boss, int index, bool asCombo = false);
    static bool RequestMoveByName(BossComponent& boss, std::string_view name, bool asCombo = false);
    static void CancelRequest(BossComponent& boss);
    // 戦いが始まるまで、ボスの行動を止める（技を頼まない・歩かない）。止めていない時は何も変わらない。
    static void SetHeld(BossComponent& boss, bool held);
    static int FindMove(const BossComponent& boss, std::string_view name);
    static int GetMoveCount(const BossComponent& boss);
    // 読んだ表の 1 行（無ければ nullptr）。
    static const BossMove* GetMove(const BossComponent& boss, int index);
    static BossState GetState(const BossComponent& boss);
    // HP の条に出している HP とその最大（いまのフェーズの条。固定 tick の値。条を用意する前は 0）。
    static float GetShownHealth(const BossComponent& boss);
    static float GetShownMaxHealth(const BossComponent& boss);
    static int GetCurrentMove(const BossComponent& boss);
    static int GetChain(const BossComponent& boss);
    // フェーズ 2 への変わり目の間（1 本目が空になった tick から phaseShiftFrames F。無敵で技を出さない）。
    static bool IsPhaseShifting(const BossComponent& boss);
    // いま気絶するのに要る気絶値。気絶するたびに stunMax の stunMaxGrowth 倍ずつ増える（崩れた気絶も数える）。
    static float GetStunThreshold(const BossComponent& boss);
    // 剣が燃えているか：火を残す技の間は前隙の最初から、フェーズ 2 では待機中も技の間もいつも。気絶・隠れている間は消える。
    // 剣先の光のエフェクトと、刀に付くエフェクト（燃える刀身）が同じ判断を使う。
    static bool IsBladeLit(const BossComponent& boss);
};

} // namespace Project_TD2_2
