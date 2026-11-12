#include "PlayerRules.h"

#include <algorithm>
#include <cmath>

namespace Project_TD2_2 {

void PlayerRules::Initialize(PlayerComponent& player) {
    if (player.initialized) {
        return;
    }
    player.health = player.maxHealth;
    player.stamina = player.maxStamina;
    player.healCharges = player.maxHealCharges;
    player.staminaAge = player.staminaDelayFrames;
    player.initialized = true;
}

bool PlayerRules::IsAlive(const PlayerComponent& player) {
    return GetHealth(player) > 0.0f && player.state != PlayerState::Dead;
}

bool PlayerRules::CanStartAction(const PlayerComponent& player) {
    return IsAlive(player) && player.state == PlayerState::Normal && player.actionLockFrames == 0;
}

bool PlayerRules::IsInvulnerable(const PlayerComponent& player) {
    return (player.state == PlayerState::Roll && player.stateFrame < player.rollInvincibleFrames) ||
           player.state == PlayerState::Knockback || player.state == PlayerState::Down;
}

HIKARI::GameProjectFloat3 PlayerRules::FlatNormalize(HIKARI::GameProjectFloat3 value,
                                                     HIKARI::GameProjectFloat3 fallback) {
    const float length = std::hypot(value.x, value.z);
    return length > 0.0f ? HIKARI::GameProjectFloat3{value.x / length, 0.0f, value.z / length} : fallback;
}

void PlayerRules::EnterState(PlayerComponent& player, PlayerState state) {
    player.state = state;
    player.stateFrame = 0;
    player.advanceStateNextTick = false;
}

bool PlayerRules::ApplyDamage(PlayerComponent& player, float amount, HIKARI::GameProjectFloat3 direction,
                              PlayerDamageKind kind) {
    Initialize(player);
    if (!IsAlive(player) || IsInvulnerable(player) || !std::isfinite(amount) || amount <= 0.0f ||
        !std::isfinite(direction.x) || !std::isfinite(direction.z)) {
        return false;
    }
    player.health = std::max(0.0f, player.health - amount);
    player.actionDirection = FlatNormalize(direction, {-player.facing.x, 0.0f, -player.facing.z});
    player.rollBuffer = 0;
    player.healBuffer = 0;
    player.actionLockFrames = 0;
    player.recoilFramesLeft = 0;
    player.pendingHitStopFrames =
        kind == PlayerDamageKind::Knockback ? player.knockbackHitStopFrames : player.staggerHitStopFrames;
    if (player.health <= 0.0f) {
        EnterState(player, PlayerState::Dead);
        player.lockedOn = false;
        player.lockTarget = {};
    } else {
        EnterState(player, kind == PlayerDamageKind::Knockback ? PlayerState::Knockback : PlayerState::Stagger);
    }
    return true;
}

bool PlayerRules::TryConsumeStamina(PlayerComponent& player, float amount, bool allowInsufficient) {
    Initialize(player);
    if (!IsAlive(player) || !std::isfinite(amount) || amount < 0.0f || player.stamina <= 0.0f ||
        (!allowInsufficient && player.stamina < amount)) {
        return false;
    }
    player.stamina = std::max(0.0f, player.stamina - amount);
    player.staminaAge = 0;
    return true;
}

void PlayerRules::LockActions(PlayerComponent& player, int frames) {
    player.actionLockFrames = std::max(player.actionLockFrames, std::max(0, frames));
}

void PlayerRules::SetMovementSpeedScale(PlayerComponent& player, float scale) {
    player.externalMoveScale = std::isfinite(scale) ? std::clamp(scale, 0.0f, 1.0f) : 1.0f;
}

float PlayerRules::GetMovementSpeedScale(const PlayerComponent& player) { return player.externalMoveScale; }

void PlayerRules::AddRecoil(PlayerComponent& player, HIKARI::GameProjectFloat3 direction, float distance, int frames) {
    Initialize(player);
    if (!IsAlive(player) || frames <= 0 || !std::isfinite(distance) || distance < 0.0f || !std::isfinite(direction.x) ||
        !std::isfinite(direction.z)) {
        return;
    }
    const auto unit = FlatNormalize(direction);
    const float step = distance / static_cast<float>(frames);
    player.recoilPerFrame = {unit.x * step, 0.0f, unit.z * step};
    player.recoilFramesLeft = frames;
}

HIKARI::GameProjectFloat3 PlayerRules::GetFacing(const PlayerComponent& player) { return player.facing; }
HIKARI::GameProjectObjectHandle PlayerRules::GetLockTarget(const PlayerComponent& player) { return player.lockTarget; }
float PlayerRules::GetHealth(const PlayerComponent& player) {
    return player.initialized ? player.health : player.maxHealth;
}
float PlayerRules::GetStamina(const PlayerComponent& player) {
    return player.initialized ? player.stamina : player.maxStamina;
}
int PlayerRules::GetHealCharges(const PlayerComponent& player) {
    return player.initialized ? player.healCharges : player.maxHealCharges;
}

} // namespace Project_TD2_2
