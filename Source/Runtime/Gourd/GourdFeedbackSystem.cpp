#include "GourdFeedbackSystem.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/ProjectLog.h"
#include "GourdShotComponent.h"
#include "GourdComponent.h"
#include "GourdRules.h"
#include "GourdShotVisuals.h"
#include "../Common/ComponentLookup.h"
#include "../Common/GameSession.h"
#include "../Combat/Feedback.h"
#include "../Combat/FeedbackCamera.h"
#include "../Common/VectorMath.h"
#include "../Common/VfxPlayback.h"

namespace Project_TD2_2 {
namespace {

using ComponentLookup::ForEachOnObject;

constexpr float kFramesPerSecond = Feedback::kFramesPerSecond;
using VectorMath::kTau;
using VectorMath::kRadiansPerDegree;
constexpr uint64_t kCameraSource = 0x5444324701ull;
constexpr int kCameraPriority = 310;
constexpr uint64_t kCatchHitStopSource = 0x5444324703ull;
constexpr uint64_t kCatchCameraSource = 0x5444324704ull;
// 色ずれ（動作の色調）の出どころと優先度。命中（320）が発射より勝つ。
constexpr uint64_t kChromaSource = 0x5444324702ull;
constexpr int kChromaPriority = 310;

void LogStart(const GourdShotComponent& shot) {
    if (shot.traceEveryFrames <= 0)
        return;
    const auto& feedback = shot.feedback;
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "Start tick=%llu shot=%llu frames=%d kick=%.6f shake=%.6f active=%d fire=%d cameraScale=%.3f",
                  static_cast<unsigned long long>(shot.processedTick), static_cast<unsigned long long>(shot.shotsFired),
                  feedback.cameraFrames, static_cast<double>(feedback.cameraKickDistance * feedback.cameraScale),
                  static_cast<double>(feedback.cameraShakeAmplitude), static_cast<int>(feedback.active),
                  static_cast<int>(shot.lastShotFire), static_cast<double>(feedback.cameraScale));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "GourdFeedback", message);
}

bool SubmitCamera(HIKARI::WorldView& world, HIKARI::ObjectView object, const GourdFeedback& feedback,
                  HIKARI::GameProjectCameraModifier& modifier) {
    const float duration = static_cast<float>(feedback.cameraFrames) / kFramesPerSecond;
    const float envelope = std::clamp(1.0f - feedback.ageSeconds / duration, 0.0f, 1.0f);
    const float strength = envelope * envelope;
    const float phase = kTau * feedback.cameraShakeFrequency * feedback.ageSeconds;
    const float shake = feedback.cameraShakeAmplitude * strength;
    // 火弾は後退幅と視野角だけを強める（cameraScale）。揺れ幅は変えない。
    const float kick = feedback.cameraKickDistance * feedback.cameraScale * strength;
    modifier.sourceId = kCameraSource ^ object.GetHandle().value;
    modifier.priority = kCameraPriority;
    modifier.worldPositionOffset = {-feedback.direction.x * kick, -feedback.direction.y * kick,
                                    -feedback.direction.z * kick};
    modifier.worldTargetOffset = modifier.worldPositionOffset;
    modifier.localPositionOffset = {std::sin(phase) * shake, std::cos(phase) * shake, 0.0f};
    modifier.localTargetOffset = modifier.localPositionOffset;
    modifier.verticalFovOffsetRadians = feedback.cameraFovDegrees * feedback.cameraScale * kRadiansPerDegree * strength;
    uint64_t cameraId = 0;
    ForEachOnObject<GourdComponent>(world, kGourdType, object,
                                    [&](HIKARI::ObjectView, GourdComponent& gourd) { cameraId = gourd.cameraId; });
    modifier.cameraObjectHandle = FeedbackCamera::Resolve(world, cameraId);
    if (modifier.cameraObjectHandle == 0)
        return false;
    return world.GetCamera().SubmitModifier(modifier);
}

