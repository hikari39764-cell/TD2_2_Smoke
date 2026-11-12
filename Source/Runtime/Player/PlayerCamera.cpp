#include "PlayerCamera.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <limits>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/ProjectLog.h"
#include "PlayerComponent.h"
#include "PlayerRules.h"

namespace Project_TD2_2 {
namespace {

// 競合の優先順位と識別子はゲームの規則であり、見た目の調整値ではない。エンジンの追従カメラ（優先度 0）より上。
constexpr uint64_t kPoseSource = 0x5444325002ull;
constexpr int kPosePriority = 300;
constexpr float kTau = 6.28318530718f;
constexpr float kRadiansPerDegree = kTau / 360.0f;
// 描画が止まった後の最初のフレームなどで、ならしが飛ばないようにする 1 回の上限（秒）。
constexpr float kMaximumStep = 0.05f;
// 対象がほぼ真上・真下にいる間は水平の向きが決まらないので、これより近い間は向きを変えない (m)。
constexpr float kMinimumAimDistance = 0.3f;
// 注視点を視点から前へ置く距離 (m)。向きだけが意味を持つ。
constexpr float kLookDistance = 10.0f;
constexpr float kDefaultFovRadians = 1.04719755f;
constexpr float kNoSpeedLimit = std::numeric_limits<float>::infinity();

using Vec3 = HIKARI::GameProjectFloat3;

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
void FollowPivot(PlayerComponent& player, Vec3 goal, float dt) {
    Vec3& pivot = player.cameraPivot;
    Vec3& velocity = player.cameraPivotVelocity;
    pivot.x = SmoothDamp(pivot.x, goal.x, velocity.x, player.cameraFollowSeconds, kNoSpeedLimit, dt);
    pivot.z = SmoothDamp(pivot.z, goal.z, velocity.z, player.cameraFollowSeconds, kNoSpeedLimit, dt);
    float wantedHeight = pivot.y;
    if (player.state != PlayerState::Knockback) {
        const float rise = goal.y - pivot.y;
        const float excess = std::abs(rise) - std::max(0.0f, player.cameraVerticalDeadZone);
        if (excess > 0.0f)
            wantedHeight = pivot.y + std::copysign(excess, rise);
    }
    pivot.y = SmoothDamp(pivot.y, wantedHeight, velocity.y, player.cameraVerticalSeconds, kNoSpeedLimit, dt);
}

// ロックオン中：対象の向きへ回る。回る速さに上限を付け、対象が背後へ現れても振り回さない。
// 対象が高い所にいる時（ボスの跳躍）は、その分だけ見上げる。
void TrackTarget(PlayerComponent& player, Vec3 targetPoint, float dt) {
    const Vec3& pivot = player.cameraPivot;
    const float offsetX = targetPoint.x - pivot.x;
    const float offsetZ = targetPoint.z - pivot.z;
    const float flatDistance = std::hypot(offsetX, offsetZ);
    if (flatDistance > kMinimumAimDistance)
        player.cameraYaw = SmoothDampAngle(player.cameraYaw, std::atan2(offsetX, offsetZ), player.cameraYawVelocity,
                                           player.lockYawSeconds, player.lockYawMaxSpeed * kRadiansPerDegree, dt);
    const float elevation = std::atan2(targetPoint.y - pivot.y, flatDistance + player.cameraDistance);
    const float wantedPitch =
        std::clamp(player.lockPitch * kRadiansPerDegree - elevation * player.lockPitchFollow,
                   player.cameraMinPitch * kRadiansPerDegree, player.cameraMaxPitch * kRadiansPerDegree);
    player.cameraPitchNow = SmoothDamp(player.cameraPitchNow, wantedPitch, player.cameraPitchVelocity,
                                       player.lockPitchSeconds, kNoSpeedLimit, dt);
}

// 自由視点：視点の入力をそのまま角度に足す（ならさない。マウスに遅れると酔う）。
void FreeLook(HIKARI::WorldView& world, PlayerComponent& player, float dt) {
    player.cameraYawVelocity = 0.0f;
    player.cameraPitchVelocity = 0.0f;
    const auto input = world.GetInput();
    HIKARI::GameProjectInputActionState look{};
    if (!input.GetAction("Gameplay.Look", look) || !std::isfinite(look.x) || !std::isfinite(look.y))
        return;
    // マウスは動かした画素数、スティックは倒した量（−1〜1）で来る。
    const bool gamepad = input.GetLastActiveDevice() == HIKARI::GameProjectInputDevice::Gamepad;
    const float yawStep = gamepad ? player.cameraStickYawSpeed * dt : player.cameraMouseSensitivity;
    const float pitchStep = gamepad ? player.cameraStickPitchSpeed * dt : player.cameraMouseSensitivity;
    // 右へ動かすと右を向く（yaw は減る）。下へ動かすと見下ろす。
    player.cameraYaw = std::remainder(player.cameraYaw - look.x * yawStep * kRadiansPerDegree, kTau);
    player.cameraPitchNow =
        std::clamp(player.cameraPitchNow + look.y * pitchStep * kRadiansPerDegree,
                   player.cameraMinPitch * kRadiansPerDegree, player.cameraMaxPitch * kRadiansPerDegree);
}

void Trace(HIKARI::WorldView& world, Vec3 body, bool locked) {
    HIKARI::GameProjectCameraPose pose{};
    if (!world.GetCamera().GetControlPose(pose))
        return;
    char message[320]{};
    std::snprintf(message, sizeof(message),
                  "CameraTrace eye=(%.4f,%.4f,%.4f) target=(%.4f,%.4f,%.4f) player=(%.4f,%.4f,%.4f) locked=%d",
                  static_cast<double>(pose.eye.x), static_cast<double>(pose.eye.y), static_cast<double>(pose.eye.z),
                  static_cast<double>(pose.target.x), static_cast<double>(pose.target.y),
                  static_cast<double>(pose.target.z), static_cast<double>(body.x), static_cast<double>(body.y),
                  static_cast<double>(body.z), static_cast<int>(locked));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "PlayerCamera", message);
}

} // namespace

