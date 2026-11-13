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
    // 押された向きの反対（攻撃した相手の方）を向いて、のけぞり・吹っ飛ぶ。
    player.facing = {-player.actionDirection.x, 0.0f, -player.actionDirection.z};
    player.rollBuffer = 0;
    player.healBuffer = 0;
    player.actionLockFrames = 0;
    player.recoilFramesLeft = 0;
    player.pendingHitStopFrames =
        kind == PlayerDamageKind::Knockback ? player.knockbackHitStopFrames : player.staggerHitStopFrames;
    if (player.health <= 0.0f) {
        EnterDeath(player);
    } else {
        EnterState(player, kind == PlayerDamageKind::Knockback ? PlayerState::Knockback : PlayerState::Stagger);
    }
    return true;
}

bool PlayerRules::ApplyDamageOverTime(PlayerComponent& player, float amount) {
    Initialize(player);
    if (!IsAlive(player) || IsInvulnerable(player) || !std::isfinite(amount) || amount <= 0.0f)
        return false;
    player.health = std::max(0.0f, player.health - amount);
    if (player.health <= 0.0f)
        EnterDeath(player);
    return true;
}

void PlayerRules::EnterDeath(PlayerComponent& player) {
    EnterState(player, PlayerState::Dead);
    player.lockedOn = false;
    player.lockTarget = {};
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

void PlayerRules::ReleaseActionLock(PlayerComponent& player) { player.actionLockFrames = 0; }

void PlayerRules::SetAimTurn(PlayerComponent& player, HIKARI::GameProjectFloat3 direction, float degreesPerSecond) {
    if (!std::isfinite(direction.x) || !std::isfinite(direction.z) || !std::isfinite(degreesPerSecond))
        return;
    player.aimTurnPending = true;
    player.aimTurnSpeed = std::max(0.0f, degreesPerSecond);
    player.aimTurnDirection = FlatNormalize(direction, player.facing);
}

void PlayerRules::FaceDirection(PlayerComponent& player, HIKARI::GameProjectFloat3 direction) {
    if (std::isfinite(direction.x) && std::isfinite(direction.z))
        player.facing = FlatNormalize(direction, player.facing);
}

float PlayerRules::EaseOutFraction(int elapsedFrames, int frames, float easePower) {
    if (frames <= 0)
        return 1.0f;
    const float progress = std::clamp(static_cast<float>(elapsedFrames) / static_cast<float>(frames), 0.0f, 1.0f);
    return 1.0f - std::pow(1.0f - progress, std::max(easePower, 0.01f));
}

void PlayerRules::SetMovementSpeedScale(PlayerComponent& player, float scale) {
    player.externalMoveScale = std::isfinite(scale) ? std::clamp(scale, 0.0f, 1.0f) : 1.0f;
}

float PlayerRules::GetMovementSpeedScale(const PlayerComponent& player) { return player.externalMoveScale; }

void PlayerRules::SetTurnOverride(PlayerComponent& player, float degreesPerSecond) {
    player.externalTurnSpeed = std::isfinite(degreesPerSecond) ? std::max(0.0f, degreesPerSecond) : 0.0f;
}

void PlayerRules::BlockHeal(PlayerComponent& player) { player.externalHealBlock = true; }

void PlayerRules::AllowWalkDuringAction(PlayerComponent& player, float scale) {
    SetMovementSpeedScale(player, scale);
    player.externalWalkAllowed = true;
}

void PlayerRules::HoldRollInput(PlayerComponent& player) { player.externalRollGate = PlayerRollGate::Hold; }

void PlayerRules::AllowRollCancel(PlayerComponent& player) { player.externalRollGate = PlayerRollGate::Cancel; }

bool PlayerRules::TryEndRollForAction(PlayerComponent& player) {
    if (!IsAlive(player) || player.state != PlayerState::Roll || player.actionLockFrames > 0 ||
        player.stateFrame < player.rollActionCancelFrame)
        return false;
    EnterState(player, PlayerState::Normal);
    return true;
}

void PlayerRules::AddRecoil(PlayerComponent& player, HIKARI::GameProjectFloat3 direction, float distance, int frames,
                            float easePower) {
    Initialize(player);
    if (!IsAlive(player) || frames <= 0 || !std::isfinite(distance) || distance < 0.0f || !std::isfinite(direction.x) ||
        !std::isfinite(direction.z) || !std::isfinite(easePower)) {
        return;
    }
    player.recoilDirection = FlatNormalize(direction);
    player.recoilDistance = distance;
    player.recoilFrames = frames;
    player.recoilEasePower = std::max(easePower, 0.01f);
    player.recoilFramesLeft = frames;
}

HIKARI::GameProjectFloat3 PlayerRules::GetFacing(const PlayerComponent& player) { return player.facing; }
HIKARI::GameProjectObjectHandle PlayerRules::GetLockTarget(const PlayerComponent& player) { return player.lockTarget; }
uint64_t PlayerRules::GetLockCandidateId(const PlayerComponent& player) { return player.lockTargetId; }

void PlayerRules::SetLockTarget(PlayerComponent& player, HIKARI::GameProjectObjectHandle target) {
    if (player.lockedOn && target.value != 0)
        player.lockTarget = target;
}

HIKARI::GameProjectFloat3 PlayerRules::GetLockPivotOffset(const PlayerComponent& player) {
    return player.lockPivotOffset;
}

bool PlayerRules::TakeLockSwitch(PlayerComponent& player) {
    const bool requested = player.lockSwitchRequested;
    player.lockSwitchRequested = false;
    return requested;
}

void PlayerRules::SetLockPivotOffset(PlayerComponent& player, HIKARI::GameProjectFloat3 offset) {
    if (std::isfinite(offset.x) && std::isfinite(offset.y) && std::isfinite(offset.z))
        player.lockPivotOffset = offset;
}
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