// 火弾の状態が始まった瞬間に、ヒョウタンの口から短く炎を噴く合図を 1 回だけ出す（残りを延ばしただけの時は出さない）。
// 合図は砲撃の一度きりのエフェクトの枠（砲口の枠）で出すので、流体域の数は増えない。
void StepFireStart(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot) {
    bool fire = false;
    HIKARI::GameProjectFloat3 mouth{}, aim{0.0f, 0.0f, 1.0f};
    ForEachOnObject<GourdComponent>(world, kGourdType, object, [&](HIKARI::ObjectView, GourdComponent& gourd) {
        fire = GourdRules::IsFireShot(gourd);
        mouth = GourdRules::GetMouthPosition(gourd);
        aim = GourdRules::GetAimDirection(gourd);
    });
    const bool started = fire && !shot.feedback.observedFireShot;
    shot.feedback.observedFireShot = fire;
    if (!started)
        return;
    GourdShotVisuals::QueueBurst(shot, GourdBurstKind::FireStart, true, mouth, aim, 1.0f);
    if (shot.traceEveryFrames > 0) {
        char message[192]{};
        std::snprintf(message, sizeof(message), "FireShotStart tick=%llu mouth=(%.3f,%.3f,%.3f) effect=%d",
                      static_cast<unsigned long long>(shot.processedTick), static_cast<double>(mouth.x),
                      static_cast<double>(mouth.y), static_cast<double>(mouth.z),
                      static_cast<int>(!shot.fireStartEffect.IsEmpty() && shot.fireStartScale > 0.0f));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "GourdFeedback", message);
    }
}

