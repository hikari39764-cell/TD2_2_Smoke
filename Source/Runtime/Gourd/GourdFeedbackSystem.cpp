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
#include "../Player/Feedback.h"
#include "../Player/FeedbackCamera.h"
#include "../Player/Player.h"
#include "../Player/PlayerRules.h"
#include "../Common/VectorMath.h"
#include "../Common/VfxParameters.h"
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
constexpr uint64_t kBiteHitStopSource = 0x5444324705ull;
constexpr uint64_t kBiteCameraSource = 0x5444324706ull;
// 吸い取りのエフェクトの色の入力。
constexpr const char* kTintInput = "input.color";
// 色ずれ（動作の色調）の出どころと優先度。命中（320）が発射より勝つ。
constexpr uint64_t kChromaSource = 0x5444324702ull;
constexpr int kChromaPriority = 310;

void LogStart(const GourdShot& shot) {
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
    HIKARI::Entity camera{};
    ForEachOnObject<Gourd>(world, kGourdType, object, [&](HIKARI::ObjectView, Gourd& gourd) { camera = gourd.camera; });
    modifier.cameraObjectHandle = FeedbackCamera::Resolve(world, camera);
    if (modifier.cameraObjectHandle == 0)
        return false;
    return world.GetCamera().SubmitModifier(modifier);
}

// 火弾の状態が始まった瞬間に、ヒョウタンの口から短く炎を噴く合図を 1 回だけ出す（残りを延ばしただけの時は出さない）。
// 合図は砲撃の一度きりのエフェクトの枠（砲口の枠）で出すので、流体域の数は増えない。
void StepFireStart(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShot& shot) {
    bool fire = false;
    HIKARI::GameProjectFloat3 mouth{}, aim{0.0f, 0.0f, 1.0f};
    ForEachOnObject<Gourd>(world, kGourdType, object, [&](HIKARI::ObjectView, Gourd& gourd) {
        fire = GourdRules::IsFireShot(gourd);
        // 合図は見えている口から（描画のヒョウタン）。
        mouth = gourd.drawMouthPosition;
        aim = gourd.drawAimDirection;
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
                      static_cast<int>(shot.fireStartEffect.IsSet() && shot.fireStartScale > 0.0f));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "GourdFeedback", message);
    }
}

void StepFeedback(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShot& shot,
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

// 項目の色に明るさを掛けて、エフェクトの入力の色へ。
HIKARI::GameProjectFloat3 Tint(const HIKARI::Color& color, float glow) {
    return {color.r * glow, color.g * glow, color.b * glow};
}

// 一度きりのエフェクトを、変換と色の入力（input.color）を付けて出す。受け付けられたら true。
bool PlayTinted(HIKARI::WorldView& world, const HIKARI::VfxAsset& asset, const HIKARI::GameProjectTransform& transform,
                HIKARI::GameProjectFloat3 color) {
    HIKARI::GameProjectVfxPlayRequest play{};
    play.asset = VfxPlayback::Reference(asset);
    play.transform = transform;
    play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
    VfxParameters::SetColor(play.parameters, kTintInput, color);
    return world.GetVfx().Play(play).IsValid();
}

// 噛む（c-e0a9c3）：ボスの斬撃・ナイフが吸い込みに捕まった瞬間。短い止めと鋭い振動を 1 つ、口から捕まえた所へ走る
// 火花の線。続けて捕まった分（X 字の 2 本・ナイフの扇）は、止めを重ねない。
void StepBite(HIKARI::WorldView& world, HIKARI::ObjectView object, Gourd& gourd, const HIKARI::FrameContext& frame) {
    gourd.biteStopCooldown = std::max(0.0f, gourd.biteStopCooldown - std::max(0.0f, frame.unscaledDt));
    if (gourd.biteObserved == gourd.biteCount) {
        Feedback::SubmitCameraShake(world, gourd.biteShake, gourd.camera, frame.unscaledDt);
        return;
    }
    gourd.biteObserved = gourd.biteCount;
    uint64_t token = 0;
    if (gourd.biteStopCooldown <= 0.0f) {
        token = Feedback::RequestHitStop(world, kBiteHitStopSource ^ object.GetHandle().value, Feedback::kCatchPriority,
                                         gourd.biteHitStopFrames, gourd.biteHitStopScale);
        gourd.biteStopCooldown = static_cast<float>(std::max(0, gourd.catchHitStopCooldownFrames)) / 60.0f;
    }
    const bool rumble = Feedback::Rumble(world, gourd.biteRumbleLow, gourd.biteRumbleHigh, gourd.biteRumbleFrames);
    // カメラの打ち込み：一瞬だけ寄って、小さく揺れる。
    Feedback::CameraShakeParams punch{};
    punch.sourceId = kBiteCameraSource ^ object.GetHandle().value;
    punch.priority = Feedback::kCatchPriority;
    punch.frames = gourd.biteCameraFrames;
    punch.fovDegrees = gourd.biteFovDegrees;
    punch.amplitude = gourd.biteShakeAmplitude;
    Feedback::StartCameraShake(gourd.biteShake, punch);
    Feedback::SubmitCameraShake(world, gourd.biteShake, gourd.camera, frame.unscaledDt);
    // 線：口を原点に、+X を捕まえた所へ向け、根の拡大 X を距離にする（資産の粒の生まれる箱が、口から捕まえた所まで伸びる）。
    bool effectPlayed = false;
    const HIKARI::GameProjectFloat3 mouth = gourd.drawMouthPosition;
    const HIKARI::GameProjectFloat3 to{gourd.bitePosition.x - mouth.x, gourd.bitePosition.y - mouth.y,
                                       gourd.bitePosition.z - mouth.z};
    const float distance = std::sqrt(to.x * to.x + to.y * to.y + to.z * to.z);
    if (gourd.biteEffect.IsSet() && distance > 0.05f) {
        HIKARI::GameProjectTransform transform{};
        transform.position = mouth;
        transform.rotation = VectorMath::RotationFromXAxis({to.x / distance, to.y / distance, to.z / distance});
        transform.scale = {distance, 1.0f, 1.0f};
        effectPlayed =
            PlayTinted(world, gourd.biteEffect, transform,
                       Tint(gourd.biteFire ? gourd.catchFireColor : gourd.catchMistColor, gourd.catchEffectGlow));
        gourd.vfxAccepted = effectPlayed && gourd.vfxAccepted;
    }
    char message[192]{};
    std::snprintf(
        message, sizeof(message), "Bite tick=%llu count=%llu stopToken=%llu rumble=%d effect=%d fire=%d distance=%.2f",
        static_cast<unsigned long long>(gourd.processedTick), static_cast<unsigned long long>(gourd.biteCount),
        static_cast<unsigned long long>(token), static_cast<int>(rumble), static_cast<int>(effectPlayed),
        static_cast<int>(gourd.biteFire), static_cast<double>(distance));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "GourdFeedback", message);
}

