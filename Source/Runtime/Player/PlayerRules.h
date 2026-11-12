#pragma once

#include "PlayerComponent.h"

namespace Project_TD2_2 {

// 呼び出し中に得た PlayerComponent だけを渡す。参照を次フレームへ保持しない。
class PlayerRules final {
public:
    static void Initialize(PlayerComponent& player);
    static bool IsAlive(const PlayerComponent& player);
    static bool CanStartAction(const PlayerComponent& player);
    static bool IsInvulnerable(const PlayerComponent& player);
    // direction は攻撃が押す世界方向。無敵・死亡・無効な量では false。
    static bool ApplyDamage(PlayerComponent& player, float amount, HIKARI::GameProjectFloat3 direction,
                            PlayerDamageKind kind);
    // allowInsufficient は残量が正なら不足分を認める。残量 0 では射撃できない。
    static bool TryConsumeStamina(PlayerComponent& player, float amount, bool allowInsufficient = false);
    static void LockActions(PlayerComponent& player, int frames);
    static void AddRecoil(PlayerComponent& player, HIKARI::GameProjectFloat3 direction, float distance, int frames);
    static HIKARI::GameProjectFloat3 GetFacing(const PlayerComponent& player);
    static HIKARI::GameProjectObjectHandle GetLockTarget(const PlayerComponent& player);
    static float GetHealth(const PlayerComponent& player);
    static float GetStamina(const PlayerComponent& player);
    static int GetHealCharges(const PlayerComponent& player);
    static HIKARI::GameProjectFloat3 FlatNormalize(HIKARI::GameProjectFloat3 value,
                                                   HIKARI::GameProjectFloat3 fallback = {0.0f, 0.0f, 1.0f});
    static void EnterState(PlayerComponent& player, PlayerState state);
};

} // namespace Project_TD2_2