void StepFeedback(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot,
                  const HIKARI::FrameContext& frame) {
    auto& feedback = shot.feedback;
    const bool hasCameraMotion = feedback.cameraKickDistance > 0.0f || feedback.cameraShakeAmplitude > 0.0f ||
                                 feedback.cameraFovDegrees > 0.0f;
    if (feedback.observedDryFires != shot.dryFires) {
        feedback.observedDryFires = shot.dryFires;
        Feedback::Rumble(world, 0.0f, feedback.dryFireRumble, feedback.dryFireRumbleFrames);
    }
    if (feedback.observedShots != shot.shotsFired) {
        feedback.observedShots = shot.shotsFired;
        feedback.ageSeconds = 0.0f;
        feedback.direction = shot.muzzleDirection;
        feedback.active = feedback.cameraFrames > 0 && hasCameraMotion;
        // 火弾の発射は、カメラ・色ずれ・振動を倍率の分だけ強める。
        const bool fire = shot.lastShotFire;
        feedback.cameraScale = fire ? std::max(0.0f, feedback.fireCameraScale) : 1.0f;
        feedback.chromaScale = fire ? std::max(0.0f, feedback.fireChromaScale) : 1.0f;
        const float rumbleScale = fire ? std::max(0.0f, feedback.fireRumbleScale) : 1.0f;
        Feedback::Rumble(world, std::min(1.0f, feedback.rumbleLow * rumbleScale),
                         std::min(1.0f, feedback.rumbleHigh * rumbleScale), feedback.rumbleFrames);
        LogStart(shot);
    }
    const float duration = static_cast<float>(feedback.cameraFrames) / kFramesPerSecond;
    if (!feedback.active || duration <= 0.0f || feedback.ageSeconds >= duration || !hasCameraMotion) {
        feedback.active = false;
        return;
    }
    HIKARI::GameProjectCameraModifier modifier{};
    const bool accepted = SubmitCamera(world, object, feedback, modifier);
    // 発射の瞬間だけ、画面の色をずらして砲の圧を伝える。毎フレーム出し直す種類の効果で、時間が過ぎれば自然に消える。
    const float chromaSeconds = static_cast<float>(feedback.chromaFrames) / kFramesPerSecond;
    if (chromaSeconds > 0.0f && feedback.ageSeconds < chromaSeconds && feedback.chromaPixels > 0.0f) {
        const float fade = 1.0f - feedback.ageSeconds / chromaSeconds;
        HIKARI::GameProjectActionCinematicGradeRequest grade{};
        grade.sourceId = kChromaSource ^ object.GetHandle().value;
        grade.priority = kChromaPriority;
        grade.weight = fade;
        grade.chromaticSeparationPixels = feedback.chromaPixels * feedback.chromaScale * fade;
        grade.contrastStrength = feedback.chromaContrast * feedback.chromaScale * fade;
        world.GetPresentationVfx().SubmitActionCinematicGrade(grade);
    }
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

// ボスの斬撃・ナイフを吸い取った瞬間の手応え。視野角だけを引き、揺れと押しは出さない（吸い込みの寄りと喧嘩しない）。
void StepCatch(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdComponent& gourd,
               const HIKARI::FrameContext& frame) {
    gourd.catchStopCooldown = std::max(0.0f, gourd.catchStopCooldown - std::max(0.0f, frame.unscaledDt));
    if (gourd.catchObservedCount != gourd.catchCount) {
        gourd.catchObservedCount = gourd.catchCount;
        const uint64_t owner = object.GetHandle().value;
        // 続けて入る間（X 字の 2 本・ナイフの扇）は、止めを重ねない。ほかの手応えは 1 つごとに出す。
        uint64_t token = 0;
        if (gourd.catchStopCooldown <= 0.0f) {
            token = Feedback::RequestHitStop(world, kCatchHitStopSource ^ owner, Feedback::kCatchPriority,
                                             gourd.catchHitStopFrames, gourd.catchHitStopScale);
            gourd.catchStopCooldown = static_cast<float>(std::max(0, gourd.catchHitStopCooldownFrames)) / 60.0f;
        }
        const bool rumble =
            Feedback::Rumble(world, gourd.catchRumbleLow, gourd.catchRumbleHigh, gourd.catchRumbleFrames);
        Feedback::CameraShakeParams shake{};
        shake.sourceId = kCatchCameraSource ^ owner;
        shake.priority = Feedback::kCatchPriority;
        shake.frames = gourd.catchFovFrames;
        shake.fovDegrees = gourd.catchFovDegrees;
        Feedback::StartCameraShake(gourd.catchShake, shake);
        const bool fovAccepted = Feedback::SubmitCameraShake(world, gourd.catchShake, gourd.cameraId, frame.unscaledDt);
        // 合図のエフェクトは口の位置で一度だけ。空なら何も出さない。
        bool effectPlayed = false;
        if (!gourd.catchEffect.IsEmpty()) {
            effectPlayed = VfxPlayback::PlayOneShot(world, gourd.catchEffect, GourdRules::GetMouthPosition(gourd));
            gourd.vfxAccepted = effectPlayed && gourd.vfxAccepted;
        }
        char message[192]{};
        std::snprintf(message, sizeof(message),
                      "Catch tick=%llu count=%llu stopFrames=%d stopToken=%llu rumble=%d fovActive=%d fovAccepted=%d "
                      "effect=%d",
                      static_cast<unsigned long long>(gourd.processedTick),
                      static_cast<unsigned long long>(gourd.catchCount), gourd.catchHitStopFrames,
                      static_cast<unsigned long long>(token), static_cast<int>(rumble),
                      static_cast<int>(gourd.catchShake.active), static_cast<int>(fovAccepted),
                      static_cast<int>(effectPlayed));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "GourdFeedback", message);
        return;
    }
    Feedback::SubmitCameraShake(world, gourd.catchShake, gourd.cameraId, frame.unscaledDt);
}

} // namespace

void GourdFeedbackSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!GameSession::IsPlaying(world, frame))
        return;
    world.ForEachComponent<GourdShotComponent>(HIKARI::MakeComponentTypeId(kGourdShotType),
                                               [&](HIKARI::ObjectView object, GourdShotComponent& shot) {
                                                   StepFireStart(world, object, shot);
                                                   StepFeedback(world, object, shot, frame);
                                               });
    world.ForEachComponent<GourdComponent>(
        HIKARI::MakeComponentTypeId(kGourdType),
        [&](HIKARI::ObjectView object, GourdComponent& gourd) { StepCatch(world, object, gourd, frame); });
}

void GourdFeedbackSystem::PostRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!GameSession::IsPlaying(world, frame))
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
