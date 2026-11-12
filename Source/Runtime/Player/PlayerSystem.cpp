#include "PlayerSystem.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/FluidView.h"
#include "PlayerRules.h"

namespace Project_TD2_2 {
namespace {

// 競合の優先順位と識別子はゲームの規則であり、見た目の調整値ではない。
constexpr uint64_t kMotionSource = 0x5444325001ull;
constexpr uint64_t kCameraSource = 0x5444325002ull;
constexpr uint64_t kHitStopSource = 0x5444325003ull;
constexpr uint64_t kRollFogInstance = 1;
constexpr int kPlayerPriority = 300;
constexpr float kFramesPerSecond = 60.0f;

using Vec3 = HIKARI::GameProjectFloat3;

bool IsPlaying(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    return world.GetGameSession() != nullptr && !frame.paused;
}

HIKARI::GameProjectInputActionState ReadAction(HIKARI::InputView input, std::string_view id) {
    HIKARI::GameProjectInputActionState action{};
    if (!input.GetAction(id, action)) {
        return {};
    }
    return action;
}

void ReadInput(HIKARI::WorldView& world, PlayerComponent& player) {
    const auto input = world.GetInput();
    HIKARI::GameProjectCameraPose camera{};
    if (world.GetCamera().GetControlPose(camera)) {
        player.cameraForward = PlayerRules::FlatNormalize(
            {camera.target.x - camera.eye.x, 0.0f, camera.target.z - camera.eye.z}, player.cameraForward);
    }
    const auto move = ReadAction(input, "Gameplay.Move");
    const Vec3 right{-player.cameraForward.z, 0.0f, player.cameraForward.x};
    player.moveInput = {right.x * move.x + player.cameraForward.x * move.y, 0.0f,
                        right.z * move.x + player.cameraForward.z * move.y};
    const float magnitude = std::hypot(player.moveInput.x, player.moveInput.z);
    if (magnitude > 1.0f) {
        player.moveInput.x /= magnitude;
        player.moveInput.z /= magnitude;
    }
    if (ReadAction(input, "Gameplay.Dodge").pressed) {
        player.rollBuffer = player.inputBufferFrames;
    }
    if (ReadAction(input, "Gameplay.Heal").pressed) {
        player.healBuffer = player.inputBufferFrames;
    }
    if (ReadAction(input, "Gameplay.LockOn").pressed && PlayerRules::IsAlive(player)) {
        const auto target = world.FindSceneObject(player.lockTargetId);
        player.lockedOn = !player.lockedOn && target.IsValid();
        player.lockTarget = player.lockedOn ? target.GetHandle() : HIKARI::GameProjectObjectHandle{};
    }
}

int StateDuration(const PlayerComponent& player) {
    switch (player.state) {
    case PlayerState::Roll:
        return player.rollFrames;
    case PlayerState::Heal:
        return player.healFrames;
    case PlayerState::Stagger:
        return player.staggerFrames;
    case PlayerState::Knockback:
        return player.knockbackFrames;
    case PlayerState::Down:
        return player.downFrames;
    case PlayerState::GetUp:
        return player.getUpFrames;
    default:
        return 0;
    }
}

void CompleteState(PlayerComponent& player) {
    const int duration = StateDuration(player);
    if (duration <= 0 || player.stateFrame < duration) {
        return;
    }
    const auto next = player.state == PlayerState::Knockback ? PlayerState::Down
                      : player.state == PlayerState::Down    ? PlayerState::GetUp
                                                             : PlayerState::Normal;
    PlayerRules::EnterState(player, next);
}

void StartBufferedAction(PlayerComponent& player) {
    const bool canRoll =
        PlayerRules::CanStartAction(player) ||
        (player.state == PlayerState::Down && player.stateFrame >= std::min(player.downRollFrame, player.downFrames) &&
         player.actionLockFrames == 0);
    if (player.rollBuffer > 0 && canRoll) {
        const bool moving = std::hypot(player.moveInput.x, player.moveInput.z) > 0.0f;
        player.actionDirection =
            moving ? PlayerRules::FlatNormalize(player.moveInput) : Vec3{-player.facing.x, 0.0f, -player.facing.z};
        if (moving && !player.lockedOn) {
            player.facing = player.actionDirection;
        }
        // 回避だけは残量 0 でも開始できる。射撃用の消費関数とは条件が違う。
        player.stamina = std::max(0.0f, player.stamina - player.rollStamina);
        player.staminaAge = 0;
        player.rollBuffer = 0;
        player.rollFogPending = true;
        PlayerRules::EnterState(player, PlayerState::Roll);
    } else if (player.healBuffer > 0 && PlayerRules::CanStartAction(player) && player.healCharges > 0 &&
               player.health < player.maxHealth) {
        --player.healCharges;
        player.healApplied = false;
        player.healBuffer = 0;
        PlayerRules::EnterState(player, PlayerState::Heal);
    }
}

float StaggerStep(const PlayerComponent& player) {
    // 合計時間を短く調整しても三歩を残し、各距離を失わないようにする。
    const int first = std::clamp(player.staggerStepOneFrames, 1, player.staggerFrames - 2);
    const int second = std::clamp(player.staggerStepTwoFrames, 1, player.staggerFrames - first - 1);
    const int third = player.staggerFrames - first - second;
    if (player.stateFrame < first)
        return player.staggerStepOne / static_cast<float>(first);
    if (player.stateFrame < first + second)
        return player.staggerStepTwo / static_cast<float>(second);
    return player.staggerStepThree / static_cast<float>(third);
}

Vec3 StateVelocity(PlayerComponent& player, float dt) {
    if (player.state == PlayerState::Normal && player.actionLockFrames == 0) {
        const float speed = player.moveSpeed * std::clamp(player.externalMoveScale, 0.0f, 1.0f);
        return {player.moveInput.x * speed, 0.0f, player.moveInput.z * speed};
    }
    if (player.state == PlayerState::Heal) {
        if (!player.healApplied && player.stateFrame + 1 >= std::min(player.healApplyFrame, player.healFrames)) {
            player.health = std::min(player.maxHealth, player.health + player.healAmount);
            player.healApplied = true;
        }
        const float speed = player.actionLockFrames == 0 ? player.healMoveSpeed : 0.0f;
        return {player.moveInput.x * speed, 0.0f, player.moveInput.z * speed};
    }
    float distance = 0.0f;
    if (player.state == PlayerState::Roll) {
        const int moveFrames = std::min(player.rollMoveFrames, player.rollFrames);
        if (player.stateFrame < moveFrames)
            distance = player.rollDistance / static_cast<float>(moveFrames);
    } else if (player.state == PlayerState::Stagger) {
        distance = StaggerStep(player);
    } else if (player.state == PlayerState::Knockback) {
        distance = player.knockbackDistance / static_cast<float>(player.knockbackFrames);
    }
    const float speed = distance / dt;
    return {player.actionDirection.x * speed, 0.0f, player.actionDirection.z * speed};
}

void UpdateFacing(HIKARI::WorldView& world, HIKARI::ObjectView object, PlayerComponent& player) {
    if (!PlayerRules::IsAlive(player))
        return;
    if (player.lockedOn) {
        const auto target = world.FindObject(player.lockTarget);
        HIKARI::GameProjectTransform sourceTransform{}, targetTransform{};
        if (target.IsValid() && target.GetWorldTransform(targetTransform) &&
            object.GetWorldTransform(sourceTransform)) {
            player.facing = PlayerRules::FlatNormalize({targetTransform.position.x - sourceTransform.position.x, 0.0f,
                                                        targetTransform.position.z - sourceTransform.position.z},
                                                       player.facing);
        } else {
            player.lockedOn = false;
            player.lockTarget = {};
        }
    } else if (player.state == PlayerState::Normal || player.state == PlayerState::Heal) {
        player.facing = PlayerRules::FlatNormalize(player.moveInput, player.facing);
    }
}

void SubmitMovement(HIKARI::WorldView& world, HIKARI::ObjectView object, PlayerComponent& player,
                    const HIKARI::FrameContext& frame, Vec3 velocity) {
    HIKARI::GameProjectKinematicMove move{};
    move.objectHandle = object.GetHandle().value;
    move.sourceId = kMotionSource;
    move.fixedTickIndex = frame.fixedTickIndex;
    move.priority = kPlayerPriority;
    move.horizontalVelocity = velocity;
    move.maximumFallSpeed = player.maximumFallSpeed;
    const float halfYaw = std::atan2(player.facing.x, player.facing.z) * 0.5f;
    move.hasDesiredRotation = 1;
    move.desiredRotation = {0.0f, std::sin(halfYaw), 0.0f, std::cos(halfYaw)};
    player.motionAccepted = world.GetMotion().SubmitKinematicMove(move);
}

void StepPlayer(HIKARI::WorldView& world, HIKARI::ObjectView object, PlayerComponent& player,
                const HIKARI::FrameContext& frame) {
    PlayerRules::Initialize(player);
    if (player.pendingHitStopFrames > 0) {
        HIKARI::GameProjectTimeDilationRequest request{};
        request.sourceId = kHitStopSource;
        request.priority = kPlayerPriority;
        request.scale = 0.0f;
        request.durationUnscaledSeconds = static_cast<float>(player.pendingHitStopFrames) / kFramesPerSecond;
        player.hitStopAccepted = world.GetTimeDilation().Request(request) != 0;
        player.pendingHitStopFrames = 0;
        SubmitMovement(world, object, player, frame, {});
        return;
    }
    UpdateFacing(world, object, player);
    Vec3 velocity = StateVelocity(player, frame.fixedDt);
    if (PlayerRules::IsAlive(player) && player.recoilFramesLeft > 0) {
        velocity.x += player.recoilPerFrame.x / frame.fixedDt;
        velocity.z += player.recoilPerFrame.z / frame.fixedDt;
        --player.recoilFramesLeft;
    }
    SubmitMovement(world, object, player, frame, velocity);
    if (PlayerRules::IsAlive(player)) {
        if (player.state != PlayerState::Roll && player.staminaAge >= player.staminaDelayFrames) {
            player.stamina = std::min(player.maxStamina, player.stamina + player.staminaPerSecond * frame.fixedDt);
        }
        player.staminaAge = std::min(player.staminaAge + 1, player.staminaDelayFrames);
    }
    player.advanceStateNextTick = StateDuration(player) > 0;
    player.rollBuffer = std::max(0, player.rollBuffer - 1);
    player.healBuffer = std::max(0, player.healBuffer - 1);
    player.actionLockFrames = std::max(0, player.actionLockFrames - 1);
}

} // namespace

void PlayerSystem::Update(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                            [&](HIKARI::ObjectView, PlayerComponent& player) {
                                                PlayerRules::Initialize(player);
                                                ReadInput(world, player);
                                            });
}

