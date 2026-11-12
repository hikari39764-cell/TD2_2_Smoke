#include "GourdFeedbackSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/ProjectLog.h"
#include "GourdShotComponent.h"
#include "GourdComponent.h"
#include "../Combat/FeedbackCamera.h"

namespace Project_TD2_2 {
namespace {

constexpr float kFramesPerSecond = 60.0f;
constexpr float kTau = 6.28318530718f;
constexpr float kRadiansPerDegree = kTau / 360.0f;
constexpr uint64_t kCameraSource = 0x5444324701ull;
constexpr int kCameraPriority = 310;

void LogStart(const GourdShotComponent& shot) {
    if (shot.traceEveryFrames <= 0)
        return;
    const auto& feedback = shot.feedback;
    char message[256]{};
    std::snprintf(message, sizeof(message), "Start tick=%llu shot=%llu frames=%d kick=%.6f shake=%.6f active=%d",
                  static_cast<unsigned long long>(shot.processedTick), static_cast<unsigned long long>(shot.shotsFired),
                  feedback.cameraFrames, static_cast<double>(feedback.cameraKickDistance),
                  static_cast<double>(feedback.cameraShakeAmplitude), static_cast<int>(feedback.active));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "GourdFeedback", message);
}

bool SubmitCamera(HIKARI::WorldView& world, HIKARI::ObjectView object, const GourdFeedback& feedback,
                  HIKARI::GameProjectCameraModifier& modifier) {
    const float duration = static_cast<float>(feedback.cameraFrames) / kFramesPerSecond;
    const float envelope = std::clamp(1.0f - feedback.ageSeconds / duration, 0.0f, 1.0f);
    const float strength = envelope * envelope;
    const float phase = kTau * feedback.cameraShakeFrequency * feedback.ageSeconds;
    const float shake = feedback.cameraShakeAmplitude * strength;
    const float kick = feedback.cameraKickDistance * strength;
    modifier.sourceId = kCameraSource ^ object.GetHandle().value;
    modifier.priority = kCameraPriority;
    modifier.worldPositionOffset = {-feedback.direction.x * kick, -feedback.direction.y * kick,
                                    -feedback.direction.z * kick};
    modifier.worldTargetOffset = modifier.worldPositionOffset;
    modifier.localPositionOffset = {std::sin(phase) * shake, std::cos(phase) * shake, 0.0f};
    modifier.localTargetOffset = modifier.localPositionOffset;
    modifier.verticalFovOffsetRadians = feedback.cameraFovDegrees * kRadiansPerDegree * strength;
    uint64_t cameraId = 0;
    world.ForEachComponent<GourdComponent>(HIKARI::MakeComponentTypeId(kGourdType),
                                           [&](HIKARI::ObjectView gourdObject, GourdComponent& gourd) {
                                               if (gourdObject.GetHandle().value == object.GetHandle().value)
                                                   cameraId = gourd.cameraId;
                                           });
    modifier.cameraObjectHandle = FeedbackCamera::Resolve(world, cameraId);
    if (modifier.cameraObjectHandle == 0)
        return false;
    return world.GetCamera().SubmitModifier(modifier);
}

void StepFeedback(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot,
                  const HIKARI::FrameContext& frame) {
    auto& feedback = shot.feedback;
    const bool hasCameraMotion = feedback.cameraKickDistance > 0.0f || feedback.cameraShakeAmplitude > 0.0f ||
                                 feedback.cameraFovDegrees > 0.0f;
    if (feedback.observedDryFires != shot.dryFires) {
        feedback.observedDryFires = shot.dryFires;
        if (feedback.dryFireRumbleFrames > 0 && feedback.dryFireRumble > 0.0f)
            world.GetInput().SetGamepadVibration(0.0f, feedback.dryFireRumble,
                                                 static_cast<float>(feedback.dryFireRumbleFrames) / kFramesPerSecond);
    }
    if (feedback.observedShots != shot.shotsFired) {
        feedback.observedShots = shot.shotsFired;
        feedback.ageSeconds = 0.0f;
        feedback.direction = shot.muzzleDirection;
        feedback.active = feedback.cameraFrames > 0 && hasCameraMotion;
        if (feedback.rumbleFrames > 0 && (feedback.rumbleLow > 0.0f || feedback.rumbleHigh > 0.0f))
            world.GetInput().SetGamepadVibration(feedback.rumbleLow, feedback.rumbleHigh,
                                                 static_cast<float>(feedback.rumbleFrames) / kFramesPerSecond);
        LogStart(shot);
    }
    const float duration = static_cast<float>(feedback.cameraFrames) / kFramesPerSecond;
    if (!feedback.active || duration <= 0.0f || feedback.ageSeconds >= duration || !hasCameraMotion) {
        feedback.active = false;
        return;
    }
    HIKARI::GameProjectCameraModifier modifier{};
    const bool accepted = SubmitCamera(world, object, feedback, modifier);
    if (shot.traceEveryFrames > 0) {
        char message[384]{};
        std::snprintf(
            message, sizeof(message),
            "Camera tick=%llu age=%.6f accepted=%d camera=%llu world=(%.6f,%.6f,%.6f) "
            "local=(%.6f,%.6f,%.6f)",
            static_cast<unsigned long long>(shot.processedTick), static_cast<double>(feedback.ageSeconds),
            static_cast<int>(accepted), static_cast<unsigned long long>(modifier.cameraObjectHandle),
            static_cast<double>(modifier.worldPositionOffset.x), static_cast<double>(modifier.worldPositionOffset.y),
            static_cast<double>(modifier.worldPositionOffset.z), static_cast<double>(modifier.localPositionOffset.x),
            static_cast<double>(modifier.localPositionOffset.y), static_cast<double>(modifier.localPositionOffset.z));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "GourdFeedback", message);
    }
    // 停止中も実時間で戻し、停止を再び延長しない。
    feedback.ageSeconds += std::max(0.0f, frame.unscaledDt);
}

} // namespace

void GourdFeedbackSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (world.GetGameSession() == nullptr || frame.paused)
        return;
    world.ForEachComponent<GourdShotComponent>(
        HIKARI::MakeComponentTypeId(kGourdShotType),
        [&](HIKARI::ObjectView object, GourdShotComponent& shot) { StepFeedback(world, object, shot, frame); });
}

void GourdFeedbackSystem::PostRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (world.GetGameSession() == nullptr || frame.paused)
        return;
    world.ForEachComponent<GourdShotComponent>(
        HIKARI::MakeComponentTypeId(kGourdShotType), [&](HIKARI::ObjectView, GourdShotComponent& shot) {
            if (shot.traceEveryFrames <= 0 || !shot.feedback.active)
                return;
            HIKARI::GameProjectCameraPose control{}, rendered{};
            const bool controlOk = world.GetCamera().GetControlPose(control);
            const bool renderedOk = world.GetCamera().GetRenderedPose(rendered);
            char message[384]{};
            std::snprintf(message, sizeof(message),
                          "Pose tick=%llu controlOk=%d renderedOk=%d control=(%.6f,%.6f,%.6f) "
                          "rendered=(%.6f,%.6f,%.6f) timeScale=%.6f",
                          static_cast<unsigned long long>(shot.processedTick), static_cast<int>(controlOk),
                          static_cast<int>(renderedOk), static_cast<double>(control.eye.x),
                          static_cast<double>(control.eye.y), static_cast<double>(control.eye.z),
                          static_cast<double>(rendered.eye.x), static_cast<double>(rendered.eye.y),
                          static_cast<double>(rendered.eye.z),
                          static_cast<double>(world.GetTimeDilation().GetEffectiveScale()));
            HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "GourdFeedback", message);
        });
}

} // namespace Project_TD2_2
