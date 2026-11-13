#include "DamageableSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/ComponentViews.h"
#include "HIKARI/Project/ProjectLog.h"
#include "DamageableComponent.h"
#include "Feedback.h"
#include "FeedbackCamera.h"

namespace Project_TD2_2 {
namespace {

constexpr float kFramesPerSecond = Feedback::kFramesPerSecond;
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

// 光りを始める。光っている最中なら、残りと新しい長さの長い方を取る（命中が崩しの光りを縮めない）。
void StartFlash(HIKARI::ObjectView object, DamageableComponent& target, int frames) {
    if (frames <= 0 || target.hitFlashStrength <= 0.0f)
        return;
    if (!target.flashActive) {
        target.flashMaterialIndex = static_cast<uint32_t>(target.hitMaterialIndex);
        target.flashSourceValid = object.GetModel().TryGetMaterialColor(target.flashMaterialIndex, target.flashSource);
    }
    const float remaining =
        target.flashActive ? std::max(0.0f, target.flashDurationSeconds - target.flashAgeSeconds) : 0.0f;
    target.flashDurationSeconds = std::max(static_cast<float>(frames) / kFramesPerSecond, remaining);
    target.flashAgeSeconds = 0.0f;
}

void BeginFeedback(HIKARI::WorldView& world, HIKARI::ObjectView object, DamageableComponent& target,
                   const HIKARI::FrameContext& frame) {
    // 前の止めがまだ続いている間の命中では、止めを延ばさず作り直さない（連射で止めが積み上がらない）。
    const bool stopRunning = target.hitStopToken != 0 && target.feedbackActive &&
                             target.feedbackAgeSeconds < static_cast<float>(target.hitStopFrames) / kFramesPerSecond;
    target.feedbackHitCount = target.hitCount;
    target.feedbackAgeSeconds = 0.0f;
    target.feedbackActive = true;
    // 威力が減った弾（距離で減衰した弾）は、カメラも同じ割合で弱く揺らす。
    target.hitCameraScale = std::clamp(target.lastRawDamage / std::max(1.0f, target.hitFeedbackFullDamage),
                                       std::clamp(target.hitFeedbackMinScale, 0.0f, 1.0f), 1.0f);
    bool stopRequested = false;
    if (!stopRunning) {
        if (target.hitStopToken != 0)
            world.GetTimeDilation().Release(target.hitStopToken);
        target.hitStopToken = Feedback::RequestHitStop(world, kHitStopSource ^ object.GetHandle().value,
                                                       kFeedbackPriority, target.hitStopFrames, target.hitStopScale);
        stopRequested = target.hitStopToken != 0;
    }
    StartFlash(object, target, target.hitFlashFrames);
    const bool rumbleAccepted =
        Feedback::Rumble(world, target.hitRumbleLow, target.hitRumbleHigh, target.hitRumbleFrames);
    if (target.traceEveryHit) {
        char message[448]{};
        std::snprintf(message, sizeof(message),
                      "Start tick=%llu count=%llu stopFrames=%d stopScale=%.6f token=%llu stopRunning=%d "
                      "stopRequested=%d effective=%.6f cameraAmplitude=%.6f cameraScale=%.6f flashStrength=%.6f "
                      "flashSource=%d rumble=%d",
                      static_cast<unsigned long long>(frame.fixedTickIndex),
                      static_cast<unsigned long long>(target.hitCount), target.hitStopFrames,
                      static_cast<double>(target.hitStopScale), static_cast<unsigned long long>(target.hitStopToken),
                      static_cast<int>(stopRunning), static_cast<int>(stopRequested),
                      static_cast<double>(world.GetTimeDilation().GetEffectiveScale()),
                      static_cast<double>(target.hitCameraAmplitude), static_cast<double>(target.hitCameraScale),
                      static_cast<double>(target.hitFlashStrength), static_cast<int>(target.flashSourceValid),
                      static_cast<int>(rumbleAccepted));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "DamageableFeedback", message);
    }
}

bool ApplyFlash(HIKARI::ObjectView object, DamageableComponent& target) {
    const float duration = target.flashDurationSeconds;
    if (duration <= 0.0f || target.flashAgeSeconds >= duration || target.hitFlashStrength <= 0.0f ||
        !target.flashSourceValid) {
        target.flashClearAccepted = ClearFlash(object, target);
        return false;
    }
    const float envelope = std::clamp(1.0f - target.flashAgeSeconds / duration, 0.0f, 1.0f);
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
    const float strength = target.hitCameraAmplitude * falloff * target.hitCameraScale;
    const float kick = target.hitCameraKickDistance * falloff * target.hitCameraScale;
    modifier.sourceId = kCameraSource ^ object.GetHandle().value;
    modifier.priority = kFeedbackPriority;
    // 視点と注視点を一緒にずらし、傾きで酔わせない。攻撃の向きへ押してから戻すと、当たった重さが伝わる。
    modifier.localPositionOffset = {std::cos(phase) * strength, std::sin(phase) * strength, 0.0f};
    modifier.localTargetOffset = modifier.localPositionOffset;
    modifier.worldPositionOffset = {target.lastDirection.x * kick, target.lastDirection.y * kick,
                                    target.lastDirection.z * kick};
    modifier.worldTargetOffset = modifier.worldPositionOffset;
    modifier.verticalFovOffsetRadians = target.hitCameraFovDegrees * kRadiansPerDegree * falloff * target.hitCameraScale;
    modifier.cameraObjectHandle = FeedbackCamera::Resolve(world, target.feedbackCameraId);
    if (modifier.cameraObjectHandle == 0)
        return false;
    return world.GetCamera().SubmitModifier(modifier);
}

// 光りは手応えの残りとは別に進める：持ち主が頼む光り（ボスの崩し・気絶）は、命中が無くても出る。
bool StepFlash(HIKARI::ObjectView object, DamageableComponent& target, const HIKARI::FrameContext& frame) {
    if (target.pendingFlashFrames > 0) {
        StartFlash(object, target, target.pendingFlashFrames);
        target.pendingFlashFrames = 0;
    }
    if (!target.flashActive && target.flashAgeSeconds >= target.flashDurationSeconds)
        return false;
    const bool accepted = ApplyFlash(object, target);
    if (target.traceEveryHit) {
        char message[160]{};
        std::snprintf(message, sizeof(message), "Flash tick=%llu active=%d accepted=%d age=%.6f duration=%.6f",
                      static_cast<unsigned long long>(frame.fixedTickIndex), static_cast<int>(target.flashActive),
                      static_cast<int>(accepted), static_cast<double>(target.flashAgeSeconds),
                      static_cast<double>(target.flashDurationSeconds));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "DamageableFeedback", message);
    }
    target.flashAgeSeconds += std::max(0.0f, frame.unscaledDt);
    return accepted;
}

void StepFeedback(HIKARI::WorldView& world, HIKARI::ObjectView object, DamageableComponent& target,
                  const HIKARI::FrameContext& frame, bool flashAccepted) {
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
    const int durationFrames = std::max({target.hitStopFrames, target.hitCameraFrames, target.hitChromaFrames});
    if (target.feedbackAgeSeconds >= static_cast<float>(durationFrames) / kFramesPerSecond)
        target.feedbackActive = false;
}

} // namespace

void DamageableSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (world.GetGameSession() == nullptr || frame.paused)
        return;
    world.ForEachComponent<DamageableComponent>(
        HIKARI::MakeComponentTypeId(kDamageableType),
        [&](HIKARI::ObjectView object, DamageableComponent& target) {
            if (target.feedbackHitCount != target.hitCount)
                BeginFeedback(world, object, target, frame);
            const bool flashAccepted = StepFlash(object, target, frame);
            if (target.feedbackActive)
                StepFeedback(world, object, target, frame, flashAccepted);
        });
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
