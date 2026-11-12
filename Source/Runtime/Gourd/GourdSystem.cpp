#include "GourdSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Runtime/ComponentTypeId.h"
#include "GourdRules.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

using Vec3 = HIKARI::GameProjectFloat3;
// 同じ所有者の吸入口を識別する。外形の基準は正式シーンのメッシュ契約。
constexpr uint64_t kAbsorbInstance = 1;
constexpr float kMeshHeight = 0.5f;
constexpr float kMeshRadius = 0.18f;

bool IsPlaying(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    return world.GetGameSession() != nullptr && !frame.paused;
}

Vec3 Normalize(Vec3 vector, Vec3 fallback) {
    const float length = std::sqrt(vector.x * vector.x + vector.y * vector.y + vector.z * vector.z);
    return length > 0.0f ? Vec3{vector.x / length, vector.y / length, vector.z / length} : fallback;
}

Vec3 Difference(Vec3 a, Vec3 b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }

HIKARI::GameProjectQuaternion PointMouth(Vec3 direction) {
    // 単位カプセルの +Y を向ける最短回転。真下だけは回転軸を明示する。
    if (direction.y <= -1.0f)
        return {1.0f, 0.0f, 0.0f, 0.0f};
    const float w = 1.0f + direction.y;
    const float length = std::sqrt(direction.z * direction.z + direction.x * direction.x + w * w);
    return {direction.z / length, 0.0f, -direction.x / length, w / length};
}

Vec3 CameraForward(HIKARI::WorldView& world, const GourdComponent& gourd, Vec3 fallback) {
    HIKARI::GameProjectCameraPose pose{};
    if (world.GetCamera().GetControlPose(pose))
        return PlayerRules::FlatNormalize(Difference(pose.target, pose.eye), fallback);
    const auto camera = world.FindSceneObject(gourd.cameraId);
    HIKARI::GameProjectTransform transform{};
    if (camera.IsValid() && camera.GetWorldTransform(transform)) {
        const auto q = transform.rotation;
        return PlayerRules::FlatNormalize({2.0f * (q.x * q.z + q.w * q.y), 0.0f, 1.0f - 2.0f * (q.x * q.x + q.y * q.y)},
                                          fallback);
    }
    return fallback;
}

template <class Callback> bool WithPlayer(HIKARI::WorldView& world, const GourdComponent& gourd, Callback&& callback) {
    const auto playerObject = world.FindSceneObject(gourd.playerId);
    if (!playerObject.IsValid())
        return false;
    bool found = false;
    world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                            [&](HIKARI::ObjectView object, PlayerComponent& player) {
                                                if (object.GetHandle().value != playerObject.GetHandle().value)
                                                    return;
                                                found = true;
                                                callback(object, player);
                                            });
    return found;
}

void RefreshPermission(HIKARI::WorldView& world, GourdComponent& gourd, bool applyMovementScale) {
    gourd.absorbing = false;
    gourd.playerValid = WithPlayer(world, gourd, [&](HIKARI::ObjectView, PlayerComponent& player) {
        gourd.absorbing = gourd.enabled && gourd.absorbHeld && PlayerRules::CanStartAction(player);
        if (applyMovementScale && gourd.absorbing)
            PlayerRules::SetMovementSpeedScale(player, gourd.absorbMoveScale);
    });
}

void SpringAxis(float& value, float& velocity, float target, float frequency, float dt) {
    // 臨界減衰の解析解なので、描画のフレーム間隔が変わってもばねが発散しない。
    const float offset = value - target;
    const float impulse = (velocity + frequency * offset) * dt;
    const float decay = std::exp(-frequency * dt);
    value = target + (offset + impulse) * decay;
    velocity = (velocity - frequency * impulse) * decay;
}

void StepRecoil(GourdComponent& gourd, float dt) {
    if (gourd.recoilFramesLeft > 0) {
        gourd.recoilOffset.x += gourd.recoilPerFrame.x;
        gourd.recoilOffset.y += gourd.recoilPerFrame.y;
        gourd.recoilOffset.z += gourd.recoilPerFrame.z;
        --gourd.recoilFramesLeft;
        return;
    }
    // 押し戻す区間は固定 F、回位は既存の追従速度を使った臨界減衰にする。
    SpringAxis(gourd.recoilOffset.x, gourd.recoilVelocity.x, 0.0f, gourd.followFrequency, dt);
    SpringAxis(gourd.recoilOffset.y, gourd.recoilVelocity.y, 0.0f, gourd.followFrequency, dt);
    SpringAxis(gourd.recoilOffset.z, gourd.recoilVelocity.z, 0.0f, gourd.followFrequency, dt);
}