// 呑み込む：ボスの斬撃・ナイフが口へ入り切った瞬間の手応え。止め・重い振動・カメラの押しと揺れと視野角・口のエフェクト
// （ヒョウタンの膨らみは GourdSystem）。
void StepCatch(HIKARI::WorldView& world, HIKARI::ObjectView object, Gourd& gourd, const HIKARI::FrameContext& frame) {
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
        // 呑み込んだ反動：口の向きと逆へ押されて戻り、小さく揺れる。
        shake.amplitude = gourd.catchShakeAmplitude;
        shake.kickDistance = gourd.catchKickDistance;
        shake.kickDirection = {-gourd.drawAimDirection.x, -gourd.drawAimDirection.y, -gourd.drawAimDirection.z};
        Feedback::StartCameraShake(gourd.catchShake, shake);
        const bool fovAccepted = Feedback::SubmitCameraShake(world, gourd.catchShake, gourd.camera, frame.unscaledDt);
        // 動きのぼかしの一瞬：押しと揺れが、画の流れになって見える。
        // （ぼかしは、プレイヤーのカメラが、戦いの間のふだんの分に重ねて出す。）
        ComponentLookup::ForEachGame<Player>([&](HIKARI::ObjectView, Player& player) {
            PlayerRules::KickBlur(player, gourd.catchBlurShutter, gourd.catchBlurPixels, gourd.catchBlurFrames);
        });
        // 合図のエフェクトは口の位置で一度だけ。空なら何も出さない。
        bool effectPlayed = false;
        if (gourd.catchEffect.IsSet()) {
            HIKARI::GameProjectTransform transform{};
            transform.position = gourd.drawMouthPosition;
            const float size = gourd.catchSmall ? gourd.catchSmallScale : 1.0f;
            transform.scale = {size, size, size};
            effectPlayed =
                PlayTinted(world, gourd.catchEffect, transform,
                           Tint(gourd.catchFire ? gourd.catchFireColor : gourd.catchMistColor, gourd.catchEffectGlow));
            gourd.vfxAccepted = effectPlayed && gourd.vfxAccepted;
        }
        char message[192]{};
        std::snprintf(message, sizeof(message),
                      "Catch tick=%llu count=%llu stopFrames=%d stopToken=%llu rumble=%d fovActive=%d fovAccepted=%d "
                      "effect=%d fire=%d",
                      static_cast<unsigned long long>(gourd.processedTick),
                      static_cast<unsigned long long>(gourd.catchCount), gourd.catchHitStopFrames,
                      static_cast<unsigned long long>(token), static_cast<int>(rumble),
                      static_cast<int>(gourd.catchShake.active), static_cast<int>(fovAccepted),
                      static_cast<int>(effectPlayed), static_cast<int>(gourd.catchFire));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "GourdFeedback", message);
        return;
    }
    Feedback::SubmitCameraShake(world, gourd.catchShake, gourd.camera, frame.unscaledDt);
}

} // namespace

void GourdFeedbackSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!GameSession::IsPlaying(world, frame))
        return;
    ComponentLookup::ForEachGame<GourdShot>([&](HIKARI::ObjectView object, GourdShot& shot) {
        StepFireStart(world, object, shot);
        StepFeedback(world, object, shot, frame);
    });
    ComponentLookup::ForEachGame<Gourd>([&](HIKARI::ObjectView object, Gourd& gourd) {
        StepBite(world, object, gourd, frame);
        StepCatch(world, object, gourd, frame);
    });
}

void GourdFeedbackSystem::PostRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!GameSession::IsPlaying(world, frame))
        return;
    ComponentLookup::ForEachGame<GourdShot>(
        [&](HIKARI::ObjectView, GourdShot& shot) {
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
