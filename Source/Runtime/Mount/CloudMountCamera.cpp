#include "CloudMountCamera.h"

#include <algorithm>
#include <cmath>

#include "../Common/VectorMath.h"

namespace Project_TD2_2 {
namespace {

// ブーストの出だしの画角を上げる長さの、cameraBoostFovKickSeconds に対する割合（残りで戻す）。上げる所もなめらかに
// して 1 フレームで跳ねないようにする。上げ方の速さを変える時だけ触る。
constexpr float kFovKickAttackShare = 0.5f;
// 出だしの間は、画角のならしを cameraBoostFovKickSeconds のこの割合まで短くする（ブーストの画角へ素早く届いてから、
// 足した分だけ行き過ぎて戻る。普段のならしのままだと届く前に足した分が消え、行き過ぎにならない）。
constexpr float kFovKickSmoothShare = 0.2f;
// 速さ・加速の割り算の下限（巡航・ブーストの速さを 0 にした時に割らない）。
constexpr float kMinimumSpan = 0.01f;
// これより弱いぼかしは出さない（ならしの尾を切る）。
constexpr float kBlurOffWeight = 0.001f;

using VectorMath::Clamp01;
using VectorMath::Lerp;
using VectorMath::Smoothstep;

// 段階（0〜2）での値：止まっている → 巡航 → ブースト。
float StageValue(float stage, float stop, float cruise, float boost) {
    return stage <= 1.0f ? Lerp(stop, cruise, std::max(stage, 0.0f)) : Lerp(cruise, boost, Clamp01(stage - 1.0f));
}

// 目標へ seconds の時定数で近づく（0 以下で一瞬）。
using VectorMath::Approach;

// 水平に追うならし：速さが増えている間は伸ばし（体が先に画面の奥へ出る）、減っている間は縮める。伸ばす時はすぐ、
// 戻す時は cameraFollowRecoverSeconds でならす（伸びたまま速さが一定になると、カメラが追いついて元の構図へ戻る）。
void StepFollow(CloudMount& mount, float dt) {
    const float base = std::max(0.0f, mount.cameraFollowSeconds);
    const float full = std::max(mount.cameraFollowFullAccel, kMinimumSpan);
    const float acceleration = mount.flight.acceleration;
    const float target =
        acceleration >= 0.0f
            ? Lerp(base, std::max(0.0f, mount.cameraFollowAccelSeconds), Clamp01(acceleration / full))
            : Lerp(base, std::max(0.0f, mount.cameraFollowBrakeSeconds), Clamp01(-acceleration / full));
    if (mount.cameraFollowNow < 0.0f)
        mount.cameraFollowNow = base;
    mount.cameraFollowNow = target > mount.cameraFollowNow
                                ? target
                                : Approach(mount.cameraFollowNow, target, mount.cameraFollowRecoverSeconds, dt);
}

// 傾き（度、右へ旋回すると正）。巡航より遅い時は速さに比例して小さくする（ゆっくり回る時に大きく傾けない）。
float RollDegrees(const CloudMount& mount, float stage) {
    if (!mount.cameraRollEnabled)
        return 0.0f;
    const float limit = stage <= 1.0f ? mount.cameraRollCruise * std::max(stage, 0.0f)
                                      : Lerp(mount.cameraRollCruise, mount.cameraRollBoost, Clamp01(stage - 1.0f));
    return std::clamp(mount.flight.turn, -1.0f, 1.0f) * std::max(limit, 0.0f);
}

} // namespace

PlayerCameraRequest CloudMountCamera::BuildRequest(CloudMount& mount, float dt) {
    StepFollow(mount, dt);
    const float stage = SpeedStage(mount);
    // 上下のならしを水平へ寄せる割合は、水平の速さで決める（上昇・下降だけの時は上下を素早く追ったまま）。
    const float flatSpeed = std::hypot(mount.velocity.x, mount.velocity.z);
    const float cruiseShare = Clamp01(flatSpeed / std::max(mount.cruiseSpeed, kMinimumSpan));
    PlayerCameraRequest request{};
    request.followSeconds = mount.cameraFollowNow;
    // 上下は、止まっている時は cameraVerticalSeconds、水平の速さが巡航へ近づくほど水平と同じならしにする（遅れが飛ぶ
    // 向きに沿い、体が画面の上下へずれずに奥へ離れる）。
    request.verticalSeconds = Lerp(std::max(0.0f, mount.cameraVerticalSeconds), mount.cameraFollowNow, cruiseShare);
    request.verticalDeadZone = mount.cameraVerticalDeadZone;
    request.minPitch = mount.cameraMinPitch;
    request.maxPitch = mount.cameraMaxPitch;
    request.distance = std::max(
        0.1f, StageValue(stage, mount.cameraDistanceStop, mount.cameraDistanceCruise, mount.cameraDistanceBoost));
    request.fovDegrees = StageValue(stage, mount.cameraFovStop, mount.cameraFovCruise, mount.cameraFovBoost);
    request.eyeFloor = std::max(0.0f, mount.cameraEyeFloor);
    request.rollDegrees = RollDegrees(mount, stage);
    request.lookDownDegrees = mount.diveLookDegrees * Clamp01(mount.flight.dive);
    request.shapeSeconds = std::max(0.0f, mount.cameraZoomSeconds);
    request.fovSeconds = std::max(0.0f, mount.cameraFovSeconds);
    request.rollSeconds = std::max(0.0f, mount.cameraRollSeconds);
    request.releaseSeconds = std::max(0.0f, mount.cameraReleaseSeconds);
    if (mount.flight.boosting && !mount.cameraWasBoosting)
        mount.cameraKickAge = 0.0f;
    mount.cameraWasBoosting = mount.flight.boosting;
    if (mount.cameraKickAge >= 0.0f) {
        request.fovKickDegrees = FovKick(mount, mount.cameraKickAge);
        request.fovSeconds =
            std::min(request.fovSeconds, std::max(0.0f, mount.cameraBoostFovKickSeconds) * kFovKickSmoothShare);
        mount.cameraKickAge += std::max(dt, 0.0f);
        if (mount.cameraKickAge >= mount.cameraBoostFovKickSeconds)
            mount.cameraKickAge = -1.0f;
    }
    return request;
}

PlayerCameraRequest CloudMountCamera::BuildFallRequest(const CloudMount& mount) {
    PlayerCameraRequest request{};
    request.verticalSeconds = std::max(0.0f, mount.cameraVerticalSeconds);
    request.verticalDeadZone = mount.cameraVerticalDeadZone;
    request.minPitch = mount.cameraMinPitch;
    request.maxPitch = mount.cameraMaxPitch;
    const float release = std::max(0.0f, mount.cameraReleaseSeconds);
    request.shapeSeconds = release;
    request.fovSeconds = release;
    request.rollSeconds = release;
    request.releaseSeconds = release;
    return request;
}

void CloudMountCamera::Reset(CloudMount& mount) {
    mount.cameraFollowNow = -1.0f;
    mount.cameraKickAge = -1.0f;
    mount.cameraWasBoosting = false;
}

float CloudMountCamera::StepBlur(CloudMount& mount, float dt) {
    const float target = std::max(mount.flight.boosting ? 1.0f : 0.0f, Clamp01(mount.flight.dive));
    mount.cameraBlurWeight = Clamp01(Approach(mount.cameraBlurWeight, target, mount.cameraBlurSeconds, dt));
    if (target <= 0.0f && mount.cameraBlurWeight < kBlurOffWeight)
        mount.cameraBlurWeight = 0.0f;
    return mount.cameraBlurWeight;
}

float CloudMountCamera::SpeedStage(const CloudMount& mount) {
    if (mount.flight.boosting)
        return 2.0f;
    const float cruise = std::max(mount.cruiseSpeed, kMinimumSpan);
    const float speed = std::max(mount.flight.speed, 0.0f);
    if (speed <= cruise)
        return speed / cruise;
    return 1.0f + Clamp01((speed - cruise) / std::max(mount.boostSpeed - cruise, kMinimumSpan));
}

float CloudMountCamera::FovKick(const CloudMount& mount, float age) {
    const float duration = mount.cameraBoostFovKickSeconds;
    if (duration <= 0.0f || age < 0.0f || age >= duration)
        return 0.0f;
    const float progress = age / duration;
    const float share = progress < kFovKickAttackShare
                            ? Smoothstep(progress / kFovKickAttackShare)
                            : 1.0f - Smoothstep((progress - kFovKickAttackShare) / (1.0f - kFovKickAttackShare));
    return mount.cameraBoostFovKick * share;
}

} // namespace Project_TD2_2
