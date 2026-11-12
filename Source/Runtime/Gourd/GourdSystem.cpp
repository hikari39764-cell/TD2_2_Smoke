#include "GourdSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Runtime/ComponentTypeId.h"
#include "GourdAbsorbField.h"
#include "GourdAbsorbVisuals.h"
#include "GourdRules.h"
#include "../Common/VectorMath.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

using VectorMath::Difference;
using VectorMath::Normalize;
using Vec3 = HIKARI::GameProjectFloat3;
// 外形の基準は正式シーンのメッシュ契約。
constexpr float kMeshHeight = 0.5f;
constexpr float kMeshRadius = 0.18f;
constexpr float kTau = 6.28318530718f;
// 吸えている速さをならす時間 (秒)。回読は 1〜3 フレーム遅れて塊で届くので、そのままでは演出がちらつく。
constexpr float kIntakeSmoothSeconds = 0.2f;

bool IsPlaying(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    return world.GetGameSession() != nullptr && !frame.paused;
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

void EnterAbsorbPhase(GourdComponent& gourd, GourdAbsorbPhase phase) {
    gourd.absorbPhase = phase;
    gourd.absorbPhaseFrames = 0;
}

// 固定 tick ごとに一度だけ進める。構え → 吸う → 後隙。被弾などで通常の姿勢でなくなったら、後隙なしで打ち切る。
void StepAbsorb(HIKARI::WorldView& world, GourdComponent& gourd) {
    gourd.playerValid = WithPlayer(world, gourd, [&](HIKARI::ObjectView, PlayerComponent& player) {
        const bool usable = gourd.enabled && PlayerRules::IsAlive(player) && player.state == PlayerState::Normal;
        if (!usable)
            EnterAbsorbPhase(gourd, GourdAbsorbPhase::Idle);
        switch (gourd.absorbPhase) {
        case GourdAbsorbPhase::Idle:
            if (usable && gourd.absorbHeld && PlayerRules::CanStartAction(player)) {
                EnterAbsorbPhase(gourd, gourd.absorbWindupFrames > 0 ? GourdAbsorbPhase::Windup
                                                                      : GourdAbsorbPhase::Absorbing);
                // 構えが明ける tick の Player::PreFixedUpdate が先に走るので、その境界までロックをつなぐ。
                if (gourd.absorbWindupFrames > 0)
                    PlayerRules::LockActions(player, gourd.absorbWindupFrames + 1);
            }
            break;
        case GourdAbsorbPhase::Windup:
            if (++gourd.absorbPhaseFrames >= gourd.absorbWindupFrames) {
                EnterAbsorbPhase(gourd, gourd.absorbHeld ? GourdAbsorbPhase::Absorbing : GourdAbsorbPhase::Recovery);
                if (gourd.absorbPhase == GourdAbsorbPhase::Recovery)
                    PlayerRules::LockActions(player, gourd.absorbRecoveryFrames);
            }
            break;
        case GourdAbsorbPhase::Absorbing:
            if (!gourd.absorbHeld) {
                EnterAbsorbPhase(gourd, GourdAbsorbPhase::Recovery);
                PlayerRules::LockActions(player, gourd.absorbRecoveryFrames);
            }
            break;
        case GourdAbsorbPhase::Recovery:
            if (++gourd.absorbPhaseFrames >= gourd.absorbRecoveryFrames)
                EnterAbsorbPhase(gourd, GourdAbsorbPhase::Idle);
            break;
        }
        if (gourd.absorbPhase == GourdAbsorbPhase::Absorbing) {
            // ほぼ動けないが、入力した方向へ向きは変えられる。吸う向きはこの向きについて行く。
            PlayerRules::SetMovementSpeedScale(player, gourd.absorbMoveScale);
            PlayerRules::SetTurnOverride(player, gourd.absorbTurnSpeed);
            PlayerRules::BlockActions(player);
        }
    });
    if (!gourd.playerValid)
        EnterAbsorbPhase(gourd, GourdAbsorbPhase::Idle);
    gourd.absorbing = gourd.absorbPhase == GourdAbsorbPhase::Absorbing;
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
        // 吸い込み（構え・後隙も）の口は、カメラやロック対象ではなくプレイヤーの向きへ向ける。
        const bool absorbPose = gourd.absorbPhase != GourdAbsorbPhase::Idle;
        if (absorbPose)
            horizontal = facing;
        else if (hasLock)
            horizontal = PlayerRules::FlatNormalize(Difference(targetTransform.position, gourd.position), facing);
        gourd.sinkPosition = {gourd.position.x + horizontal.x * gourd.absorbForwardDistance,
                              playerTransform.position.y + gourd.absorbHeightOffset,
                              gourd.position.z + horizontal.z * gourd.absorbForwardDistance};
        // 吸っている間の口は、前方 gatherRange 先の足元へ向ける。口のエフェクト（渦の円錐）の軸になる。
        const Vec3 coneEnd{gourd.position.x + horizontal.x * gourd.gatherRange, gourd.sinkPosition.y,
                           gourd.position.z + horizontal.z * gourd.gatherRange};
        gourd.aimDirection = gourd.absorbing            ? Normalize(Difference(coneEnd, gourd.position), horizontal)
                             : absorbPose               ? horizontal
                             : gourd.aimOverridePending ? gourd.aimOverrideDirection
                             : hasLock ? Normalize(Difference(targetTransform.position, gourd.position), horizontal)
                                       : horizontal;
        gourd.mouthPosition = {gourd.position.x + gourd.aimDirection.x * gourd.height * 0.5f,
                               gourd.position.y + gourd.aimDirection.y * gourd.height * 0.5f,
                               gourd.position.z + gourd.aimDirection.z * gourd.height * 0.5f};
        HIKARI::GameProjectTransform transform{};
        transform.position = gourd.position;
        transform.rotation = VectorMath::RotationFromYAxis(gourd.aimDirection);
        // 吸い込み中は脈を打つように膨らむ（見た目だけ。口の位置と吸い込みの規則は変えない）。
        const float pulse =
            1.0f + gourd.absorbPulseScale * gourd.absorbBlend *
                       (0.5f + 0.5f * std::sin(kTau * gourd.absorbPulseFrequency * gourd.absorbSeconds));
        transform.scale = {gourd.radius / kMeshRadius * pulse, gourd.height / kMeshHeight,
                           gourd.radius / kMeshRadius * pulse};
        gourd.transformAccepted = object.SetWorldTransform(transform).Accepted();
    });
}

