#pragma once

#include "DamageableComponent.h"

namespace Project_TD2_2 {

// 参照は取得したクエリの中だけで使う。初期化は最初のダメージ入口で行う。
class DamageableRules final {
public:
    static void Initialize(DamageableComponent& target);
    // direction は攻撃が押す世界方向。有限の零ベクトルは方向なしとして受け付ける。
    // 無敵・死亡・非正の量は拒否し、成功時だけ被弾記録を更新する。
    static bool ApplyDamage(DamageableComponent& target, float amount, HIKARI::GameProjectFloat3 direction);
    static bool IsAlive(const DamageableComponent& target);
    static float GetHealth(const DamageableComponent& target);
    static float GetMaxHealth(const DamageableComponent& target);
    // HP を下回る残量でも、最後のダメージ量には攻撃そのものの量を記録する。
    static float GetLastDamage(const DamageableComponent& target);
    static HIKARI::GameProjectFloat3 GetLastDirection(const DamageableComponent& target);
    static uint64_t GetHitCount(const DamageableComponent& target);
    // 次に受けるダメージにかける倍率（0 以上）。ApplyDamage は amount × 倍率を HP から引く。
    static void SetIncomingDamageScale(DamageableComponent& target, float scale);
    // 倍率をかける前の量で数えた、受け付けたダメージの累計。
    static double GetBaseDamageTotal(const DamageableComponent& target);
};

} // namespace Project_TD2_2
