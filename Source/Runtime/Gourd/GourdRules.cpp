#include "GourdRules.h"

#include <algorithm>
#include <cmath>

namespace Project_TD2_2 {

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
