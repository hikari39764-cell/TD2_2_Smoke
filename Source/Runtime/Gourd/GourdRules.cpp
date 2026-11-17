#include "GourdRules.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ProjectLog.h"

namespace Project_TD2_2 {
namespace {

// 火弾の始まり・延長・終わりを記録する（見た目のカード c-5d738d と確かめが読む）。cause は fire（場の火）・blade（火の刃）・
// time（時間切れ）。before は変わる前の残り秒。
void LogFireShot(const GourdComponent& gourd, const char* event, const char* cause, float before) {
    char message[224]{};
    std::snprintf(message, sizeof(message),
                  "FireShot tick=%llu event=%s cause=%s remaining=%.3f before=%.3f duration=%.3f areaPending=%.3f",
                  static_cast<unsigned long long>(gourd.processedTick), event, cause,
                  static_cast<double>(gourd.fireShotSeconds), static_cast<double>(before),
                  static_cast<double>(GourdRules::GetFireShotDuration(gourd)),
                  static_cast<double>(gourd.fireAreaPending));
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "Gourd", message);
}

// 残りを fireShotDuration へ戻す（短い残りを延ばすだけ。それより長くは積まない）。
void RefreshFireShot(GourdComponent& gourd, const char* cause) {
    const float duration = GourdRules::GetFireShotDuration(gourd);
    if (!(duration > 0.0f))
        return;
    const float before = gourd.fireShotSeconds;
    gourd.fireShotSeconds = std::max(before, duration);
    LogFireShot(gourd, before > 0.0f ? "extend" : "start", cause, before);
}

} // namespace

void GourdRules::Initialize(GourdComponent& gourd) {
    if (gourd.initialized)
        return;
    gourd.storedSmoke = std::clamp(gourd.initialSmoke, 0.0f, GetCapacity(gourd));
    gourd.initialized = true;
}

float GourdRules::GetSmoke(const GourdComponent& gourd) {
    return std::clamp(gourd.initialized ? gourd.storedSmoke : gourd.initialSmoke, 0.0f, GetCapacity(gourd));
}

float GourdRules::GetCapacity(const GourdComponent& gourd) { return std::max(0.0f, gourd.capacity); }
float GourdRules::GetSmokePerVolume(const GourdComponent& gourd) { return std::max(0.0f, gourd.smokePerVolume); }

bool GourdRules::IsFull(const GourdComponent& gourd) {
    return GetCapacity(gourd) > 0.0f && GetSmoke(gourd) >= GetCapacity(gourd);
}

bool GourdRules::HasRoomToAbsorb(const GourdComponent& gourd) {
    // 空きがこれより少なければ満杯と見る（煙の足し引きの丸めで、ほぼ満杯のまま吸い続けないように）。
    constexpr float kAbsorbRoomEpsilon = 0.01f;
    return GetCapacity(gourd) - GetSmoke(gourd) > kAbsorbRoomEpsilon;
}

bool GourdRules::TryConsumeSmoke(GourdComponent& gourd, float amount) {
    Initialize(gourd);
    if (!std::isfinite(amount) || amount < 0.0f || GetSmoke(gourd) < amount)
        return false;
    gourd.storedSmoke = GetSmoke(gourd) - amount;
    return true;
}

float GourdRules::AddSmoke(GourdComponent& gourd, float amount) {
    Initialize(gourd);
    if (!std::isfinite(amount) || amount <= 0.0f)
        return 0.0f;
    const float before = GetSmoke(gourd);
    gourd.storedSmoke = std::min(GetCapacity(gourd), before + amount);
    return gourd.storedSmoke - before;
}

void GourdRules::NotifyCatch(GourdComponent& gourd, int count) {
    if (count > 0)
        gourd.catchCount += static_cast<uint64_t>(count);
}

void GourdRules::NotifySwallowing(GourdComponent& gourd) {
    // StepSwallow が次の tick の始めに 1 減らすので、1 足しておく（呼ばれている間は 0 にならない）。
    gourd.swallowFrames = std::max(0, gourd.swallowTailFrames) + 1;
}

void GourdRules::StepSwallow(GourdComponent& gourd) { gourd.swallowFrames = std::max(0, gourd.swallowFrames - 1); }

bool GourdRules::IsSwallowing(const GourdComponent& gourd) { return gourd.swallowFrames > 0; }

