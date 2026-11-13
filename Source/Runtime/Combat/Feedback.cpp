#include "Feedback.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/ActionViews.h"
#include "FeedbackCamera.h"

namespace Project_TD2_2 {
namespace Feedback {
namespace {

constexpr float kTau = 6.28318530718f;
constexpr float kRadiansPerDegree = kTau / 360.0f;

} // namespace

uint64_t RequestHitStop(HIKARI::WorldView& world, uint64_t sourceId, int priority, int frames, float scale) {
    if (frames <= 0 || scale >= 1.0f)
        return 0;
    HIKARI::GameProjectTimeDilationRequest request{};
    request.sourceId = sourceId;
    request.priority = priority;
    request.scale = std::max(0.0f, scale);
    request.durationUnscaledSeconds = static_cast<float>(frames) / kFramesPerSecond;
    return world.GetTimeDilation().Request(request);
}

bool Rumble(HIKARI::WorldView& world, float low, float high, int frames) {
    if (frames <= 0 || (low <= 0.0f && high <= 0.0f))
        return false;
    return world.GetInput().SetGamepadVibration(low, high, static_cast<float>(frames) / kFramesPerSecond);
}

void StartCameraShake(CameraShake& shake, const CameraShakeParams& params) {
    shake.active = false;
    if (params.frames <= 0 || (params.amplitude <= 0.0f && params.kickDistance <= 0.0f && params.fovDegrees == 0.0f))
        return;
    shake.params = params;
    shake.ageSeconds = 0.0f;
    shake.durationSeconds = static_cast<float>(params.frames) / kFramesPerSecond;
    shake.active = true;
}

bool SubmitCameraShake(HIKARI::WorldView& world, CameraShake& shake, uint64_t preferredCameraId, float unscaledDt,
                       HIKARI::GameProjectFloat3* offsetOut) {
    if (!shake.active)
        return false;
    const auto& params = shake.params;
    const float envelope = std::clamp(1.0f - shake.ageSeconds / shake.durationSeconds, 0.0f, 1.0f);
    const float falloff = envelope * envelope;
    const float phase = kTau * params.frequency * shake.ageSeconds;
    const float strength = params.amplitude * falloff;
    const float kick = params.kickDistance * falloff;
    HIKARI::GameProjectCameraModifier modifier{};
    modifier.sourceId = params.sourceId;
    modifier.priority = params.priority;
    // 視点と注視点を一緒にずらし、傾きで酔わせない。
    modifier.localPositionOffset = {std::cos(phase) * strength, std::sin(phase) * strength, 0.0f};
    modifier.localTargetOffset = modifier.localPositionOffset;
    modifier.worldPositionOffset = {params.kickDirection.x * kick, params.kickDirection.y * kick,
                                    params.kickDirection.z * kick};
    modifier.worldTargetOffset = modifier.worldPositionOffset;
    modifier.verticalFovOffsetRadians = params.fovDegrees * kRadiansPerDegree * falloff;
    modifier.cameraObjectHandle = FeedbackCamera::Resolve(world, preferredCameraId);
    if (offsetOut != nullptr)
        *offsetOut = modifier.localPositionOffset;
    // 停止中も実時間で戻し、停止そのものが揺れを延ばさないようにする。
    shake.ageSeconds += std::max(0.0f, unscaledDt);
    if (shake.ageSeconds >= shake.durationSeconds)
        shake.active = false;
    return modifier.cameraObjectHandle != 0 && world.GetCamera().SubmitModifier(modifier);
}

} // namespace Feedback
} // namespace Project_TD2_2
