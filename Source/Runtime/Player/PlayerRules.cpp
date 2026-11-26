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

void PlayerRules::Initialize(Player& player) {
    if (player.initialized) {
        return;
    }
    player.health = player.maxHealth;
    player.stamina = player.maxStamina;
    player.healCharges = player.maxHealCharges;
    player.staminaAge = player.staminaDelayFrames;
    player.initialized = true;
}

PlayerState PlayerRules::GetState(const Player& player) { return player.state; }

bool PlayerRules::IsAlive(const Player& player) {
    return GetHealth(player) > 0.0f && player.state != PlayerState::Dead;
}

bool PlayerRules::CanStartAction(const Player& player) {
    return IsAlive(player) && player.state == PlayerState::Normal && player.actionLockFrames == 0;
}

bool PlayerRules::IsInvulnerable(const Player& player) {
    return player.cinematicHold > 0 ||
           (player.state == PlayerState::Roll && player.stateFrame < player.rollInvincibleFrames) ||
           player.state == PlayerState::Knockback || player.state == PlayerState::Down;
}

HIKARI::GameProjectFloat3 PlayerRules::FlatNormalize(HIKARI::GameProjectFloat3 value,
                                                     HIKARI::GameProjectFloat3 fallback) {
    const float length = std::hypot(value.x, value.z);
    return length > 0.0f ? HIKARI::GameProjectFloat3{value.x / length, 0.0f, value.z / length} : fallback;
}

void PlayerRules::EnterState(Player& player, PlayerState state) {
    player.state = state;
    player.stateFrame = 0;
    player.advanceStateNextTick = false;
}

