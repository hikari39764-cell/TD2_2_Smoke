#include "PlayerRules.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ProjectLog.h"

namespace Project_TD2_2 {
namespace {

// 1/60 秒を引き続けた時の丸め誤差の余裕 (秒)。
constexpr float kBurnTimeSlack = 1e-4f;

const char* BurnCauseKey(PlayerBurnCause cause) {
    switch (cause) {
    case PlayerBurnCause::Slash: return "slash";
    case PlayerBurnCause::Explosion: return "explosion";
    default: return "ground";
    }
}

const char* BurnEndKey(PlayerBurnEnd reason) {
    switch (reason) {
    case PlayerBurnEnd::Roll: return "roll";
    case PlayerBurnEnd::Absorb: return "absorb";
    case PlayerBurnEnd::Timeout: return "timeout";
    case PlayerBurnEnd::Death: return "death";
    default: return "none";
    }
}

} // namespace

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
                              PlayerDamageKind kind, PlayerDamageType type, float knockbackDistance) {
    Initialize(player);
    if (!IsAlive(player) || IsInvulnerable(player) || !std::isfinite(amount) || amount <= 0.0f ||
        !std::isfinite(direction.x) || !std::isfinite(direction.z)) {
        return false;
    }
    player.health = std::max(0.0f, player.health - amount);
    player.lastHitType = type;
    ++player.hitCount;
    player.actionDirection = FlatNormalize(direction, {-player.facing.x, 0.0f, -player.facing.z});
    // 押された向きの反対（攻撃した相手の方）を向いて、のけぞり・吹っ飛ぶ。
    player.facing = {-player.actionDirection.x, 0.0f, -player.actionDirection.z};
    player.rollBuffer = 0;
    player.healBuffer = 0;
    player.actionLockFrames = 0;
    player.recoilFramesLeft = 0;
    player.pendingHitStopFrames =
        kind == PlayerDamageKind::Knockback ? player.knockbackHitStopFrames : player.staggerHitStopFrames;
    player.pendingHitFeedback = true;
    player.pendingHitFeedbackKnockback = kind == PlayerDamageKind::Knockback;
    player.knockbackDistanceNow =
        std::isfinite(knockbackDistance) && knockbackDistance > 0.0f ? knockbackDistance : player.knockbackDistance;
    if (player.health <= 0.0f) {
        EnterDeath(player);
    } else {
        EnterState(player, kind == PlayerDamageKind::Knockback ? PlayerState::Knockback : PlayerState::Stagger);
    }
    return true;
}

bool PlayerRules::ApplyDamageOverTime(PlayerComponent& player, float amount) {
    Initialize(player);
    if (!IsAlive(player) || !std::isfinite(amount) || amount <= 0.0f)
        return false;
    player.health = std::max(0.0f, player.health - amount);
    if (player.health <= 0.0f)
        EnterDeath(player);
    return true;
}

bool PlayerRules::IsBurning(const PlayerComponent& player) { return player.burnRemaining > 0.0f; }

void PlayerRules::Ignite(PlayerComponent& player, PlayerBurnCause cause, uint64_t tick) {
    Initialize(player);
    if (!IsAlive(player))
        return;
    const bool wasBurning = IsBurning(player);
    player.burnRemaining = player.burnSeconds;
    if (wasBurning)
        return;
    player.burnElapsed = 0.0f;
    player.burnDamage = 0.0f;
    player.burnCause = cause;
    ++player.burnIgnitions;
    char message[160]{};
    std::snprintf(message, sizeof(message), "BurnStart tick=%llu cause=%s health=%.3f",
                  static_cast<unsigned long long>(tick), BurnCauseKey(cause), static_cast<double>(player.health));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "Player", message);
}

void PlayerRules::Extinguish(PlayerComponent& player, PlayerBurnEnd reason, uint64_t tick) {
    if (!IsBurning(player))
        return;
    player.burnRemaining = 0.0f;
    player.burnEndReason = reason;
    ++player.burnEndings;
    char message[192]{};
    std::snprintf(message, sizeof(message), "BurnEnd tick=%llu reason=%s seconds=%.3f damage=%.3f health=%.3f",
                  static_cast<unsigned long long>(tick), BurnEndKey(reason), static_cast<double>(player.burnElapsed),
                  static_cast<double>(player.burnDamage), static_cast<double>(GetHealth(player)));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "Player", message);
}

void PlayerRules::StepBurn(PlayerComponent& player, float dt, uint64_t tick) {
    // 燃える地面の知らせはこの tick だけ使う（燃える地面が無くなっても、次の tick に地面の減り方を残さない）。
    const bool inFire = player.burnInGroundFire;
    const float ground = player.burnGroundDamagePerSecond;
    player.burnInGroundFire = false;
    player.burnGroundDamagePerSecond = 0.0f;
    if (!IsBurning(player))
        return;
    if (!IsAlive(player)) {
        Extinguish(player, PlayerBurnEnd::Death, tick);
        return;
    }
    // 火の中にいる間は地面の減り方で減り、残りは満タンのまま（離れてから burnSeconds で消える）。二つは重ねない。
    const float before = player.health;
    if (ApplyDamageOverTime(player, (inFire ? ground : player.burnDamagePerSecond) * dt)) {
        player.burnDamage += before - player.health;
        player.burnDamageTotal += static_cast<double>(before - player.health);
    }
    player.burnElapsed += dt;
    if (!IsAlive(player)) {
        Extinguish(player, PlayerBurnEnd::Death, tick);
        return;
    }
    if (inFire) {
        player.burnRemaining = player.burnSeconds;
        return;
    }
    // 1/60 秒を足し引きした丸め誤差で、burnSeconds ちょうどの tick が 1 つ余分に燃えないようにする。
    player.burnRemaining -= dt;
    if (player.burnRemaining <= kBurnTimeSlack)
        Extinguish(player, PlayerBurnEnd::Timeout, tick);
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
    player.external.moveScale = std::isfinite(scale) ? std::clamp(scale, 0.0f, 1.0f) : 1.0f;
}

void PlayerRules::SetTurnOverride(PlayerComponent& player, float degreesPerSecond) {
    player.external.turnSpeed = std::isfinite(degreesPerSecond) ? std::max(0.0f, degreesPerSecond) : 0.0f;
}

void PlayerRules::BlockHeal(PlayerComponent& player) { player.external.healBlocked = true; }

void PlayerRules::AllowWalkDuringAction(PlayerComponent& player, float scale) {
    SetMovementSpeedScale(player, scale);
    player.external.walkAllowed = true;
}

void PlayerRules::HoldRollInput(PlayerComponent& player) { player.external.rollGate = PlayerRollGate::Hold; }

void PlayerRules::AllowRollCancel(PlayerComponent& player) { player.external.rollGate = PlayerRollGate::Cancel; }

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

void PlayerRules::SetLockCandidates(PlayerComponent& player, const HIKARI::GameProjectObjectHandle* handles,
                                    size_t count) {
    player.lockCandidateCount = std::min(count, player.lockCandidates.size());
    std::copy_n(handles, player.lockCandidateCount, player.lockCandidates.begin());
}

size_t PlayerRules::GetLockCandidateCount(const PlayerComponent& player) { return player.lockCandidateCount; }

HIKARI::GameProjectObjectHandle PlayerRules::GetLockCandidate(const PlayerComponent& player, size_t index) {
    return index < player.lockCandidateCount ? player.lockCandidates[index] : HIKARI::GameProjectObjectHandle{};
}

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