void PlayerSystem::PreFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame) || frame.fixedDt <= 0.0f)
        return;
    world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                            [&](HIKARI::ObjectView, PlayerComponent& player) {
                                                PlayerRules::Initialize(player);
                                                // 全 FixedUpdate の前に境界を決め、被弾側の実行順で無敵が一 F
                                                // ずれないようにする。
                                                if (player.advanceStateNextTick)
                                                    ++player.stateFrame;
                                                player.advanceStateNextTick = false;
                                                CompleteState(player);
                                                StartBufferedAction(player);
                                            });
}

void PlayerSystem::FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame) || frame.fixedDt <= 0.0f)
        return;
    world.ForEachComponent<PlayerComponent>(
        HIKARI::MakeComponentTypeId(kPlayerType),
        [&](HIKARI::ObjectView object, PlayerComponent& player) { StepPlayer(world, object, player, frame); });
}

void PlayerSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                            [&](HIKARI::ObjectView, PlayerComponent& player) {
                                                if (!player.lockedOn || !world.FindObject(player.lockTarget).IsValid())
                                                    return;
                                                const auto camera = world.FindSceneObject(player.cameraId);
                                                if (!camera.IsValid()) {
                                                    player.cameraAccepted = false;
                                                    return;
                                                }
                                                HIKARI::GameProjectCameraTargetFramingRequest request{};
                                                request.cameraObjectHandle = camera.GetHandle().value;
                                                request.targetObjectHandle = player.lockTarget.value;
                                                request.sourceId = kCameraSource;
                                                request.priority = kPlayerPriority;
                                                request.targetPivotOffset = {};
                                                request.targetWeight = player.lockTargetWeight;
                                                request.yawTrackingSpeedDegreesPerSecond = player.lockYawSpeed;
                                                request.preferredPitchDegrees = player.lockPitch;
                                                request.pitchTrackingSpeedDegreesPerSecond = player.lockPitchSpeed;
                                                request.distanceScale = player.lockDistanceScale;
                                                request.maximumDistanceOffset = player.lockMaximumDistanceOffset;
                                                player.cameraAccepted = world.GetCamera().SubmitTargetFraming(request);
                                            });
}

void PlayerSystem::PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<PlayerComponent>(
        HIKARI::MakeComponentTypeId(kPlayerType), [&](HIKARI::ObjectView object, PlayerComponent& player) {
            if (!player.rollFogPending)
                return;
            player.rollFogPending = false;
            HIKARI::GameProjectTransform transform{};
            if (!object.GetWorldTransform(transform))
                return;
            HIKARI::GameProjectFluidInteraction impulse{};
            impulse.kind = HIKARI::GameProjectFluidInteractionKind::Impulse;
            impulse.position = transform.position;
            impulse.position.y += player.rollFogHeightOffset;
            impulse.extents = {player.rollFogRadius, 0.0f, 0.0f};
            impulse.direction = player.actionDirection;
            impulse.strength = player.rollFogStrength;
            player.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), kRollFogInstance, impulse).Accepted();
        });
}

} // namespace Project_TD2_2
