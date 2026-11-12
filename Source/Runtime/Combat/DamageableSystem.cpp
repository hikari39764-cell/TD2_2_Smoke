#include "DamageableSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/ComponentViews.h"
#include "HIKARI/Project/ProjectLog.h"
#include "DamageableComponent.h"
#include "FeedbackCamera.h"

namespace Project_TD2_2 {
namespace {

constexpr float kFramesPerSecond = 60.0f;
constexpr float kTau = 6.28318530718f;
constexpr float kRadiansPerDegree = kTau / 360.0f;
constexpr uint64_t kHitStopSource = 0x5444324401ull;
constexpr uint64_t kCameraSource = 0x5444324402ull;
constexpr int kFeedbackPriority = 320;
constexpr uint64_t kChromaSource = 0x5444324403ull;
constexpr int kChromaPriority = 320;

bool ClearFlash(HIKARI::ObjectView object, DamageableComponent& target) {
    if (!target.flashActive)
        return true;
    const bool accepted = object.GetModel().ClearMaterialColorOverride(target.flashMaterialIndex).Accepted();
    if (accepted)
        target.flashActive = false;
    return accepted;
}

void BeginFeedback(HIKARI::WorldView& world, HIKARI::ObjectView object, DamageableComponent& target,
                   const HIKARI::FrameContext& frame) {
    target.feedbackHitCount = target.hitCount;
    target.feedbackAgeSeconds = 0.0f;
    target.feedbackActive = true;
    const bool released = target.hitStopToken == 0 || world.GetTimeDilation().Release(target.hitStopToken);
    target.hitStopToken = 0;
    if (target.hitStopFrames > 0 && target.hitStopScale < 1.0f) {
        HIKARI::GameProjectTimeDilationRequest request{};
        request.sourceId = kHitStopSource ^ object.GetHandle().value;
        request.priority = kFeedbackPriority;
        request.scale = target.hitStopScale;
        request.durationUnscaledSeconds = static_cast<float>(target.hitStopFrames) / kFramesPerSecond;
        target.hitStopToken = world.GetTimeDilation().Request(request);
    }
    if (!target.flashActive) {
        target.flashMaterialIndex = static_cast<uint32_t>(target.hitMaterialIndex);
        target.flashSourceValid = target.hitFlashFrames > 0 && target.hitFlashStrength > 0.0f &&
                                  object.GetModel().TryGetMaterialColor(target.flashMaterialIndex, target.flashSource);
    }
    const bool rumbleAccepted =
        target.hitRumbleFrames > 0 && (target.hitRumbleLow > 0.0f || target.hitRumbleHigh > 0.0f) &&
        world.GetInput().SetGamepadVibration(target.hitRumbleLow, target.hitRumbleHigh,
                                             static_cast<float>(target.hitRumbleFrames) / kFramesPerSecond);
    if (target.traceEveryHit) {
        char message[384]{};
        std::snprintf(message, sizeof(message),
                      "Start tick=%llu count=%llu stopFrames=%d stopScale=%.6f token=%llu previousReleased=%d "
                      "effective=%.6f cameraAmplitude=%.6f flashStrength=%.6f flashSource=%d rumble=%d",
                      static_cast<unsigned long long>(frame.fixedTickIndex),
                      static_cast<unsigned long long>(target.hitCount), target.hitStopFrames,
                      static_cast<double>(target.hitStopScale), static_cast<unsigned long long>(target.hitStopToken),
                      static_cast<int>(released), static_cast<double>(world.GetTimeDilation().GetEffectiveScale()),
                      static_cast<double>(target.hitCameraAmplitude), static_cast<double>(target.hitFlashStrength),
                      static_cast<int>(target.flashSourceValid), static_cast<int>(rumbleAccepted));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "DamageableFeedback", message);
    }
}

bool ApplyFlash(HIKARI::ObjectView object, DamageableComponent& target) {
    const float duration = static_cast<float>(target.hitFlashFrames) / kFramesPerSecond;
    if (duration <= 0.0f || target.feedbackAgeSeconds >= duration || target.hitFlashStrength <= 0.0f ||
        !target.flashSourceValid) {
        target.flashClearAccepted = ClearFlash(object, target);
        return false;
    }
    const float envelope = std::clamp(1.0f - target.feedbackAgeSeconds / duration, 0.0f, 1.0f);
    const bool accepted = object.GetModel()
                              .SetMaterialColorOverride(target.flashMaterialIndex, target.flashSource.baseColor,
                                                        target.flashSource.opacity, target.hitFlashColor,
                                                        target.hitFlashStrength * envelope)
                              .Accepted();
    target.flashActive = accepted || target.flashActive;
    return accepted;
}

bool ApplyCamera(HIKARI::WorldView& world, HIKARI::ObjectView object, const DamageableComponent& target,
                 HIKARI::GameProjectCameraModifier& modifier) {
    const float duration = static_cast<float>(target.hitCameraFrames) / kFramesPerSecond;
    if (duration <= 0.0f || target.feedbackAgeSeconds >= duration ||
        (target.hitCameraAmplitude <= 0.0f && target.hitCameraKickDistance <= 0.0f &&
         target.hitCameraFovDegrees <= 0.0f))
        return false;
    const float envelope = std::clamp(1.0f - target.feedbackAgeSeconds / duration, 0.0f, 1.0f);
    const float falloff = envelope * envelope;
    const float phase = kTau * target.hitCameraFrequency * target.feedbackAgeSeconds;
    const float strength = target.hitCameraAmplitude * falloff;
    const float kick = target.hitCameraKickDistance * falloff;
    modifier.sourceId = kCameraSource ^ object.GetHandle().value;
    modifier.priority = kFeedbackPriority;
    // 視点と注視点を一緒にずらし、傾きで酔わせない。攻撃の向きへ押してから戻すと、当たった重さが伝わる。
    modifier.localPositionOffset = {std::cos(phase) * strength, std::sin(phase) * strength, 0.0f};
    modifier.localTargetOffset = modifier.localPositionOffset;
    modifier.worldPositionOffset = {target.lastDirection.x * kick, target.lastDirection.y * kick,
                                    target.lastDirection.z * kick};
    modifier.worldTargetOffset = modifier.worldPositionOffset;
    modifier.verticalFovOffsetRadians = target.hitCameraFovDegrees * kRadiansPerDegree * falloff;
    modifier.cameraObjectHandle = FeedbackCamera::Resolve(world, target.feedbackCameraId);
    if (modifier.cameraObjectHandle == 0)
        return false;
    return world.GetCamera().SubmitModifier(modifier);
}

void StepFeedback(HIKARI::WorldView& world, HIKARI::ObjectView object, DamageableComponent& target,
                  const HIKARI::FrameContext& frame) {
    if (target.feedbackHitCount != target.hitCount)
        BeginFeedback(world, object, target, frame);
    if (!target.feedbackActive)
        return;
    const bool flashAccepted = ApplyFlash(object, target);
    HIKARI::GameProjectCameraModifier modifier{};
    const bool cameraAccepted = ApplyCamera(world, object, target, modifier);
    // 命中の瞬間だけ、画面の色をずらしてコントラストを上げる（毎フレーム出し直す。時間が過ぎれば自然に消える）。
    const float chromaSeconds = static_cast<float>(target.hitChromaFrames) / kFramesPerSecond;
    if (chromaSeconds > 0.0f && target.feedbackAgeSeconds < chromaSeconds && target.hitChromaPixels > 0.0f) {
        const float fade = 1.0f - target.feedbackAgeSeconds / chromaSeconds;
        HIKARI::GameProjectActionCinematicGradeRequest grade{};
        grade.sourceId = kChromaSource ^ object.GetHandle().value;
        grade.priority = kChromaPriority;
        grade.weight = fade;
        grade.chromaticSeparationPixels = target.hitChromaPixels * fade;
        grade.contrastStrength = target.hitChromaContrast * fade;
        world.GetPresentationVfx().SubmitActionCinematicGrade(grade);
    }
    if (target.traceEveryHit) {
        char message[384]{};
        std::snprintf(
            message, sizeof(message),
            "Visual tick=%llu count=%llu age=%.6f cameraAccepted=%d camera=%llu "
            "offset=(%.6f,%.6f,%.6f) fov=%.6f flashActive=%d flashAccepted=%d flashClear=%d effective=%.6f",
            static_cast<unsigned long long>(frame.fixedTickIndex), static_cast<unsigned long long>(target.hitCount),
            static_cast<double>(target.feedbackAgeSeconds), static_cast<int>(cameraAccepted),
            static_cast<unsigned long long>(modifier.cameraObjectHandle),
            static_cast<double>(modifier.localPositionOffset.x), static_cast<double>(modifier.localPositionOffset.y),
            static_cast<double>(modifier.localPositionOffset.z),
            static_cast<double>(modifier.verticalFovOffsetRadians), static_cast<int>(target.flashActive),
            static_cast<int>(flashAccepted), static_cast<int>(target.flashClearAccepted),
            static_cast<double>(world.GetTimeDilation().GetEffectiveScale()));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "DamageableFeedback", message);
    }
    // ヒットストップ中も実時間で消える。固定 tick の戦闘ルールへ追加の停止を挟まない。
    target.feedbackAgeSeconds += std::max(0.0f, frame.unscaledDt);
    const int durationFrames = std::max({target.hitStopFrames, target.hitCameraFrames, target.hitFlashFrames});
    if (target.feedbackAgeSeconds >= static_cast<float>(durationFrames) / kFramesPerSecond && !target.flashActive)
        target.feedbackActive = false;
}

} // namespace

void DamageableSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (world.GetGameSession() == nullptr || frame.paused)
        return;
    world.ForEachComponent<DamageableComponent>(
        HIKARI::MakeComponentTypeId(kDamageableType),
        [&](HIKARI::ObjectView object, DamageableComponent& target) { StepFeedback(world, object, target, frame); });
}

void DamageableSystem::PostRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (world.GetGameSession() == nullptr || frame.paused)
        return;
    world.ForEachComponent<DamageableComponent>(
        HIKARI::MakeComponentTypeId(kDamageableType), [&](HIKARI::ObjectView object, DamageableComponent& target) {
            if (!target.traceEveryHit || !target.feedbackActive)
                return;
            HIKARI::GameProjectCameraPose control{}, rendered{};
            const bool controlOk = world.GetCamera().GetControlPose(control);
            const bool renderedOk = world.GetCamera().GetRenderedPose(rendered);
            char message[384]{};
            std::snprintf(message, sizeof(message),
                          "Pose tick=%llu object=%llu controlOk=%d renderedOk=%d control=(%.6f,%.6f,%.6f) "
                          "rendered=(%.6f,%.6f,%.6f) timeScale=%.6f",
                          static_cast<unsigned long long>(frame.fixedTickIndex),
                          static_cast<unsigned long long>(object.GetSceneObjectId()), static_cast<int>(controlOk),
                          static_cast<int>(renderedOk), static_cast<double>(control.eye.x),
                          static_cast<double>(control.eye.y), static_cast<double>(control.eye.z),
                          static_cast<double>(rendered.eye.x), static_cast<double>(rendered.eye.y),
                          static_cast<double>(rendered.eye.z),
                          static_cast<double>(world.GetTimeDilation().GetEffectiveScale()));
            HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "DamageableFeedback", message);
        });
}

void DamageableSystem::OnWorldDetached(HIKARI::WorldView& world) {
    world.ForEachComponent<DamageableComponent>(
        HIKARI::MakeComponentTypeId(kDamageableType), [&](HIKARI::ObjectView object, DamageableComponent& target) {
            const bool cleared = ClearFlash(object, target);
            const bool released = target.hitStopToken == 0 || world.GetTimeDilation().Release(target.hitStopToken);
            target.hitStopToken = 0;
            if (target.traceEveryHit && (!cleared || !released))
                HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "DamageableFeedback",
                                        "Cleanup: flash unavailable or stop token already expired");
        });
}

} // namespace Project_TD2_2
