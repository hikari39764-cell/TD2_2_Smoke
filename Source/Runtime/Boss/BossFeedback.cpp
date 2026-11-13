#include "BossFeedback.h"

#include <cstdio>

#include "HIKARI/Project/ComponentViews.h"
#include "BossFrame.h"
#include "BossWaves.h"
#include "../Combat/DamageableComponent.h"
#include "../Combat/DamageableRules.h"
#include "../Combat/Feedback.h"

namespace Project_TD2_2 {
namespace {

constexpr uint64_t kBreakHitStopSource = 0x5444324A01ull;
constexpr uint64_t kBreakCameraSource = 0x5444324A02ull;

void RequestBodyFlash(HIKARI::WorldView& world, HIKARI::ObjectView object, int frames) {
    world.ForEachComponent<DamageableComponent>(
        HIKARI::MakeComponentTypeId(kDamageableType), [&](HIKARI::ObjectView healthObject, DamageableComponent& health) {
            if (healthObject.GetHandle().value == object.GetHandle().value)
                DamageableRules::RequestFlash(health, frames);
        });
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
    RequestBodyFlash(world, object, boss.breakFlashFrames);
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
    HIKARI::GameProjectFloat3 offset{};
    const bool wasActive = boss.breakShake.active;
    const bool accepted = Feedback::SubmitCameraShake(world, boss.breakShake, 0, frame.unscaledDt, &offset);
    if (wasActive) {
        char message[128]{};
        std::snprintf(message, sizeof(message), "BreakShake tick=%llu offset=(%.6f,%.6f) accepted=%d",
                      static_cast<unsigned long long>(boss.tick), static_cast<double>(offset.x),
                      static_cast<double>(offset.y), static_cast<int>(accepted));
        BossLog::Write(message);
    }
}

} // namespace Project_TD2_2
