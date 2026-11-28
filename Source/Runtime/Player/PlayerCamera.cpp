#include "PlayerCamera.h"

#include <algorithm>
#include <utility>
#include <cmath>
#include <cstdio>
#include <limits>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Project/UiView.h"
#include "Player.h"
#include "PlayerRules.h"
#include "../Common/VectorMath.h"
#include "HIKARI/Extend/World.h"

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
PlayerCameraShape DefaultShape(const Player& player) {
    return {player.cameraDistance, 0.0f, player.cameraEyeFloor, 0.0f, 0.0f};
}

// 要求はこのフレームだけ使い、消す（要求した側が消えても次のフレームへ残さない）。
CameraLimits TakeLimits(Player& player) {
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
    // 見上げ・見下ろしの限度は別々に変えられるので、逆に入っていたら並べ替える（std::clamp は下限 > 上限で未定義）。
    if (limits.minPitch > limits.maxPitch)
        std::swap(limits.minPitch, limits.maxPitch);
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

// ロックオン中に見る点（PlayerRules::LockAimPoint。照準と同じ点）。ロックしていない・対象が無い時は false。
bool LockPoint(HIKARI::WorldView& world, const Player& player, Vec3& outPoint) {
    if (!player.lockedOn)
        return false;
    const auto target = world.FindObject(player.lockTargetHandle);
    HIKARI::GameProjectTransform transform{};
    if (!target.IsValid() || !target.GetWorldTransform(transform))
        return false;
    outPoint = PlayerRules::LockAimPoint(player, transform.position);
    return true;
}

using VectorMath::Lerp;

// a 以下で 0、b 以上で 1、その間はなめらかに（a ≥ b なら a で切り替える）。
float SmoothStep(float a, float b, float x) {
    if (b <= a)
        return x < a ? 0.0f : 1.0f;
    const float t = std::clamp((x - a) / (b - a), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}

// ロックの見せ方（c-880340）。近さ（1 で近い）とロックの重みで、Player の値から混ぜた、このフレームの値。
struct LockFraming {
    float distanceAdd = 0.0f; // 見せ方の距離に足す (m)
    float rise = 0.0f;        // 軸を上げる (m)
    float lookUp = 0.0f;      // 見る向きだけの見上げ (rad)
};

// 対象との水平の距離から、近さとロックの重みをならす。ロックしていない・外部の要求（騎乗）の間は重みを 0 へ戻す。
LockFraming StepLockFraming(Player& player, bool locked, float flatDistance, float dt) {
    const bool active = locked;
    if (active) {
        const float nearness = 1.0f - SmoothStep(player.lockNearDistance, player.lockFarDistance, flatDistance);
        player.lockNearness = SmoothDamp(player.lockNearness, nearness, player.lockNearnessVelocity,
                                         player.lockFramingSeconds, kNoSpeedLimit, dt);
    }
    player.lockAmount = SmoothDamp(player.lockAmount, active ? 1.0f : 0.0f, player.lockAmountVelocity,
                                   player.lockFramingSeconds, kNoSpeedLimit, dt);
    const float n = player.lockNearness;
    const float w = player.lockAmount;
    LockFraming framing;
    framing.distanceAdd =
        w * (Lerp(player.lockCameraDistanceFar, player.lockCameraDistanceNear, n) - player.cameraDistance);
    framing.rise = w * Lerp(player.lockPivotRiseFar, player.lockPivotRiseNear, n);
    framing.lookUp = w * Lerp(player.lockLookUpFar, player.lockLookUpNear, n) * kRadiansPerDegree;
    return framing;
}

// 対象が跳んだ（隠れた・一瞬で離れた所へ現れた・切り替わった）かを見て、向き直しの間かを決める。
void NoteLockJump(Player& player, Vec3 targetPoint) {
    const bool switched = player.lockTargetLast.value != player.lockTargetHandle.value;
    const bool jumped = player.lockPointKnown &&
                        std::hypot(targetPoint.x - player.lockPointLast.x, targetPoint.z - player.lockPointLast.z) >
                            player.lockJumpDistance;
    if (player.lockReacquireEnabled && player.lockPointKnown && (switched || jumped))
        player.lockReacquiring = true;
    if (!player.lockReacquireEnabled)
        player.lockReacquiring = false;
    player.lockPointLast = targetPoint;
    player.lockPointKnown = true;
    player.lockTargetLast = player.lockTargetHandle;
}

void Start(HIKARI::WorldView& world, HIKARI::ObjectView camera, Player& player, Vec3 goal) {
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
void FollowPivot(Player& player, const CameraLimits& limits, Vec3 goal, float dt) {
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
// 対象が高い所にいる時（ボスの跳躍）は、その分だけ見上げる。lockFraming が true の時（ロックオン。演出の向く点を
// 追う時は false で前のまま）は、c-880340 の規則：近いほど追うならしを長く・回る速さの上限を低くし、見下ろしを
// 近さで混ぜ、隠れている間は向きを保ち、現れた・切り替えた後はゆっくり向き直す。
void TrackTarget(Player& player, const CameraLimits& limits, Vec3 targetPoint, float dt, bool lockFraming) {
    const Vec3& pivot = player.cameraPivot;
    const float offsetX = targetPoint.x - pivot.x;
    const float offsetZ = targetPoint.z - pivot.z;
    const float flatDistance = std::hypot(offsetX, offsetZ);
    float yawSeconds = player.lockYawSeconds;
    float yawMaxSpeed = player.lockYawMaxSpeed;
    float basePitch = player.lockPitch;
    float minPitch = limits.minPitch;
    if (lockFraming) {
        const float closeness =
            1.0f - SmoothStep(player.lockTurnNearDistance, player.lockTurnFarDistance, flatDistance);
        yawSeconds = Lerp(player.lockYawSeconds, player.lockYawSecondsNear, closeness);
        yawMaxSpeed = Lerp(player.lockYawMaxSpeed, player.lockYawMaxSpeedNear, closeness);
        basePitch = Lerp(player.lockPitch, player.lockPitchNear, player.lockNearness);
        minPitch = std::max(minPitch, player.lockMinPitch);
        if (player.lockReacquiring) {
            // 向き直しの間は、遅い方を使う。向きがそろったら普段の追いへ戻す。
            yawSeconds = std::max(yawSeconds, player.lockReacquireSeconds);
            yawMaxSpeed = std::min(yawMaxSpeed, player.lockReacquireMaxSpeed);
            const float error = std::abs(std::remainder(std::atan2(offsetX, offsetZ) - player.cameraYaw, kTau));
            if (flatDistance > kMinimumAimDistance && error < 3.0f * kRadiansPerDegree)
                player.lockReacquiring = false;
        }
    }
    if (flatDistance > kMinimumAimDistance)
        player.cameraYaw = SmoothDampAngle(player.cameraYaw, std::atan2(offsetX, offsetZ), player.cameraYawVelocity,
                                           yawSeconds, yawMaxSpeed * kRadiansPerDegree, dt);
    const float elevation = std::atan2(targetPoint.y - pivot.y, flatDistance + player.cameraDistance);
    const float wantedPitch =
        std::clamp(basePitch * kRadiansPerDegree - elevation * player.lockPitchFollow,
                   std::min(minPitch, limits.maxPitch) * kRadiansPerDegree, limits.maxPitch * kRadiansPerDegree);
    player.cameraPitchNow = SmoothDamp(player.cameraPitchNow, wantedPitch, player.cameraPitchVelocity,
                                       player.lockPitchSeconds, kNoSpeedLimit, dt);
}

// 自由視点：視点の入力をそのまま角度に足す（ならさない。マウスに遅れると酔う）。
// 見上げ・見下ろしが限度の外にある間（騎乗の要求が終わって限度が狭まった直後）は、一度に戻さず releaseSeconds で
// 限度へ寄せる。入力で外へ広げることはしない。限度の内では今までどおり。
void FreeLook(HIKARI::WorldView& world, Player& player, const CameraLimits& limits, float dt) {
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

// 演出のカメラの姿勢と、ゲームのカメラの姿勢を混ぜる（位置は直線、見る向きは向きで、画角は直線。重み 1 でゲーム）。
void BlendCinematic(HIKARI::GameProjectCameraPose& pose, const PlayerCinematicRequest& cinematic) {
    const float w = std::clamp(cinematic.gameWeight, 0.0f, 1.0f);
    if (w >= 1.0f)
        return;
    const auto mix = [](Vec3 a, Vec3 b, float t) {
        return Vec3{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t};
    };
    const auto unit = [](Vec3 v, Vec3 fallback) {
        const float length = std::sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
        return length > 1.0e-5f ? Vec3{v.x / length, v.y / length, v.z / length} : fallback;
    };
    const Vec3 game =
        unit({pose.target.x - pose.eye.x, pose.target.y - pose.eye.y, pose.target.z - pose.eye.z}, {0.0f, 0.0f, 1.0f});
    const Vec3 film = unit({cinematic.target.x - cinematic.eye.x, cinematic.target.y - cinematic.eye.y,
                            cinematic.target.z - cinematic.eye.z},
                           game);
    const Vec3 eye = mix(cinematic.eye, pose.eye, w);
    const Vec3 view = unit(mix(film, game, w), game);
    pose.eye = eye;
    pose.target = {eye.x + view.x * kLookDistance, eye.y + view.y * kLookDistance, eye.z + view.z * kLookDistance};
    pose.up = unit(mix({0.0f, 1.0f, 0.0f}, pose.up, w), {0.0f, 1.0f, 0.0f});
    pose.verticalFovRadians = cinematic.fovRadians + (pose.verticalFovRadians - cinematic.fovRadians) * w;
}

bool Settled(float now, float target) { return std::abs(now - target) <= kShapeSettleEpsilon; }

// 見せ方（距離・画角・視点の下限・傾き・見下ろし）を目標へならす。要求が一度も無い間と、戻り切った後は Player の値
// そのまま（ならさない。エディタで Player の値を変えるとすぐ効く）。
void StepShape(Player& player, const CameraLimits& limits, float dt) {
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

void Trace(HIKARI::WorldView& world, Player& player, Vec3 body, bool locked, Vec3 submittedEye, float distance,
           float fovRadians, float followSeconds, float dt) {
    HIKARI::GameProjectCameraPose pose{};
    if (!world.GetCamera().GetControlPose(pose))
        return;
    // eye・target は前のフレームに解決した姿勢（補正を含む）。後ろの値（距離・画角・傾き・見下ろし・水平に追うならし・角度）
    // は、このフレームの姿勢を作った時の値。modifier は、前のフレームに出した視点から補正（揺れなど）で動いた長さ (m)。
    // lockNear・lockAmount・reacquire・hidden は、ロックの見せ方（c-880340）の近さ・重み・向き直しの間・対象が隠れているか。
    // lockTarget・marker はロックの対象と、出した印の位置（1920×1080 の画面の画素。出していない時は負）。
    char message[640]{};
    std::snprintf(
        message, sizeof(message),
        "CameraTrace eye=(%.4f,%.4f,%.4f) target=(%.4f,%.4f,%.4f) player=(%.4f,%.4f,%.4f) locked=%d "
        "distance=%.4f fov=%.4f roll=%.4f lookDown=%.4f follow=%.4f pitch=%.4f yaw=%.4f dt=%.5f modifier=%.5f "
        "lockNear=%.3f lockAmount=%.3f reacquire=%d lockTarget=%llu marker=(%.1f,%.1f)",
        static_cast<double>(pose.eye.x), static_cast<double>(pose.eye.y), static_cast<double>(pose.eye.z),
        static_cast<double>(pose.target.x), static_cast<double>(pose.target.y), static_cast<double>(pose.target.z),
        static_cast<double>(body.x), static_cast<double>(body.y), static_cast<double>(body.z), static_cast<int>(locked),
        static_cast<double>(distance), static_cast<double>(fovRadians / kRadiansPerDegree),
        static_cast<double>(player.cameraShape.rollRadians / kRadiansPerDegree),
        static_cast<double>(player.cameraShape.lookDownRadians / kRadiansPerDegree), static_cast<double>(followSeconds),
        static_cast<double>(player.cameraPitchNow / kRadiansPerDegree),
        static_cast<double>(player.cameraYaw / kRadiansPerDegree), static_cast<double>(dt),
        static_cast<double>(std::hypot(pose.eye.x - player.cameraTraceEye.x, pose.eye.y - player.cameraTraceEye.y,
                                       pose.eye.z - player.cameraTraceEye.z)),
        static_cast<double>(player.lockNearness), static_cast<double>(player.lockAmount),
        static_cast<int>(player.lockReacquiring), static_cast<unsigned long long>(player.lockTargetHandle.value),
        static_cast<double>(player.lockMarkerX), static_cast<double>(player.lockMarkerY));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "PlayerCamera", message);
    player.cameraTraceEye = submittedEye;
}

// ロックの印（c-880340）：見る点に小さいひし形（縁取り付き）を出す。画面の大きさ・縦横比は直前に描けた画面の
// 写し（TryGetViewSnapshot）から、姿勢と画角はこのフレームに出した値を使う（写しの姿勢は 1 フレーム遅れるので、
// 回っている間に印が対象からずれないように）。写しが無い時は 1920×1080 とみなす。
// 出した印の位置（1920×1080 の画面の画素）は player.lockMarkerX・Y（出していない時は負。確かめの記録用）。
void DrawLockMarker(HIKARI::WorldView& world, Player& player, const HIKARI::GameProjectCameraPose& pose, bool show,
                    Vec3 point) {
    constexpr uint64_t kMarkerEdgeId = 0x5444325010ull;
    constexpr uint64_t kMarkerId = 0x5444325011ull;
    constexpr int kMarkerOrder = 50;
    constexpr float kReferenceWidth = 1920.0f;
    constexpr float kReferenceHeight = 1080.0f;
    constexpr float kEdgePixels = 3.0f;
    player.lockMarkerX = player.lockMarkerY = -1.0f;
    if (!show || player.lockMarkerOpacity <= 0.0f || player.lockMarkerSize <= 0.0f)
        return;
    HIKARI::GameProjectCameraViewSnapshot snapshot{};
    if (!world.GetCamera().TryGetViewSnapshot(snapshot) || snapshot.uiLogicalWidth == 0u ||
        snapshot.uiLogicalHeight == 0u) {
        snapshot = {};
        snapshot.aspectRatio = kReferenceWidth / kReferenceHeight;
        snapshot.uiLogicalWidth = static_cast<uint32_t>(kReferenceWidth);
        snapshot.uiLogicalHeight = static_cast<uint32_t>(kReferenceHeight);
        snapshot.sceneContentLogical = {0.0f, 0.0f, kReferenceWidth, kReferenceHeight};
    }
    snapshot.pose.eye = pose.eye;
    snapshot.pose.target = pose.target;
    snapshot.pose.up = pose.up;
    snapshot.lens.verticalFovRadians = pose.verticalFovRadians;
    HIKARI::GameProjectWorldToScreenResult screen{};
    if (!HIKARI::ProjectWorldToScreen(snapshot, point, HIKARI::GameProjectScreenCoordinateSpace::UiLogicalPixels,
                                      screen) ||
        (screen.flags & HIKARI::GameProjectWorldToScreenFlags(HIKARI::GameProjectWorldToScreenFlag::InFront)) == 0u)
        return;
    HIKARI::GameProjectUiCanvasFit fit{};
    float x = 0.0f;
    float y = 0.0f;
    if (!HIKARI::ResolveGameProjectUiCanvasFit(snapshot.uiLogicalWidth, snapshot.uiLogicalHeight, kReferenceWidth,
                                               kReferenceHeight, fit) ||
        !HIKARI::ProjectGameUiScreenPointToCanvas(fit, screen.x, screen.y, x, y))
        return;
    player.lockMarkerX = x;
    player.lockMarkerY = y;
    const auto diamond = [&](uint64_t id, int order, float size, HIKARI::GameProjectFloat3 color, float opacity) {
        HIKARI::GameProjectUiSpriteRequest request{};
        request.sourceId = id;
        request.order = order;
        request.x = x;
        request.y = y;
        request.width = size;
        request.height = size;
        request.pivotX = 0.5f;
        request.pivotY = 0.5f;
        request.rotationRadians = 0.25f * VectorMath::kPi;
        request.tint = {color.x, color.y, color.z, opacity};
        world.GetUi().SubmitSprite("generated://solid/ffffffff", request);
    };
    const float size = player.lockMarkerSize;
    diamond(kMarkerEdgeId, kMarkerOrder, size + kEdgePixels, {0.05f, 0.04f, 0.04f}, player.lockMarkerOpacity * 0.6f);
    const HIKARI::Color color = player.lockMarkerColor;
    diamond(kMarkerId, kMarkerOrder + 1, size, {color.r, color.g, color.b}, player.lockMarkerOpacity);
}

} // namespace

void PlayerCamera::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, Player& player, float dt, float alpha) {
    const CameraLimits limits = TakeLimits(player);
    const auto camera = HIKARI::Extend::ObjectOf(player.camera);
    HIKARI::GameProjectTransform body{};
    if (!camera.IsValid() || !object.GetWorldTransform(body)) {
        player.cameraAccepted = false;
        return;
    }
    dt = std::clamp(dt, 0.0f, kMaximumStep);
    // 体は描かれる位置（tick の間の補間）で追う。tick ごとの段の位置で追うと、描画が tick より速い時にカメラががくつく。
    body.position = PlayerRules::PresentedPosition(player, body.position, alpha);
    const Vec3 goal{body.position.x, body.position.y + player.cameraPivotHeight, body.position.z};
    if (!player.cameraReady)
        Start(world, camera, player, goal);
    FollowPivot(player, limits, goal, dt);
    Vec3 targetPoint{};
    const bool locked = LockPoint(world, player, targetPoint);
    if (locked) {
        NoteLockJump(player, targetPoint);
    } else {
        player.lockPointKnown = false;
        player.lockReacquiring = false;
        player.lockTargetLast = {};
    }
    // 距離に合わせた見せ方（騎乗などの外部の要求がある間は使わない＝要求の見せ方のまま）。
    const LockFraming framing =
        StepLockFraming(player, locked && !limits.requested,
                        std::hypot(targetPoint.x - body.position.x, targetPoint.z - body.position.z), dt);
    if (locked)
        // 騎乗などの外部の要求がある間は、前の追い方のまま（筋斗雲のカメラを変えない）。
        TrackTarget(player, limits, targetPoint, dt, !limits.requested);
    else if (player.cinematicPending)
        // 演出の間は、ゲームのカメラを演出の向く点へ向けておく（戻った時にそちらを見ている）。
        TrackTarget(player, limits, player.cinematic.focus, dt, false);
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
    float distance = std::max(0.1f, shape.distance + framing.distanceAdd);
    if (pitchSin < 0.0f)
        distance = std::min(distance, std::max(0.0f, shape.eyeFloor) / -pitchSin);
    // 視点と注視点を同じだけ横へずらす（プレイヤーが画面の中心から少し外れ、見ている向きは変わらない）。
    // ロック中は軸を少し上げる（framing.rise。近い時に大きいボスを見上げやすく）。
    const Vec3 axis{player.cameraPivot.x + right.x * player.cameraSideOffset, player.cameraPivot.y + framing.rise,
                    player.cameraPivot.z + right.z * player.cameraSideOffset};
    const Vec3 eye{axis.x - forward.x * distance, axis.y - forward.y * distance, axis.z - forward.z * distance};
    // 見せるだけの見下ろし：視点はそのまま、見る向きだけ下げる（プレイヤーが画面の少し上へ寄り、進む先が見える）。
    // ロック中の見上げ（framing.lookUp）も同じ仕組みで、見る向きだけ上げる（プレイヤーが画面の下の方に残る）。
    const float viewPitch = player.cameraPitchNow + shape.lookDownRadians - framing.lookUp;
    Vec3 view = forward;
    Vec3 from = axis;
    float reach = kLookDistance;
    if (shape.lookDownRadians != 0.0f || framing.lookUp != 0.0f) {
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
    // 演出のカメラ（ボスの登場）：ゲームのカメラの姿勢と重みで混ぜる。最後は重み 1 で、切れ目なくゲームのカメラへ戻る。
    // 演出の間（混ぜている間・入力を止めている間）は、ロックの印を出さない。
    const bool cinematic = player.cinematicPending || player.cinematicHold > 0;
    if (player.cinematicPending) {
        BlendCinematic(request.pose, player.cinematic);
        player.cinematicPending = false;
    }
    player.cameraAccepted = world.GetCamera().SubmitPose(request);
    DrawLockMarker(world, player, request.pose, locked && !cinematic, targetPoint);
    if (player.cameraTrace)
        Trace(world, player, body.position, locked, eye, distance, request.pose.verticalFovRadians,
              limits.followSeconds, dt);
}

} // namespace Project_TD2_2
