#include "BossFeedback.h"

#include <algorithm>
#include <cstdio>

#include "HIKARI/Project/ComponentViews.h"
#include "BossFrame.h"
#include "BossWaves.h"
#include "../Combat/Damageable.h"
#include "../Common/ComponentLookup.h"
#include "../Player/Feedback.h"
#include "../Player/Player.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

constexpr uint64_t kBreakHitStopSource = 0x5444324A01ull;
constexpr uint64_t kBreakCameraSource = 0x5444324A02ull;
constexpr uint64_t kBurstCameraSource = 0x5444324A03ull;
constexpr uint64_t kReleaseCameraSource = 0x5444324A04ull;
// 放った瞬間の打ち込みの優先度（プレイヤー 300 のすぐ上。吸い込み・発射・吸い取り・命中・崩しより低い）。
constexpr int kReleasePriority = 301;

// 一瞬の動きのぼかしを、プレイヤーのカメラへ頼む（戦いの間のふだんのぼかしに重ねて出る）。
void KickBlur(float shutterDegrees, float maxPixels, int frames) {
    ComponentLookup::ForEachGame<Player>(
        [&](HIKARI::ObjectView, Player& player) { PlayerRules::KickBlur(player, shutterDegrees, maxPixels, frames); });
}

void RequestBodyFlash(HIKARI::ObjectView object, int frames) {
    if (Damageable* health = Damageable::Of(object.GetHandle().value))
        health->RequestFlash(frames);
}

void Trigger(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss) {
    const uint64_t owner = object.GetHandle().value;
    const uint64_t stopToken = Feedback::RequestHitStop(world, kBreakHitStopSource ^ owner, Feedback::kBreakPriority,
                                                        boss.breakHitStopFrames, boss.breakHitStopScale);
    Feedback::CameraShakeParams shake{};
    shake.sourceId = kBreakCameraSource ^ owner;
    shake.priority = Feedback::kBreakPriority;
    shake.frames = boss.breakCameraFrames;
    shake.amplitude = boss.breakCameraAmplitude;
    shake.frequency = boss.breakCameraFrequency;
    Feedback::StartCameraShake(boss.breakShake, shake);
    const bool rumble = Feedback::Rumble(world, boss.breakRumbleLow, boss.breakRumbleHigh, boss.breakRumbleFrames);
    KickBlur(boss.breakBlurShutter, boss.breakBlurPixels, boss.breakBlurFrames);
    RequestBodyFlash(object, boss.breakFlashFrames);
    // 合図の塊：当たった所の爆発と同じ枠へ入れる（BossVisuals が場の流体の塊と光を出し、BossSmoke が場の霧を押す）。
    // 本体は色を読める材質が無いと光らないので、崩した合図はこの塊で見せる。
    bool blastAdded = false;
    HIKARI::GameProjectTransform transform{};
    if (boss.breakBlastScale > 0.0f && object.GetWorldTransform(transform)) {
        BossWaves::AddBlast(boss, {.pushPending = true,
                                   .effectPending = true,
                                   .fire = boss.lastPhaseNumber >= 2,
                                   .scale = boss.breakBlastScale,
                                   .position = transform.position});
        blastAdded = true;
    }
    char message[192]{};
    std::snprintf(message, sizeof(message),
                  "BreakFeedback tick=%llu stopFrames=%d stopToken=%llu shake=%d rumble=%d flashFrames=%d blast=%d",
                  static_cast<unsigned long long>(boss.tick), boss.breakHitStopFrames,
                  static_cast<unsigned long long>(stopToken), static_cast<int>(boss.breakShake.active),
                  static_cast<int>(rumble), boss.breakFlashFrames, static_cast<int>(blastAdded));
    BossLog::Write(message);
}

} // namespace

void BossFeedback::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                          const HIKARI::FrameContext& frame) {
    if (boss.breakFeedbackPending) {
        boss.breakFeedbackPending = false;
        Trigger(world, object, boss);
    }
    if (boss.burstFeedbackPending) {
        // 範囲爆発の叩きつけ（c-3c908c）：爆発の瞬間にカメラを揺らし、パッドを振るわせる。時間は止めない。
        boss.burstFeedbackPending = false;
        Feedback::CameraShakeParams shake{};
        shake.sourceId = kBurstCameraSource ^ object.GetHandle().value;
        shake.priority = Feedback::kBreakPriority;
        shake.frames = boss.burstCameraFrames;
        const float scale = std::clamp(boss.burstFeedbackScale, 0.0f, 1.0f);
        shake.amplitude = boss.burstCameraAmplitude * scale;
        shake.frequency = boss.burstCameraFrequency;
        Feedback::StartCameraShake(boss.burstShake, shake);
        Feedback::Rumble(world, boss.burstRumbleLow * scale, boss.burstRumbleHigh * scale, boss.burstRumbleFrames);
        KickBlur(boss.burstBlurShutter * scale, boss.burstBlurPixels * scale, boss.burstBlurFrames);
    }
    Feedback::SubmitCameraShake(world, boss.burstShake, {}, frame.unscaledDt);
    if (boss.releaseFeedbackPending > 0.0f) {
        // 斬撃・霧の弾を放った瞬間（c-e0a9c3 の続き）：カメラを小さく打ち込む。時間は止めない。
        const float scale = std::clamp(boss.releaseFeedbackPending, 0.0f, 1.0f);
        boss.releaseFeedbackPending = 0.0f;
        Feedback::CameraShakeParams punch{};
        punch.sourceId = kReleaseCameraSource ^ object.GetHandle().value;
        punch.priority = kReleasePriority;
        punch.frames = boss.releaseCameraFrames;
        punch.amplitude = boss.releaseCameraAmplitude * scale;
        punch.frequency = boss.releaseCameraFrequency;
        punch.fovDegrees = boss.releaseCameraFov * scale;
        Feedback::StartCameraShake(boss.releaseShake, punch);
        Feedback::Rumble(world, boss.releaseRumble * scale, boss.releaseRumble * scale, boss.releaseRumbleFrames);
    }
    Feedback::SubmitCameraShake(world, boss.releaseShake, {}, frame.unscaledDt);
    HIKARI::GameProjectFloat3 offset{};
    const bool wasActive = boss.breakShake.active;
    const bool accepted = Feedback::SubmitCameraShake(world, boss.breakShake, {}, frame.unscaledDt, &offset);
    if (wasActive) {
        char message[128]{};
        std::snprintf(message, sizeof(message), "BreakShake tick=%llu offset=(%.6f,%.6f) accepted=%d",
                      static_cast<unsigned long long>(boss.tick), static_cast<double>(offset.x),
                      static_cast<double>(offset.y), static_cast<int>(accepted));
        BossLog::Write(message);
    }
}

} // namespace Project_TD2_2
