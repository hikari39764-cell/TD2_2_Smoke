#include "PlayerCamera.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/ProjectLog.h"
#include "PlayerComponent.h"
#include "PlayerRules.h"
#include "../Common/VectorMath.h"

namespace Project_TD2_2 {
namespace {

// 競合の優先順位と識別子はゲームの規則であり、見た目の調整値ではない。エンジンの追従カメラ（優先度 0）より上。
constexpr uint64_t kPoseSource = 0x5444325002ull;
constexpr int kPosePriority = 300;
using VectorMath::kTau;
using VectorMath::kRadiansPerDegree;
// 描画が止まった後の最初のフレームなどで、ならしが飛ばないようにする 1 回の上限（秒）。
constexpr float kMaximumStep = 0.05f;
// 対象がほぼ真上・真下にいる間は水平の向きが決まらないので、これより近い間は向きを変えない (m)。
constexpr float kMinimumAimDistance = 0.3f;
// 注視点を視点から前へ置く距離 (m)。向きだけが意味を持つ。
constexpr float kLookDistance = 10.0f;
constexpr float kDefaultFovRadians = 1.04719755f;
constexpr float kNoSpeedLimit = std::numeric_limits<float>::infinity();
// 見せ方（距離 m・角度 rad）が要求の値から Player の値へ戻り切ったとみなす差。戻り切ったらならしをやめる。
constexpr float kShapeSettleEpsilon = 1.0e-4f;

using Vec3 = HIKARI::GameProjectFloat3;

// このフレームのカメラの追い方と見上げの限度（度）、見せ方の目標。外部の要求（騎乗中など）があればそれ、
// 無ければ Player の値（見せ方は Player の値へ戻す）。
struct CameraLimits {
    float followSeconds = 0.0f;
    float verticalSeconds = 0.0f;
    float verticalDeadZone = 0.0f;
    float minPitch = 0.0f;
    float maxPitch = 0.0f;
    bool requested = false;
    PlayerCameraShape shape{};
    float fovKickRadians = 0.0f;
    float shapeSeconds = 0.0f;
    float fovSeconds = 0.0f;
    float rollSeconds = 0.0f;
};

// 要求の値（負は Player の値のまま）。
float RequestedOr(float requested, float fallback) { return requested >= 0.0f ? requested : fallback; }

// 見せ方の、Player の値（要求が無い時）。画角の 0 は「カメラの画角」（始める前は分からないので StepShape で決める）。
PlayerCameraShape DefaultShape(const PlayerComponent& player) {
    return {player.cameraDistance, 0.0f, player.cameraEyeFloor, 0.0f, 0.0f};
}

// 要求はこのフレームだけ使い、消す（要求した側が消えても次のフレームへ残さない）。
CameraLimits TakeLimits(PlayerComponent& player) {
    CameraLimits limits{player.cameraFollowSeconds, player.cameraVerticalSeconds, player.cameraVerticalDeadZone,
                        player.cameraMinPitch, player.cameraMaxPitch};
    limits.shape = DefaultShape(player);
    limits.shapeSeconds = limits.fovSeconds = limits.rollSeconds = player.cameraReleaseSeconds;
    if (player.cameraRequestPending) {
        const PlayerCameraRequest& request = player.cameraRequest;
        limits.followSeconds = RequestedOr(request.followSeconds, player.cameraFollowSeconds);
        limits.verticalSeconds = request.verticalSeconds;
        limits.verticalDeadZone = request.verticalDeadZone;
        limits.minPitch = request.minPitch;
        limits.maxPitch = request.maxPitch;
        limits.requested = true;
        limits.shape.distance = RequestedOr(request.distance, player.cameraDistance);
        limits.shape.fovRadians = request.fovDegrees > 0.0f ? request.fovDegrees * kRadiansPerDegree : 0.0f;
        limits.shape.eyeFloor = RequestedOr(request.eyeFloor, player.cameraEyeFloor);
        limits.shape.rollRadians = request.rollDegrees * kRadiansPerDegree;
        limits.shape.lookDownRadians = request.lookDownDegrees * kRadiansPerDegree;
        limits.fovKickRadians = request.fovKickDegrees * kRadiansPerDegree;
        limits.shapeSeconds = request.shapeSeconds;
        limits.fovSeconds = request.fovSeconds;
        limits.rollSeconds = request.rollSeconds;
        player.cameraReleaseSeconds = std::max(0.0f, request.releaseSeconds);
    }
    player.cameraRequestPending = false;
    return limits;
}

// 臨界減衰のばね。目標が急に動いても、出だしも止まり際もなめらかに追う。maximumSpeed は追う速さの上限。
float SmoothDamp(float current, float target, float& velocity, float smoothSeconds, float maximumSpeed, float dt) {
    if (dt <= 0.0f)
        return current;
    if (smoothSeconds <= 0.0001f) {
        velocity = 0.0f;
        return target;
    }
    const float omega = 2.0f / smoothSeconds;
    const float x = omega * dt;
    const float decay = 1.0f / (1.0f + x + 0.48f * x * x + 0.235f * x * x * x);
    const float limit = maximumSpeed * smoothSeconds;
    const float change = std::clamp(current - target, -limit, limit);
    const float reachable = current - change;
    const float step = (velocity + omega * change) * dt;
    velocity = (velocity - omega * step) * decay;
    float result = reachable + (change + step) * decay;
    // 目標を行き過ぎたら、そこで止める。
    if ((target - current > 0.0f) == (result > target)) {
        result = target;
        velocity = 0.0f;
    }
    return result;
}

// 角度（rad）の版。近いほうの回りで追う。
float SmoothDampAngle(float current, float target, float& velocity, float smoothSeconds, float maximumSpeed, float dt) {
    return SmoothDamp(current, current + std::remainder(target - current, kTau), velocity, smoothSeconds, maximumSpeed,
                      dt);
}

// ロックオン中に見る点（対象の原点＋外部が入れたずらし）。ロックしていない・対象が無い時は false。
bool LockPoint(HIKARI::WorldView& world, const PlayerComponent& player, Vec3& outPoint) {
    if (!player.lockedOn)
        return false;
    const auto target = world.FindObject(player.lockTarget);
    HIKARI::GameProjectTransform transform{};
    if (!target.IsValid() || !target.GetWorldTransform(transform))
        return false;
    outPoint = {transform.position.x + player.lockPivotOffset.x, transform.position.y + player.lockPivotOffset.y,
                transform.position.z + player.lockPivotOffset.z};
    return true;
}

void Start(HIKARI::WorldView& world, HIKARI::ObjectView camera, PlayerComponent& player, Vec3 goal) {
    player.cameraReady = true;
    player.cameraYaw = std::atan2(player.facing.x, player.facing.z);
    player.cameraPitchNow = player.cameraPitch * kRadiansPerDegree;
    player.cameraYawVelocity = 0.0f;
    player.cameraPitchVelocity = 0.0f;
    player.cameraPivot = goal;
    player.cameraPivotVelocity = {};
    HIKARI::GameProjectCameraLens lens{};
    const bool hasLens = world.GetCamera().GetAuthoredLens(camera.GetHandle().value, lens);
    player.cameraFovRadians = hasLens && lens.verticalFovRadians > 0.0f ? lens.verticalFovRadians : kDefaultFovRadians;
}

// 軸（プレイヤーの少し上）を追う。水平は速く追い、上下は遊びの幅を超えた分だけゆっくり追う。
// 吹っ飛びの放物線は追わない（跳ね上がって落ちるだけなので、追うと画面が上下に揺れる）。
void FollowPivot(PlayerComponent& player, const CameraLimits& limits, Vec3 goal, float dt) {
    Vec3& pivot = player.cameraPivot;
    Vec3& velocity = player.cameraPivotVelocity;
    pivot.x = SmoothDamp(pivot.x, goal.x, velocity.x, limits.followSeconds, kNoSpeedLimit, dt);
    pivot.z = SmoothDamp(pivot.z, goal.z, velocity.z, limits.followSeconds, kNoSpeedLimit, dt);
    float wantedHeight = pivot.y;
    if (player.state != PlayerState::Knockback) {
        const float rise = goal.y - pivot.y;
        const float excess = std::abs(rise) - std::max(0.0f, limits.verticalDeadZone);
        if (excess > 0.0f)
            wantedHeight = pivot.y + std::copysign(excess, rise);
    }
    pivot.y = SmoothDamp(pivot.y, wantedHeight, velocity.y, limits.verticalSeconds, kNoSpeedLimit, dt);
}

// ロックオン中：対象の向きへ回る。回る速さに上限を付け、対象が背後へ現れても振り回さない。
// 対象が高い所にいる時（ボスの跳躍）は、その分だけ見上げる。
void TrackTarget(PlayerComponent& player, const CameraLimits& limits, Vec3 targetPoint, float dt) {
    const Vec3& pivot = player.cameraPivot;
    const float offsetX = targetPoint.x - pivot.x;
    const float offsetZ = targetPoint.z - pivot.z;
    const float flatDistance = std::hypot(offsetX, offsetZ);
    if (flatDistance > kMinimumAimDistance)
        player.cameraYaw = SmoothDampAngle(player.cameraYaw, std::atan2(offsetX, offsetZ), player.cameraYawVelocity,
                                           player.lockYawSeconds, player.lockYawMaxSpeed * kRadiansPerDegree, dt);
    const float elevation = std::atan2(targetPoint.y - pivot.y, flatDistance + player.cameraDistance);
    const float wantedPitch = std::clamp(player.lockPitch * kRadiansPerDegree - elevation * player.lockPitchFollow,
                                         limits.minPitch * kRadiansPerDegree, limits.maxPitch * kRadiansPerDegree);
    player.cameraPitchNow = SmoothDamp(player.cameraPitchNow, wantedPitch, player.cameraPitchVelocity,
                                       player.lockPitchSeconds, kNoSpeedLimit, dt);
}

// 自由視点：視点の入力をそのまま角度に足す（ならさない。マウスに遅れると酔う）。
// 見上げ・見下ろしが限度の外にある間（騎乗の要求が終わって限度が狭まった直後）は、一度に戻さず releaseSeconds で
// 限度へ寄せる。入力で外へ広げることはしない。限度の内では今までどおり。
void FreeLook(HIKARI::WorldView& world, PlayerComponent& player, const CameraLimits& limits, float dt) {
    const float minPitch = limits.minPitch * kRadiansPerDegree;
    const float maxPitch = limits.maxPitch * kRadiansPerDegree;
    const float pitch = player.cameraPitchNow;
    const bool inside = pitch >= minPitch && pitch <= maxPitch;
    player.cameraYawVelocity = 0.0f;
    if (inside)
        player.cameraPitchVelocity = 0.0f;
    const auto input = world.GetInput();
    HIKARI::GameProjectInputActionState look{};
    if (input.GetAction("Gameplay.Look", look) && std::isfinite(look.x) && std::isfinite(look.y)) {
        // マウスは動かした画素数、スティックは倒した量（−1〜1）で来る。
        const bool gamepad = input.GetLastActiveDevice() == HIKARI::GameProjectInputDevice::Gamepad;
        const float yawStep = gamepad ? player.cameraStickYawSpeed * dt : player.cameraMouseSensitivity;
        const float pitchStep = gamepad ? player.cameraStickPitchSpeed * dt : player.cameraMouseSensitivity;
        // 右へ動かすと右を向く（yaw は減る）。下へ動かすと見下ろす。
        player.cameraYaw = std::remainder(player.cameraYaw - look.x * yawStep * kRadiansPerDegree, kTau);
        player.cameraPitchNow = std::clamp(pitch + look.y * pitchStep * kRadiansPerDegree, std::min(minPitch, pitch),
                                           std::max(maxPitch, pitch));
    }
    if (inside)
        return;
    const float limit = std::clamp(player.cameraPitchNow, minPitch, maxPitch);
    player.cameraPitchNow = SmoothDamp(player.cameraPitchNow, limit, player.cameraPitchVelocity,
                                       player.cameraReleaseSeconds, kNoSpeedLimit, dt);
}

bool Settled(float now, float target) { return std::abs(now - target) <= kShapeSettleEpsilon; }

// 見せ方（距離・画角・視点の下限・傾き・見下ろし）を目標へならす。要求が一度も無い間と、戻り切った後は Player の値
// そのまま（ならさない。エディタで Player の値を変えるとすぐ効く）。
void StepShape(PlayerComponent& player, const CameraLimits& limits, float dt) {
    PlayerCameraShape target = limits.shape;
    if (target.fovRadians <= 0.0f)
        target.fovRadians = player.cameraFovRadians;
    PlayerCameraShape& now = player.cameraShape;
    PlayerCameraShape& velocity = player.cameraShapeVelocity;
    if (!limits.requested && !player.cameraShaped) {
        now = target;
        velocity = {};
        return;
    }
    now.distance = SmoothDamp(now.distance, target.distance, velocity.distance, limits.shapeSeconds, kNoSpeedLimit, dt);
    now.eyeFloor = SmoothDamp(now.eyeFloor, target.eyeFloor, velocity.eyeFloor, limits.shapeSeconds, kNoSpeedLimit, dt);
    now.lookDownRadians = SmoothDamp(now.lookDownRadians, target.lookDownRadians, velocity.lookDownRadians,
                                     limits.shapeSeconds, kNoSpeedLimit, dt);
    now.fovRadians =
        SmoothDamp(now.fovRadians, target.fovRadians, velocity.fovRadians, limits.fovSeconds, kNoSpeedLimit, dt);
    now.rollRadians =
        SmoothDamp(now.rollRadians, target.rollRadians, velocity.rollRadians, limits.rollSeconds, kNoSpeedLimit, dt);
    const bool settled = Settled(now.distance, target.distance) && Settled(now.eyeFloor, target.eyeFloor) &&
                         Settled(now.lookDownRadians, target.lookDownRadians) &&
                         Settled(now.fovRadians, target.fovRadians) && Settled(now.rollRadians, target.rollRadians);
    player.cameraShaped = limits.requested || !settled;
    if (!player.cameraShaped) {
        now = target;
        velocity = {};
    }
}

void Trace(HIKARI::WorldView& world, PlayerComponent& player, Vec3 body, bool locked, Vec3 submittedEye, float distance,
           float fovRadians, float followSeconds, float dt) {
    HIKARI::GameProjectCameraPose pose{};
    if (!world.GetCamera().GetControlPose(pose))
        return;
    // eye・target は前のフレームに解決した姿勢（補正を含む）。後ろの値（距離・画角・傾き・見下ろし・水平に追うならし・角度）
    // は、このフレームの姿勢を作った時の値。modifier は、前のフレームに出した視点から補正（揺れなど）で動いた長さ (m)。
    char message[448]{};
    std::snprintf(
        message, sizeof(message),
        "CameraTrace eye=(%.4f,%.4f,%.4f) target=(%.4f,%.4f,%.4f) player=(%.4f,%.4f,%.4f) locked=%d "
        "distance=%.4f fov=%.4f roll=%.4f lookDown=%.4f follow=%.4f pitch=%.4f yaw=%.4f dt=%.5f modifier=%.5f",
        static_cast<double>(pose.eye.x), static_cast<double>(pose.eye.y), static_cast<double>(pose.eye.z),
        static_cast<double>(pose.target.x), static_cast<double>(pose.target.y), static_cast<double>(pose.target.z),
        static_cast<double>(body.x), static_cast<double>(body.y), static_cast<double>(body.z), static_cast<int>(locked),
        static_cast<double>(distance), static_cast<double>(fovRadians / kRadiansPerDegree),
        static_cast<double>(player.cameraShape.rollRadians / kRadiansPerDegree),
        static_cast<double>(player.cameraShape.lookDownRadians / kRadiansPerDegree), static_cast<double>(followSeconds),
        static_cast<double>(player.cameraPitchNow / kRadiansPerDegree),
        static_cast<double>(player.cameraYaw / kRadiansPerDegree), static_cast<double>(dt),
        static_cast<double>(std::hypot(pose.eye.x - player.cameraTraceEye.x, pose.eye.y - player.cameraTraceEye.y,
                                       pose.eye.z - player.cameraTraceEye.z)));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "PlayerCamera", message);
    player.cameraTraceEye = submittedEye;
}

} // namespace