void Trace(HIKARI::ObjectView object, const GourdComponent& gourd) {
    char message[768]{};
    std::snprintf(
        message, sizeof(message),
        "Trace tick=%llu frame=%llu object=%llu smoke=%.6f capacity=%.3f "
        "removedTotal=%.9f delta=%.9f "
        "absorbing=%d playerValid=%d position=(%.6f,%.6f,%.6f) "
        "sinkPosition=(%.6f,%.6f,%.6f) "
        "mouth=(%.6f,%.6f,%.6f) transformAccepted=%d submitAccepted=%d "
        "readAccepted=%d "
        "recoilOffset=(%.6f,%.6f,%.6f) recoilFramesLeft=%d fluidSubmissions=%u "
        "gatherStrength=%.3f intake=%.3f",
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
        static_cast<double>(gourd.recoilOffset.y), static_cast<double>(gourd.recoilOffset.z), gourd.recoilFramesLeft,
        gourd.fluidSubmissions, static_cast<double>(gourd.gatherStrength), static_cast<double>(gourd.intakeRate));
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
                                           });
}

void GourdSystem::PreFixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<GourdComponent>(HIKARI::MakeComponentTypeId(kGourdType),
                                           [&](HIKARI::ObjectView, GourdComponent& gourd) {
                                               gourd.processedTick = frame.fixedTickIndex;
                                               GourdRules::Initialize(gourd);
                                               StepAbsorb(world, gourd);
                                               if (gourd.enabled && frame.fixedDt > 0.0f)
                                                   StepRecoil(gourd, frame.fixedDt);
                                           });
}

void GourdSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<GourdComponent>(HIKARI::MakeComponentTypeId(kGourdType),
                                           [&](HIKARI::ObjectView object, GourdComponent& gourd) {
                                               if (gourd.enabled && gourd.playerValid)
                                                   Follow(world, object, gourd, std::max(0.0f, frame.gameDt));
                                               gourd.aimOverridePending = false;
                                               GourdAbsorbVisuals::Update(world, object, gourd, frame);
                                           });
}

void GourdSystem::PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    world.ForEachComponent<GourdComponent>(HIKARI::MakeComponentTypeId(kGourdType), [&](HIKARI::ObjectView object,
                                                                                        GourdComponent& gourd) {
        const auto fluid = world.GetFluid();
        double total = 0.0;
        gourd.readAccepted =
            fluid.GetRemovedSmoke(object.GetHandle(), GourdAbsorbField::SinkInstance(), total).Succeeded();
        // 松開後の遅れた回読も回収する。容量超過時も累計の基準は必ず更新する。
        if (gourd.readAccepted)
            GourdRules::ReceiveRemovedSmoke(gourd, total);
        else
            gourd.removedDelta = 0.0;
        if (frame.gameDt > 0.0f) {
            const float rate =
                static_cast<float>(gourd.removedDelta) * GourdRules::GetSmokePerVolume(gourd) / frame.gameDt;
            gourd.intakeRate += (rate - gourd.intakeRate) * (1.0f - std::exp(-frame.gameDt / kIntakeSmoothSeconds));
        }
        gourd.fluidAccepted = true;
        gourd.fluidSubmissions = 0;
        if (gourd.absorbing && gourd.poseInitialized)
            GourdAbsorbField::Submit(world, object, gourd, gourd.absorbSeconds);
        ++gourd.renderFrameCount;
        if (gourd.traceEveryFrames > 0 && gourd.renderFrameCount % static_cast<uint64_t>(gourd.traceEveryFrames) == 0)
            Trace(object, gourd);
    });
}

void GourdSystem::OnWorldDetached(HIKARI::WorldView& world) {
    world.ForEachComponent<GourdComponent>(
        HIKARI::MakeComponentTypeId(kGourdType),
        [&](HIKARI::ObjectView, GourdComponent& gourd) { GourdAbsorbVisuals::Stop(world, gourd); });
}

} // namespace Project_TD2_2