bool PlayerRules::ApplyDamage(Player& player, float amount, HIKARI::GameProjectFloat3 direction, PlayerDamageKind kind,
                              PlayerDamageType type, float knockbackDistance) {
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

bool PlayerRules::ApplyDamageOverTime(Player& player, float amount) {
    Initialize(player);
    if (!IsAlive(player) || !std::isfinite(amount) || amount <= 0.0f)
        return false;
    player.health = std::max(0.0f, player.health - amount);
    if (player.health <= 0.0f)
        EnterDeath(player);
    return true;
}

bool PlayerRules::IsBurning(const Player& player) { return player.burnRemaining > 0.0f; }

void PlayerRules::Ignite(Player& player, PlayerBurnCause cause, uint64_t tick) {
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

void PlayerRules::Extinguish(Player& player, PlayerBurnEnd reason, uint64_t tick) {
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

void PlayerRules::StepBurn(Player& player, float dt, uint64_t tick) {
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

void PlayerRules::EnterDeath(Player& player) {
    EnterState(player, PlayerState::Dead);
    player.lockedOn = false;
    player.lockTargetHandle = {};
}

bool PlayerRules::TryConsumeStamina(Player& player, float amount, bool allowInsufficient) {
    Initialize(player);
    if (!IsAlive(player) || !std::isfinite(amount) || amount < 0.0f || player.stamina <= 0.0f ||
        (!allowInsufficient && player.stamina < amount)) {
        return false;
    }
    player.stamina = std::max(0.0f, player.stamina - amount);
    player.staminaAge = 0;
    return true;
}

bool PlayerRules::DrainStamina(Player& player, float perSecond, float dt) {
    if (!std::isfinite(perSecond) || !std::isfinite(dt) || perSecond < 0.0f || dt < 0.0f)
        return false;
    return TryConsumeStamina(player, perSecond * dt, true) && player.stamina > 0.0f;
}

void PlayerRules::LockActions(Player& player, int frames) {
    player.actionLockFrames = std::max(player.actionLockFrames, std::max(0, frames));
}

void PlayerRules::ReleaseActionLock(Player& player) { player.actionLockFrames = 0; }

void PlayerRules::SetAimTurn(Player& player, HIKARI::GameProjectFloat3 direction, float degreesPerSecond) {
    if (!std::isfinite(direction.x) || !std::isfinite(direction.z) || !std::isfinite(degreesPerSecond))
        return;
    player.aimTurnPending = true;
    player.aimTurnSpeed = std::max(0.0f, degreesPerSecond);
    player.aimTurnDirection = FlatNormalize(direction, player.facing);
}

void PlayerRules::FaceDirection(Player& player, HIKARI::GameProjectFloat3 direction) {
    if (std::isfinite(direction.x) && std::isfinite(direction.z))
        player.facing = FlatNormalize(direction, player.facing);
}

float PlayerRules::EaseOutFraction(int elapsedFrames, int frames, float easePower) {
    if (frames <= 0)
        return 1.0f;
    const float progress = std::clamp(static_cast<float>(elapsedFrames) / static_cast<float>(frames), 0.0f, 1.0f);
    return 1.0f - std::pow(1.0f - progress, std::max(easePower, 0.01f));
}

void PlayerRules::SetMovementSpeedScale(Player& player, float scale) {
    player.external.moveScale = std::isfinite(scale) ? std::clamp(scale, 0.0f, 1.0f) : 1.0f;
}

void PlayerRules::SetTurnOverride(Player& player, float degreesPerSecond) {
    player.external.turnSpeed = std::isfinite(degreesPerSecond) ? std::max(0.0f, degreesPerSecond) : 0.0f;
}

void PlayerRules::BlockHeal(Player& player) { player.external.healBlocked = true; }

void PlayerRules::AllowWalkDuringAction(Player& player, float scale) {
    SetMovementSpeedScale(player, scale);
    player.external.walkAllowed = true;
}

void PlayerRules::HoldRollInput(Player& player) { player.external.rollGate = PlayerRollGate::Hold; }

void PlayerRules::AllowRollCancel(Player& player) { player.external.rollGate = PlayerRollGate::Cancel; }

bool PlayerRules::TryEndRollForAction(Player& player) {
    if (!IsAlive(player) || player.state != PlayerState::Roll || player.actionLockFrames > 0 ||
        player.stateFrame < player.rollActionCancelFrame)
        return false;
    EnterState(player, PlayerState::Normal);
    return true;
}

void PlayerRules::AddRecoil(Player& player, HIKARI::GameProjectFloat3 direction, float distance, int frames,
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

bool PlayerRules::BeginRide(Player& player) {
    Initialize(player);
    if (!CanStartAction(player))
        return false;
    EnterState(player, PlayerState::Ride);
    player.rollBuffer = 0;
    player.healBuffer = 0;
    player.recoilFramesLeft = 0;
    player.moveVelocity = {};
    return true;
}

void PlayerRules::EndRide(Player& player) {
    if (player.state != PlayerState::Ride)
        return;
    EnterState(player, PlayerState::Normal);
    player.moveVelocity = {};
}

bool PlayerRules::IsRiding(const Player& player) { return player.state == PlayerState::Ride; }

void PlayerRules::StepRide(Player& player) {
    player.rollBuffer = 0;
    player.healBuffer = 0;
    player.moveVelocity = {};
}

void PlayerRules::RequestCamera(Player& player, const PlayerCameraRequest& request) {
    const float values[] = {request.verticalSeconds, request.verticalDeadZone, request.minPitch,
                            request.maxPitch,        request.followSeconds,    request.distance,
                            request.fovDegrees,      request.eyeFloor,         request.fovKickDegrees,
                            request.rollDegrees,     request.lookDownDegrees,  request.shapeSeconds,
                            request.fovSeconds,      request.rollSeconds,      request.releaseSeconds};
    for (const float value : values)
        if (!std::isfinite(value))
            return;
    player.cameraRequest = request;
    player.cameraRequestPending = true;
}

HIKARI::GameProjectFloat3 PlayerRules::GetCameraLook(const Player& player) { return player.cameraLook; }

void PlayerRules::RequestCinematic(Player& player, const PlayerCinematicRequest& request) {
    player.cinematic = request;
    player.cinematicPending = true;
}

void PlayerRules::HoldForCinematic(Player& player, int ticks) {
    player.cinematicHold = std::max(player.cinematicHold, ticks);
}

bool PlayerRules::IsCinematicHeld(const Player& player) { return player.cinematicHold > 0; }

void PlayerRules::SetGroundFire(Player& player, float damagePerSecond) {
    player.burnInGroundFire = true;
    player.burnGroundDamagePerSecond = damagePerSecond;
}

int PlayerRules::GetStateFrame(const Player& player) { return player.stateFrame; }

HIKARI::GameProjectFloat3 PlayerRules::GetActionDirection(const Player& player) { return player.actionDirection; }

HIKARI::GameProjectFloat3 PlayerRules::GetMoveVelocity(const Player& player) { return player.moveVelocity; }

float PlayerRules::GetMaximumFallSpeed(const Player& player) { return player.maximumFallSpeed; }

HIKARI::Entity PlayerRules::GetCamera(const Player& player) { return player.camera; }

double PlayerRules::GetBurnDamageTotal(const Player& player) { return player.burnDamageTotal; }

float PlayerRules::GetBurnRemaining(const Player& player) { return player.burnRemaining; }

uint32_t PlayerRules::GetBurnEndings(const Player& player) { return player.burnEndings; }

PlayerBurnEnd PlayerRules::GetBurnEndReason(const Player& player) { return player.burnEndReason; }

HIKARI::GameProjectFloat3 PlayerRules::GetFacing(const Player& player) { return player.facing; }

HIKARI::GameProjectFloat3 PlayerRules::PresentedPosition(const Player& player, HIKARI::GameProjectFloat3 current,
                                                         float alpha) {
    // 1 tick にこれより離れていたら瞬間移動と見なし、補間しない（エンジンの表示も跳ぶ）。速い急降下でも 1 tick は 1 m 未満。
    constexpr float kTeleportDistance = 5.0f;
    if (!player.tickStartKnown)
        return current;
    const HIKARI::GameProjectFloat3 from = player.tickStartPosition;
    const float dx = current.x - from.x, dy = current.y - from.y, dz = current.z - from.z;
    if (dx * dx + dy * dy + dz * dz > kTeleportDistance * kTeleportDistance)
        return current;
    const float t = std::clamp(alpha, 0.0f, 1.0f);
    return {from.x + dx * t, from.y + dy * t, from.z + dz * t};
}
HIKARI::GameProjectObjectHandle PlayerRules::GetLockTarget(const Player& player) { return player.lockTargetHandle; }
HIKARI::GameProjectObjectHandle PlayerRules::GetLockCandidateTarget(const Player& player) {
    return HIKARI::Extend::HandleOf(player.lockTarget);
}

void PlayerRules::SetLockCandidates(Player& player, const HIKARI::GameProjectObjectHandle* handles, size_t count) {
    player.lockCandidateCount = std::min(count, player.lockCandidates.size());
    std::copy_n(handles, player.lockCandidateCount, player.lockCandidates.begin());
}

size_t PlayerRules::GetLockCandidateCount(const Player& player) { return player.lockCandidateCount; }

HIKARI::GameProjectObjectHandle PlayerRules::GetLockCandidate(const Player& player, size_t index) {
    return index < player.lockCandidateCount ? player.lockCandidates[index] : HIKARI::GameProjectObjectHandle{};
}

void PlayerRules::SetLockTarget(Player& player, HIKARI::GameProjectObjectHandle target) {
    if (player.lockedOn && target.value != 0)
        player.lockTargetHandle = target;
}

bool PlayerRules::TakeLockSwitch(Player& player) {
    const bool requested = player.lockSwitchRequested;
    player.lockSwitchRequested = false;
    return requested;
}

void PlayerRules::SetLockTargetShape(Player& player, float halfHeight) {
    player.lockTargetHalfHeight = std::isfinite(halfHeight) ? std::max(0.0f, halfHeight) : 0.0f;
}

HIKARI::GameProjectFloat3 PlayerRules::LockAimPoint(const Player& player, HIKARI::GameProjectFloat3 origin) {
    return {origin.x, origin.y + player.lockTargetHalfHeight * player.lockAimHeightShare, origin.z};
}

void PlayerRules::ReleaseLock(Player& player) {
    player.lockedOn = false;
    player.lockTargetHandle = {};
    player.lockSwitchRequested = false;
}
float PlayerRules::GetHealth(const Player& player) { return player.initialized ? player.health : player.maxHealth; }
float PlayerRules::GetMaxHealth(const Player& player) { return player.maxHealth; }
float PlayerRules::GetStamina(const Player& player) { return player.initialized ? player.stamina : player.maxStamina; }
float PlayerRules::GetMaxStamina(const Player& player) { return player.maxStamina; }
int PlayerRules::GetHealCharges(const Player& player) {
    return player.initialized ? player.healCharges : player.maxHealCharges;
}

} // namespace Project_TD2_2