void PlayerCamera::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, PlayerComponent& player, float dt) {
    const CameraLimits limits = TakeLimits(player);
    const auto camera = world.FindSceneObject(player.cameraId);
    HIKARI::GameProjectTransform body{};
    if (!camera.IsValid() || !object.GetWorldTransform(body)) {
        player.cameraAccepted = false;
        return;
    }
    dt = std::clamp(dt, 0.0f, kMaximumStep);
    const Vec3 goal{body.position.x, body.position.y + player.cameraPivotHeight, body.position.z};
    if (!player.cameraReady)
        Start(world, camera, player, goal);
    FollowPivot(player, limits, goal, dt);
    Vec3 targetPoint{};
    const bool locked = LockPoint(world, player, targetPoint);
    if (locked)
        TrackTarget(player, limits, targetPoint, dt);
    else
        FreeLook(world, player, limits, dt);
    StepShape(player, limits, dt);
    const PlayerCameraShape& shape = player.cameraShape;

    const float pitchCos = std::cos(player.cameraPitchNow);
    const float pitchSin = std::sin(player.cameraPitchNow);
    const Vec3 flatForward{std::sin(player.cameraYaw), 0.0f, std::cos(player.cameraYaw)};
    const Vec3 forward{flatForward.x * pitchCos, -pitchSin, flatForward.z * pitchCos};
    const Vec3 right{-flatForward.z, 0.0f, flatForward.x};
    // 移動の入力は、この向きを基準にする（前を押すと画面の奥へ進む）。見せるだけの見下ろし・傾きは入れない。
    player.cameraForward = flatForward;
    player.cameraLook = forward;
    // 見上げる向きでは視点が軸より下がる。地面へ潜らない所まで寄せる。
    float distance = std::max(0.1f, shape.distance);
    if (pitchSin < 0.0f)
        distance = std::min(distance, std::max(0.0f, shape.eyeFloor) / -pitchSin);
    // 視点と注視点を同じだけ横へずらす（プレイヤーが画面の中心から少し外れ、見ている向きは変わらない）。
    const Vec3 axis{player.cameraPivot.x + right.x * player.cameraSideOffset, player.cameraPivot.y,
                    player.cameraPivot.z + right.z * player.cameraSideOffset};
    const Vec3 eye{axis.x - forward.x * distance, axis.y - forward.y * distance, axis.z - forward.z * distance};
    // 見せるだけの見下ろし：視点はそのまま、見る向きだけ下げる（プレイヤーが画面の少し上へ寄り、進む先が見える）。
    const float viewPitch = player.cameraPitchNow + shape.lookDownRadians;
    Vec3 view = forward;
    Vec3 from = axis;
    float reach = kLookDistance;
    if (shape.lookDownRadians != 0.0f) {
        view = {flatForward.x * std::cos(viewPitch), -std::sin(viewPitch), flatForward.z * std::cos(viewPitch)};
        from = eye;
        reach = distance + kLookDistance;
    }
    HIKARI::GameProjectCameraPoseRequest request{};
    request.cameraObjectHandle = camera.GetHandle().value;
    request.sourceId = kPoseSource;
    request.priority = kPosePriority;
    request.pose.eye = eye;
    request.pose.target = {from.x + view.x * reach, from.y + view.y * reach, from.z + view.z * reach};
    request.pose.up = {0.0f, 1.0f, 0.0f};
    if (shape.rollRadians != 0.0f) {
        // 見ている向きに直交する上を、右へ傾ける（右へ旋回すると右へ傾く）。
        const float rollCos = std::cos(shape.rollRadians);
        const float rollSin = std::sin(shape.rollRadians);
        const Vec3 viewUp{flatForward.x * std::sin(viewPitch), std::cos(viewPitch),
                          flatForward.z * std::sin(viewPitch)};
        request.pose.up = {viewUp.x * rollCos + right.x * rollSin, viewUp.y * rollCos,
                           viewUp.z * rollCos + right.z * rollSin};
    }
    request.pose.verticalFovRadians = shape.fovRadians + limits.fovKickRadians;
    player.cameraAccepted = world.GetCamera().SubmitPose(request);
    if (player.cameraTrace)
        Trace(world, player, body.position, locked, eye, distance, request.pose.verticalFovRadians,
              limits.followSeconds, dt);
}

} // namespace Project_TD2_2
