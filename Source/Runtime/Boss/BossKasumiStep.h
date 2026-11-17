#pragma once

#include <string_view>

#include "BossFrame.h"

namespace Project_TD2_2 {

// 霞歩（横っ飛び）の規則：行き先の決め方（戦場の縁・燃えている地面・プレイヤーの近くを避ける、弾の線から離れる側）、
// 跳ぶ体の動き（前隙で屈む → 攻撃中に弧を描いて跳ぶ → 後隙で着地に沈む）、跳ぶ向きへの体の傾き、弾をかわす霞歩の
// 当たらない F と、続けてかわせる回数（使い切るとしばらくかわさない）。値はボスの項目 kasumiStep と技の表の行。
class BossKasumiStep final {
public:
    // 表の「跳ぶ向き」「跳ぶ距離 m」「跳ぶ高さ m」を読む（型が霞歩の行だけ）。読めない値は move.warnings へ。
    static void Read(std::string_view directions, std::string_view distance, std::string_view height, BossMove& move);
    // 技を始めた tick：向きと行き先を決める。弾をかわす霞歩なら、続けてかわした回数に数える。
    static void Begin(BossFrame& frame, const BossMove& move);
    // 前隙・攻撃中・後隙の 1 tick（progress はそれぞれの進み 0〜1）。
    static void Pre(BossComponent& boss, float progress);
    static void Act(BossFrame& frame, float progress);
    static void Post(BossComponent& boss, float progress);
    // いま弾が当たらない（弾をかわす霞歩の跳び始めの kasumiStepGuardFrames F）。
    static bool Guarding(const BossComponent& boss);
    // 跳ぶ向きへの体の傾き（rad）を lean（前後。正で前）・sway（横。体の傾きの規則と同じ向き）へ足す。霞歩の外は足さない。
    static void AddLean(const BossComponent& boss, float& lean, float& sway);
    // 弾をかわせる（続けてかわせる回数を使い切った後の、かわさない間でない）。
    static bool CanDodge(const BossComponent& boss);
    // かわさない間（息が切れている。胴の煙が薄くなる）。
    static bool Spent(const BossComponent& boss);
    // 霞歩の行をいま出せるか（かわさない間は様子見の霞歩も出さない設定なら、出せない）。霞歩でない行は true。
    static bool Allows(const BossComponent& boss, const BossMove& move);
    static const char* DirectionKey(BossStepDirection direction);
};

} // namespace Project_TD2_2