void Follow(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdComponent& gourd, float dt) {
    WithPlayer(world, gourd, [&](HIKARI::ObjectView playerObject, PlayerComponent& player) {
        HIKARI::GameProjectTransform playerTransform{};
        if (!playerObject.GetWorldTransform(playerTransform))
            return;
        const auto facing = PlayerRules::GetFacing(player);
        const Vec3 right{facing.z, 0.0f, -facing.x};
        const auto offset = gourd.shoulderOffset;
        const Vec3 desired{playerTransform.position.x + right.x * offset.x + facing.x * offset.z,
                           playerTransform.position.y + offset.y,
                           playerTransform.position.z + right.z * offset.x + facing.z * offset.z};
        const auto error = Difference(desired, gourd.followPosition);
        const float distanceSquared = error.x * error.x + error.y * error.y + error.z * error.z;
        if (!gourd.poseInitialized || distanceSquared > gourd.teleportDistance * gourd.teleportDistance) {
            gourd.followPosition = desired;
            gourd.followVelocity = {};
            gourd.poseInitialized = true;
        } else {
            SpringAxis(gourd.followPosition.x, gourd.followVelocity.x, desired.x, gourd.followFrequency, dt);
            SpringAxis(gourd.followPosition.y, gourd.followVelocity.y, desired.y, gourd.followFrequency, dt);
            SpringAxis(gourd.followPosition.z, gourd.followVelocity.z, desired.z, gourd.followFrequency, dt);
        }
        gourd.position = {gourd.followPosition.x + gourd.recoilOffset.x, gourd.followPosition.y + gourd.recoilOffset.y,
                          gourd.followPosition.z + gourd.recoilOffset.z};
        Vec3 horizontal = CameraForward(world, gourd, facing);
        const auto lockTarget = world.FindObject(PlayerRules::GetLockTarget(player));
        HIKARI::GameProjectTransform targetTransform{};
        const bool hasLock = lockTarget.IsValid() && lockTarget.GetWorldTransform(targetTransform);
        if (hasLock)
            horizontal = PlayerRules::FlatNormalize(Difference(targetTransform.position, gourd.position), facing);
        gourd.sinkPosition = {gourd.position.x + horizontal.x * gourd.absorbForwardDistance,
                              playerTransform.position.y + gourd.absorbHeightOffset,
                              gourd.position.z + horizontal.z * gourd.absorbForwardDistance};
        gourd.aimDirection = gourd.absorbing ? Normalize(Difference(gourd.sinkPosition, gourd.position), horizontal)
                             : gourd.aimOverridePending ? gourd.aimOverrideDirection
                             : hasLock ? Normalize(Difference(targetTransform.position, gourd.position), horizontal)
                                       : horizontal;
        gourd.mouthPosition = {gourd.position.x + gourd.aimDirection.x * gourd.height * 0.5f,
                               gourd.position.y + gourd.aimDirection.y * gourd.height * 0.5f,
                               gourd.position.z + gourd.aimDirection.z * gourd.height * 0.5f};
        HIKARI::GameProjectTransform transform{};
        transform.position = gourd.position;
        transform.rotation = PointMouth(gourd.aimDirection);
        transform.scale = {gourd.radius / kMeshRadius, gourd.height / kMeshHeight, gourd.radius / kMeshRadius};
        gourd.transformAccepted = object.SetWorldTransform(transform).Accepted();
    });
}

