#include "Feedback.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/CameraLensTypes.h"
#include "FeedbackCamera.h"
#include "../Common/VectorMath.h"

namespace Project_TD2_2 {
namespace Feedback {
namespace {

using VectorMath::kRadiansPerDegree;
using VectorMath::kTau;

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

bool SubmitCameraShake(HIKARI::WorldView& world, CameraShake& shake, HIKARI::Entity preferredCamera, float unscaledDt,
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
    modifier.cameraObjectHandle = FeedbackCamera::Resolve(world, preferredCamera);
    if (offsetOut != nullptr)
        *offsetOut = modifier.localPositionOffset;
    // 停止中も実時間で戻し、停止そのものが揺れを延ばさないようにする。
    shake.ageSeconds += std::max(0.0f, unscaledDt);
    if (shake.ageSeconds >= shake.durationSeconds)
        shake.active = false;
    return modifier.cameraObjectHandle != 0 && world.GetCamera().SubmitModifier(modifier);
}

bool SubmitMotionBlur(HIKARI::WorldView& world, uint64_t sourceId, int priority, float weight, float shutterDegrees,
                      float maxPixels) {
    HIKARI::GameProjectCameraMotionBlurRequest blur{};
    blur.cameraObjectHandle = 0; // いま画を出しているカメラ
    blur.sourceId = sourceId;
    blur.priority = priority;
    blur.weight = weight;
    blur.settings.enabled = 1;
    blur.settings.shutterAngleDegrees = std::clamp(shutterDegrees, 0.0f, 360.0f);
    blur.settings.maxBlur1080pPixels = std::clamp(maxPixels, 0.0f, 64.0f);
    return world.GetCamera().SubmitMotionBlur(blur);
}

// 一瞬のぼかしの、いまの強さ（0〜1。出した瞬間が 1、二乗に弱まる）。
static float BlurPulseWeight(const BlurPulse& pulse) {
    if (!pulse.active || !(pulse.durationSeconds > 0.0f))
        return 0.0f;
    const float envelope = std::clamp(1.0f - pulse.ageSeconds / pulse.durationSeconds, 0.0f, 1.0f);
    return envelope * envelope;
}

void StartBlurPulse(BlurPulse& pulse, const BlurPulseParams& params) {
    if (params.frames <= 0 || !(params.shutterDegrees > 0.0f) || !(params.maxPixels > 0.0f))
        return;
    if (pulse.params.shutterDegrees * BlurPulseWeight(pulse) > params.shutterDegrees)
        return;
    pulse.params = params;
    pulse.ageSeconds = 0.0f;
    pulse.durationSeconds = static_cast<float>(params.frames) / kFramesPerSecond;
    pulse.active = true;
}

bool SubmitBlur(HIKARI::WorldView& world, uint64_t sourceId, int priority, BlurPulse& pulse, float restShutterDegrees,
                float restPixels, float unscaledDt) {
    const float weight = BlurPulseWeight(pulse);
    // 停止中も実時間で戻す。
    if (pulse.active) {
        pulse.ageSeconds += std::max(0.0f, unscaledDt);
        if (pulse.ageSeconds >= pulse.durationSeconds)
            pulse.active = false;
    }
    const bool rest = restShutterDegrees > 0.0f && restPixels > 0.0f;
    const float restShutter = rest ? restShutterDegrees : 0.0f;
    const float restLength = rest ? restPixels : 0.0f;
    const float shutter = std::max(restShutter, restShutter + (pulse.params.shutterDegrees - restShutter) * weight);
    const float length = std::max(restLength, restLength + (pulse.params.maxPixels - restLength) * weight);
    if (!(shutter > 0.0f) || !(length > 0.0f))
        return false;
    return SubmitMotionBlur(world, sourceId, priority, 1.0f, shutter, length);
}

bool SubmitCameraOffset(HIKARI::WorldView& world, uint64_t sourceId, int priority,
                        HIKARI::GameProjectFloat3 localOffset) {
    HIKARI::GameProjectCameraModifier modifier{};
    modifier.sourceId = sourceId;
    modifier.priority = priority;
    modifier.localPositionOffset = localOffset;
    modifier.localTargetOffset = localOffset;
    modifier.cameraObjectHandle = FeedbackCamera::Resolve(world);
    return modifier.cameraObjectHandle != 0 && world.GetCamera().SubmitModifier(modifier);
}

} // namespace Feedback
} // namespace Project_TD2_2
