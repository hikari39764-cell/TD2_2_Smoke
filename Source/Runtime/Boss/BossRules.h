#pragma once

#include <string_view>

#include "BossComponent.h"
#include "../Combat/DamageableComponent.h"

namespace Project_TD2_2 {

// ほかの機能（行動ツリー c-44ba9d・HUD など）からボスを使う入口。参照はクエリの中だけで使う。
class BossRules final {
public:
    // 斬撃のエフェクトがいま使っている・すぐ使う流体域の数（砲撃の枠を同じ数だけ譲ってもらう）。
    static size_t GetFluidDomainDemand(const BossComponent& boss);
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
    // 技を出していない（待機中で、要求も無い）。
    static bool IsIdle(const BossComponent& boss);
    static int GetCurrentMove(const BossComponent& boss);
    static BossActPhase GetActPhase(const BossComponent& boss);
    static int GetChain(const BossComponent& boss);
    // フェーズ（1 か 2）。HP の条の何本目か（c-3e0ffa。1 本目が空になった tick から 2）。
    static int GetPhaseNumber(const BossComponent& boss);
    // フェーズ 2 への変わり目の間（1 本目が空になった tick から phaseShiftFrames F。無敵で技を出さない）。
    static bool IsPhaseShifting(const BossComponent& boss);
    static float GetStun(const BossComponent& boss);
    // いま気絶するのに要る気絶値。気絶するたびに stunMax の stunMaxGrowth 倍ずつ増える（崩れた気絶も数える）。
    static float GetStunThreshold(const BossComponent& boss);
    static int GetStunCount(const BossComponent& boss);
    static bool IsStunned(const BossComponent& boss);
    // 剣が燃えているか：火を残す技の間は前隙の最初から、フェーズ 2 では待機中も技の間もいつも。気絶・隠れている間は消える。
    // 剣先の光のエフェクトと、外形の刀身（灼けた刀身への差し替え）が同じ判断を使う。
    static bool IsBladeLit(const BossComponent& boss);
    // ボスの足元（地面の点）と向き。行動ツリーが距離や角度を測るのに使う。
    static HIKARI::GameProjectFloat3 GetGroundPosition(const BossComponent& boss);
    static float GetYaw(const BossComponent& boss);
};

} // namespace Project_TD2_2