void PlayerCamera::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, PlayerComponent& player, float dt) {
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
    FollowPivot(player, goal, dt);
    Vec3 targetPoint{};
    const bool locked = LockPoint(world, player, targetPoint);
    if (locked)
        TrackTarget(player, targetPoint, dt);
    else
        FreeLook(world, player, dt);

    const float pitchCos = std::cos(player.cameraPitchNow);
    const float pitchSin = std::sin(player.cameraPitchNow);
    const Vec3 flatForward{std::sin(player.cameraYaw), 0.0f, std::cos(player.cameraYaw)};
    const Vec3 forward{flatForward.x * pitchCos, -pitchSin, flatForward.z * pitchCos};
    const Vec3 right{-flatForward.z, 0.0f, flatForward.x};
    // 移動の入力は、この向きを基準にする（前を押すと画面の奥へ進む）。
    player.cameraForward = flatForward;
    // 見上げる向きでは視点が軸より下がる。地面へ潜らない所まで寄せる。
    float distance = std::max(0.1f, player.cameraDistance);
    if (pitchSin < 0.0f)
        distance = std::min(distance, std::max(0.0f, player.cameraEyeFloor) / -pitchSin);
    // 視点と注視点を同じだけ横へずらす（プレイヤーが画面の中心から少し外れ、見ている向きは変わらない）。
    const Vec3 axis{player.cameraPivot.x + right.x * player.cameraSideOffset, player.cameraPivot.y,
                    player.cameraPivot.z + right.z * player.cameraSideOffset};
    HIKARI::GameProjectCameraPoseRequest request{};
    request.cameraObjectHandle = camera.GetHandle().value;
    request.sourceId = kPoseSource;
    request.priority = kPosePriority;
    request.pose.eye = {axis.x - forward.x * distance, axis.y - forward.y * distance, axis.z - forward.z * distance};
    request.pose.target = {axis.x + forward.x * kLookDistance, axis.y + forward.y * kLookDistance,
                           axis.z + forward.z * kLookDistance};
    request.pose.up = {0.0f, 1.0f, 0.0f};
    request.pose.verticalFovRadians = player.cameraFovRadians;
    player.cameraAccepted = world.GetCamera().SubmitPose(request);
    if (player.cameraTrace)
        Trace(world, body.position, locked);
}

} // namespace Project_TD2_2
