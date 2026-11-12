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
    // 範囲外の行・このカードの実行器が動かせない型（跳躍・回し蹴り・霞隠れ・分身）は false。
    static bool RequestMove(BossComponent& boss, int index, bool asCombo = false);
    static bool RequestMoveByName(BossComponent& boss, std::string_view name, bool asCombo = false);
    static void CancelRequest(BossComponent& boss);
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
    // フェーズ（1 か 2）。HP の持ち主は Damageable なので、その割合から決める。
    static int GetPhaseNumber(const BossComponent& boss, const DamageableComponent& health);
    static float GetStun(const BossComponent& boss);
    static int GetStunCount(const BossComponent& boss);
    static bool IsStunned(const BossComponent& boss);
    // ボスの足元（地面の点）と向き。行動ツリーが距離や角度を測るのに使う。
    static HIKARI::GameProjectFloat3 GetGroundPosition(const BossComponent& boss);
    static float GetYaw(const BossComponent& boss);
};

} // namespace Project_TD2_2
