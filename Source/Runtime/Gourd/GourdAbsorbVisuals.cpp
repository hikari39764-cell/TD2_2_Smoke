#include "GourdAbsorbVisuals.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/VfxView.h"
#include "GourdRules.h"
#include "../Combat/FeedbackCamera.h"
#include "../Common/VectorMath.h"
#include "../Common/VfxParameters.h"

namespace Project_TD2_2 {
namespace {

constexpr float kFramesPerSecond = 60.0f;
constexpr float kTau = 6.28318530718f;
constexpr float kRadiansPerDegree = kTau / 360.0f;
// 縦の震えを横と違う周期にして、同じ円を描き続けないようにする。
constexpr float kShakeVerticalRatio = 1.37f;
constexpr uint64_t kCameraSource = 0x5444324801ull;
// 発射（310）と命中（320）の反動を、吸い込みの寄りより優先させる。
constexpr int kCameraPriority = 305;
// 吸い込み中の振動は毎フレーム延長する。入力が切れた後に残る長さ。
constexpr float kRumbleRefreshSeconds = 0.1f;

void UpdateEffect(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdComponent& gourd, bool absorbing,
                  const HIKARI::FrameContext& frame) {
    // 口の域は使う間だけ出す。同時に描ける流体の域には上限があり、弾と爆発の枠を空けておく。
    gourd.mouthIdleSeconds = absorbing ? 0.0f : gourd.mouthIdleSeconds + std::max(0.0f, frame.gameDt);
    const bool retired =
        gourd.mouthIdleSeconds >= static_cast<float>(gourd.absorbRetireFrames) / kFramesPerSecond;
    if (!gourd.enabled || !gourd.poseInitialized || gourd.absorbEffect.IsEmpty() || gourd.absorbEffectScale <= 0.0f ||
        (!absorbing && (retired || !gourd.absorbVfx.IsValid()))) {
        GourdAbsorbVisuals::Stop(world, gourd);
        return;
    }
    const auto vfx = world.GetVfx();
    HIKARI::GameProjectTransform transform{};
    transform.position = gourd.mouthPosition;
    // 吸い込み中の口は霧の集まる点を向いているので、口の向きがそのまま流れの軸（資産の +X）になる。
    transform.rotation = VectorMath::RotationFromXAxis(gourd.aimDirection);
    transform.scale = {gourd.absorbEffectScale, gourd.absorbEffectScale, gourd.absorbEffectScale};
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, "input.on", absorbing ? 1.0f : 0.0f);
    if (gourd.absorbVfx.IsValid() && !vfx.IsActive(gourd.absorbVfx))
        gourd.absorbVfx = {};
    if (!gourd.absorbVfx.IsValid()) {
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = gourd.absorbEffect;
        play.ownerObjectHandle = object.GetHandle().value;
        play.transform = transform;
        play.loopMode = HIKARI::GameProjectVfxLoopMode::Loop;
        play.parameters = parameters;
        gourd.absorbVfx = vfx.Play(play);
        gourd.vfxAccepted = gourd.absorbVfx.IsValid();
        return;
    }
    HIKARI::GameProjectVfxUpdateRequest update{};
    update.updateTransform = 1;
    update.updateParameters = 1;
    update.transform = transform;
    update.parameters = parameters;
    gourd.vfxAccepted = vfx.Update(gourd.absorbVfx, update);
}

void UpdateCamera(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdComponent& gourd, bool absorbing,
                  const HIKARI::FrameContext& frame) {
    const float step = std::max(0.0f, frame.unscaledDt) * kFramesPerSecond /
                       static_cast<float>(std::max(1, gourd.absorbFovEaseFrames));
    gourd.absorbBlend = std::clamp(gourd.absorbBlend + (absorbing ? step : -step), 0.0f, 1.0f);
    gourd.absorbSeconds = gourd.absorbBlend > 0.0f ? gourd.absorbSeconds + std::max(0.0f, frame.unscaledDt) : 0.0f;
    if (gourd.absorbBlend <= 0.0f || (gourd.absorbFovDegrees <= 0.0f && gourd.absorbShakeAmplitude <= 0.0f))
        return;
    HIKARI::GameProjectCameraModifier modifier{};
    modifier.cameraObjectHandle = FeedbackCamera::Resolve(world, gourd.cameraId);
    if (modifier.cameraObjectHandle == 0)
        return;
    modifier.sourceId = kCameraSource ^ object.GetHandle().value;
    modifier.priority = kCameraPriority;
    // 寄りと戻りの両端をなめらかにして、視野角の切り替わりを感じさせない。
    const float eased = gourd.absorbBlend * gourd.absorbBlend * (3.0f - 2.0f * gourd.absorbBlend);
    modifier.verticalFovOffsetRadians = -gourd.absorbFovDegrees * kRadiansPerDegree * eased;
    // 引き込まれる力を、細かい震えで伝える。視点と注視点を一緒にずらし、画面を傾けない。
    const float phase = kTau * gourd.absorbShakeFrequency * gourd.absorbSeconds;
    const float shake = gourd.absorbShakeAmplitude * eased;
    modifier.localPositionOffset = {std::sin(phase) * shake, std::sin(phase * kShakeVerticalRatio) * shake, 0.0f};
    modifier.localTargetOffset = modifier.localPositionOffset;
    world.GetCamera().SubmitModifier(modifier);
}

void UpdateRumble(HIKARI::WorldView& world, GourdComponent& gourd, bool absorbing) {
    const float capacity = GourdRules::GetCapacity(gourd);
    const bool full = capacity > 0.0f && GourdRules::GetSmoke(gourd) >= capacity;
    if (absorbing && full && !gourd.wasFull && gourd.fullRumbleFrames > 0 && gourd.fullRumble > 0.0f) {
        // 満杯の合図。これ以降は振動を止め、吸っても増えないことを手で分からせる。
        world.GetInput().SetGamepadVibration(gourd.fullRumble, gourd.fullRumble,
                                             static_cast<float>(gourd.fullRumbleFrames) / kFramesPerSecond);
    } else if (absorbing && !full && gourd.absorbRumble > 0.0f) {
        world.GetInput().SetGamepadVibration(gourd.absorbRumble, 0.0f, kRumbleRefreshSeconds);
    }
    gourd.wasFull = full;
}

} // namespace

void GourdAbsorbVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdComponent& gourd,
                                const HIKARI::FrameContext& frame) {
    const bool absorbing = gourd.enabled && gourd.absorbing && gourd.poseInitialized;
    UpdateEffect(world, object, gourd, absorbing, frame);
    UpdateCamera(world, object, gourd, absorbing, frame);
    UpdateRumble(world, gourd, absorbing);
}

void GourdAbsorbVisuals::Stop(HIKARI::WorldView& world, GourdComponent& gourd) {
    if (!gourd.absorbVfx.IsValid())
        return;
    const auto vfx = world.GetVfx();
    if (vfx.IsActive(gourd.absorbVfx))
        vfx.Stop(gourd.absorbVfx);
    gourd.absorbVfx = {};
}

} // namespace Project_TD2_2
