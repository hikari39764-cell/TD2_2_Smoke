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

bool GourdRules::TryConsumeSmoke(GourdComponent& gourd, float amount) {
    Initialize(gourd);
    if (!std::isfinite(amount) || amount < 0.0f || GetSmoke(gourd) < amount)
        return false;
    gourd.storedSmoke = GetSmoke(gourd) - amount;
    return true;
}

bool GourdRules::IsAbsorbing(const GourdComponent& gourd) { return gourd.absorbing; }
HIKARI::GameProjectFloat3 GourdRules::GetMouthPosition(const GourdComponent& gourd) { return gourd.mouthPosition; }
HIKARI::GameProjectFloat3 GourdRules::GetAimDirection(const GourdComponent& gourd) { return gourd.aimDirection; }

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
