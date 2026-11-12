#include "PlayerDebugSystem.h"

#include <cstdio>

#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Runtime/ComponentTypeId.h"

#include "PlayerDebugComponent.h"
#include "PlayerRules.h"

namespace Project_TD2_2 {
namespace {

const char* StateName(PlayerState state) {
    switch (state) {
    case PlayerState::Normal:
        return "Normal";
    case PlayerState::Roll:
        return "Roll";
    case PlayerState::Heal:
        return "Heal";
    case PlayerState::Stagger:
        return "Stagger";
    case PlayerState::Knockback:
        return "Knockback";
    case PlayerState::Down:
        return "Down";
    case PlayerState::GetUp:
        return "GetUp";
    case PlayerState::Dead:
        return "Dead";
    }
    return "Unknown";
}

bool AtTick(uint64_t tick, int scheduledTick) {
    return scheduledTick >= 0 && tick == static_cast<uint64_t>(scheduledTick);
}

bool IsDamageScheduled(const PlayerDebugComponent& debug) {
    if (debug.damageAtTick < 0 || debug.damageAttempts >= debug.damageRepeatCount ||
        (debug.damageAttempts > 0 && debug.damageRepeatInterval <= 0)) {
        return false;
    }
    const uint64_t nextTick =
        static_cast<uint64_t>(debug.damageAtTick) +
        static_cast<uint64_t>(debug.damageAttempts) * static_cast<uint64_t>(debug.damageRepeatInterval);
    return debug.elapsedTicks == nextTick;
}

void ApplyScheduledActions(PlayerComponent& player, PlayerDebugComponent& debug) {
    PlayerRules::Initialize(player);
    char message[256]{};
    if (IsDamageScheduled(debug)) {
        const bool hit = PlayerRules::ApplyDamage(player, debug.damageAmount, debug.damageDirection, debug.damageKind);
        ++debug.damageAttempts;
        std::snprintf(message, sizeof(message), "Damage tick=%llu amount=%.3f kind=%s accepted=%d health=%.3f",
                      static_cast<unsigned long long>(debug.elapsedTicks), static_cast<double>(debug.damageAmount),
                      debug.damageKind == PlayerDamageKind::Stagger ? "Stagger" : "Knockback", static_cast<int>(hit),
                      static_cast<double>(player.health));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "PlayerDebug", message);
    }
    if (AtTick(debug.elapsedTicks, debug.consumeStaminaAtTick)) {
        const bool accepted =
            PlayerRules::TryConsumeStamina(player, debug.consumeStaminaAmount, debug.consumeAllowInsufficient);
        std::snprintf(message, sizeof(message), "ConsumeStamina tick=%llu amount=%.3f accepted=%d stamina=%.3f",
                      static_cast<unsigned long long>(debug.elapsedTicks),
                      static_cast<double>(debug.consumeStaminaAmount), static_cast<int>(accepted),
                      static_cast<double>(player.stamina));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "PlayerDebug", message);
    }
    if (AtTick(debug.elapsedTicks, debug.lockAtTick)) {
        PlayerRules::LockActions(player, debug.lockFrames);
    }
    if (AtTick(debug.elapsedTicks, debug.recoilAtTick)) {
        PlayerRules::AddRecoil(player, debug.recoilDirection, debug.recoilDistance, debug.recoilFrames);
    }
}

void TracePlayer(HIKARI::ObjectView object, const PlayerComponent& player, const PlayerDebugComponent& debug) {
    HIKARI::GameProjectTransform transform{};
    const bool positionValid = object.GetWorldTransform(transform);
    char message[768]{};
    std::snprintf(
        message, sizeof(message),
        "Trace tick=%llu object=%llu state=%s stateFrame=%d health=%.3f stamina=%.3f healCharges=%d invulnerable=%d "
        "positionValid=%d position=(%.6f,%.6f,%.6f) motionAccepted=%d cameraAccepted=%d "
        "fluidAccepted=%d hitStopAccepted=%d actionLockFrames=%d recoilFramesLeft=%d",
        static_cast<unsigned long long>(debug.processedTick),
        static_cast<unsigned long long>(object.GetSceneObjectId()), StateName(player.state), player.stateFrame,
        static_cast<double>(player.health), static_cast<double>(player.stamina), player.healCharges,
        static_cast<int>(PlayerRules::IsInvulnerable(player)), static_cast<int>(positionValid),
        static_cast<double>(transform.position.x), static_cast<double>(transform.position.y),
        static_cast<double>(transform.position.z), static_cast<int>(player.motionAccepted),
        static_cast<int>(player.cameraAccepted), static_cast<int>(player.fluidAccepted),
        static_cast<int>(player.hitStopAccepted), player.actionLockFrames, player.recoilFramesLeft);
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "PlayerDebug", message);
}

} // namespace

void PlayerDebugSystem::FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (world.GetGameSession() == nullptr || frame.paused) {
        return;
    }
    world.ForEachComponent<PlayerDebugComponent>(
        HIKARI::MakeComponentTypeId(kPlayerDebugType), [&](HIKARI::ObjectView owner, PlayerDebugComponent& debug) {
            if (!debug.enabled) {
                return;
            }
            world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                                    [&](HIKARI::ObjectView object, PlayerComponent& player) {
                                                        if (object.GetHandle() == owner.GetHandle()) {
                                                            ApplyScheduledActions(player, debug);
                                                        }
                                                    });
            debug.processedTick = debug.elapsedTicks;
            ++debug.elapsedTicks;
        });
}

void PlayerDebugSystem::PostFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (world.GetGameSession() == nullptr || frame.paused) {
        return;
    }
    world.ForEachComponent<PlayerDebugComponent>(
        HIKARI::MakeComponentTypeId(kPlayerDebugType), [&](HIKARI::ObjectView owner, PlayerDebugComponent& debug) {
            if (!debug.enabled || debug.traceEveryFrames <= 0 ||
                debug.processedTick % static_cast<uint64_t>(debug.traceEveryFrames) != 0) {
                return;
            }
            world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                                    [&](HIKARI::ObjectView object, PlayerComponent& player) {
                                                        if (object.GetHandle() == owner.GetHandle()) {
                                                            TracePlayer(object, player, debug);
                                                        }
                                                    });
        });
}

} // namespace Project_TD2_2