void Trace(HIKARI::ObjectView object, const GourdComponent& gourd) {
    char message[768]{};
    std::snprintf(
        message, sizeof(message),
        "Trace tick=%llu frame=%llu object=%llu smoke=%.6f capacity=%.3f removedTotal=%.9f delta=%.9f "
        "absorbing=%d playerValid=%d position=(%.6f,%.6f,%.6f) sinkPosition=(%.6f,%.6f,%.6f) "
        "mouth=(%.6f,%.6f,%.6f) transformAccepted=%d submitAccepted=%d readAccepted=%d "
        "recoilOffset=(%.6f,%.6f,%.6f) recoilFramesLeft=%d",
        static_cast<unsigned long long>(gourd.processedTick), static_cast<unsigned long long>(gourd.renderFrameCount),
        static_cast<unsigned long long>(object.GetSceneObjectId()), static_cast<double>(GourdRules::GetSmoke(gourd)),
        static_cast<double>(GourdRules::GetCapacity(gourd)), gourd.lastRemovedTotal, gourd.removedDelta,
        static_cast<int>(gourd.absorbing), static_cast<int>(gourd.playerValid), static_cast<double>(gourd.position.x),
        static_cast<double>(gourd.position.y), static_cast<double>(gourd.position.z),
        static_cast<double>(gourd.sinkPosition.x), static_cast<double>(gourd.sinkPosition.y),
        static_cast<double>(gourd.sinkPosition.z), static_cast<double>(gourd.mouthPosition.x),
        static_cast<double>(gourd.mouthPosition.y), static_cast<double>(gourd.mouthPosition.z),
        static_cast<int>(gourd.transformAccepted), static_cast<int>(gourd.fluidAccepted),
        static_cast<int>(gourd.readAccepted), static_cast<double>(gourd.recoilOffset.x),
        static_cast<double>(gourd.recoilOffset.y), static_cast<double>(gourd.recoilOffset.z), gourd.recoilFramesLeft);
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "Gourd", message);
}

} // namespace

void GourdSystem::Update(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    HIKARI::GameProjectInputActionState input{};
    const bool held = world.GetInput().GetAction("Gameplay.Absorb", input) && input.down;
    world.ForEachComponent<GourdComponent>(HIKARI::MakeComponentTypeId(kGourdType),
                                           [&](HIKARI::ObjectView, GourdComponent& gourd) {
                                               GourdRules::Initialize(gourd);
                                               gourd.absorbHeld = held;
                                               RefreshPermission(world, gourd, false);
                                           });
}

void GourdSystem::PreFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<GourdComponent>(HIKARI::MakeComponentTypeId(kGourdType),
                                           [&](HIKARI::ObjectView, GourdComponent& gourd) {
                                               gourd.processedTick = frame.fixedTickIndex;
                                               GourdRules::Initialize(gourd);
                                               RefreshPermission(world, gourd, true);
                                               if (gourd.enabled && frame.fixedDt > 0.0f)
                                                   StepRecoil(gourd, frame.fixedDt);
                                           });
}

void GourdSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<GourdComponent>(HIKARI::MakeComponentTypeId(kGourdType),
                                           [&](HIKARI::ObjectView object, GourdComponent& gourd) {
                                               RefreshPermission(world, gourd, false);
                                               if (gourd.enabled && gourd.playerValid)
                                                   Follow(world, object, gourd, std::max(0.0f, frame.gameDt));
                                               gourd.aimOverridePending = false;
                                           });
}

void GourdSystem::PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<GourdComponent>(HIKARI::MakeComponentTypeId(kGourdType), [&](HIKARI::ObjectView object,
                                                                                        GourdComponent& gourd) {
        const auto fluid = world.GetFluid();
        double total = 0.0;
        gourd.readAccepted = fluid.GetRemovedSmoke(object.GetHandle(), kAbsorbInstance, total).Succeeded();
        // 松開後の遅れた回読も回収する。容量超過時も累計の基準は必ず更新する。
        if (gourd.readAccepted)
            GourdRules::ReceiveRemovedSmoke(gourd, total);
        else
            gourd.removedDelta = 0.0;
        RefreshPermission(world, gourd, false);
        if (gourd.absorbing && gourd.poseInitialized) {
            HIKARI::GameProjectFluidInteraction sink{};
            sink.kind = HIKARI::GameProjectFluidInteractionKind::Force;
            sink.position = gourd.sinkPosition;
            sink.extents = {gourd.absorbRadius, 0.0f, 0.0f};
            sink.strength = -gourd.absorbStrength;
            sink.absorption = gourd.absorption;
            gourd.fluidAccepted = fluid.Submit(object.GetHandle(), kAbsorbInstance, sink).Accepted();
        }
        ++gourd.renderFrameCount;
        if (gourd.traceEveryFrames > 0 && gourd.renderFrameCount % static_cast<uint64_t>(gourd.traceEveryFrames) == 0)
            Trace(object, gourd);
    });
}

} // namespace Project_TD2_2