void GourdRules::AddAbsorbedFireArea(GourdComponent& gourd, float area) {
    if (!std::isfinite(area) || area <= 0.0f)
        return;
    const float trigger = std::max(0.0f, gourd.fireShotTriggerArea);
    gourd.fireAreaPending += area;
    if (gourd.fireAreaPending < trigger)
        return;
    // 1 tick に何度届いても、戻すのは 1 回（残りは長さより多く積まない）。届かなかった端数は次へ持ち越す。
    gourd.fireAreaPending = trigger > 0.0f ? std::fmod(gourd.fireAreaPending, trigger) : 0.0f;
    RefreshFireShot(gourd, "fire");
}

void GourdRules::NotifyFireCatch(GourdComponent& gourd, int count) {
    if (count > 0)
        RefreshFireShot(gourd, "blade");
}

void GourdRules::StepFireShot(GourdComponent& gourd, float dt) {
    if (!(gourd.fireShotSeconds > 0.0f) || !(dt > 0.0f))
        return;
    const float before = gourd.fireShotSeconds;
    gourd.fireShotSeconds = std::max(0.0f, before - dt);
    if (gourd.fireShotSeconds <= 0.0f)
        LogFireShot(gourd, "end", "time", before);
}

bool GourdRules::IsFireShot(const GourdComponent& gourd) { return gourd.fireShotSeconds > 0.0f; }
float GourdRules::GetFireShotSeconds(const GourdComponent& gourd) { return std::max(0.0f, gourd.fireShotSeconds); }
float GourdRules::GetFireShotDuration(const GourdComponent& gourd) { return std::max(0.0f, gourd.fireShotDuration); }

bool GourdRules::IsAbsorbing(const GourdComponent& gourd) { return gourd.absorbing; }
bool GourdRules::IsAbsorbCommitted(const GourdComponent& gourd) {
    return gourd.absorbPhase == GourdAbsorbPhase::Windup || gourd.absorbPhase == GourdAbsorbPhase::Absorbing;
}

bool GourdRules::WantsAbsorb(const GourdComponent& gourd) { return gourd.absorbHeld || gourd.absorbBuffer > 0; }

void GourdRules::CancelAbsorbRecovery(GourdComponent& gourd) {
    if (gourd.absorbPhase != GourdAbsorbPhase::Recovery)
        return;
    gourd.absorbPhase = GourdAbsorbPhase::Idle;
    gourd.absorbPhaseFrames = 0;
}

HIKARI::GameProjectFloat3 GourdRules::GetMouthPosition(const GourdComponent& gourd) { return gourd.mouthPosition; }
HIKARI::GameProjectFloat3 GourdRules::GetAimDirection(const GourdComponent& gourd) { return gourd.aimDirection; }

void GourdRules::AddRecoil(GourdComponent& gourd, HIKARI::GameProjectFloat3 direction, float distance, int frames) {
    if (frames <= 0 || !std::isfinite(distance) || distance < 0.0f || !std::isfinite(direction.x) ||
        !std::isfinite(direction.y) || !std::isfinite(direction.z))
        return;
    const float length = std::hypot(direction.x, direction.y, direction.z);
    if (!std::isfinite(length) || length <= 0.0f)
        return;
    const float count = static_cast<float>(frames);
    // 再射撃は現在位置から新しい最大変位へ進め、連射で反動を無限に積み重ねない。
    gourd.recoilPerFrame = {(direction.x / length * distance - gourd.recoilOffset.x) / count,
                            (direction.y / length * distance - gourd.recoilOffset.y) / count,
                            (direction.z / length * distance - gourd.recoilOffset.z) / count};
    gourd.recoilVelocity = {};
    gourd.recoilFramesLeft = frames;
}

void GourdRules::SetAimOverride(GourdComponent& gourd, HIKARI::GameProjectFloat3 direction) {
    if (!std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z))
        return;
    const float length = std::hypot(direction.x, direction.y, direction.z);
    if (!std::isfinite(length) || length <= 0.0f)
        return;
    gourd.aimOverrideDirection = {direction.x / length, direction.y / length, direction.z / length};
    gourd.aimOverridePending = true;
}

void GourdRules::ReceiveRemovedSmoke(GourdComponent& gourd, double removedTotal) {
    Initialize(gourd);
    gourd.removedDelta = 0.0;
    if (!std::isfinite(removedTotal) || removedTotal < 0.0)
        return;
    gourd.removedDelta = removedTotal >= gourd.lastRemovedTotal ? removedTotal - gourd.lastRemovedTotal : removedTotal;
    gourd.lastRemovedTotal = removedTotal;
    const double income = gourd.removedDelta * static_cast<double>(std::max(0.0f, gourd.smokePerVolume));
    gourd.storedSmoke = static_cast<float>(
        std::clamp(static_cast<double>(GetSmoke(gourd)) + income, 0.0, static_cast<double>(GetCapacity(gourd))));
}

} // namespace Project_TD2_2
