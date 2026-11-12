#include "GourdShotRules.h"

#include <algorithm>
#include <cmath>

namespace Project_TD2_2 {

float GourdShotRules::GetFalloffFraction(const GourdShotComponent& shot, float distance) {
    const float start = std::min(shot.falloffStart, shot.range);
    if (distance <= start)
        return 0.0f;
    return shot.range > start ? std::clamp((distance - start) / (shot.range - start), 0.0f, 1.0f) : 1.0f;
}

float GourdShotRules::GetDamageAtDistance(const GourdShotComponent& shot, float distance) {
    return shot.damage * (1.0f - (1.0f - shot.finalDamageRatio) * GetFalloffFraction(shot, distance));
}

float GourdShotRules::GetSpeedAtDistance(const GourdShotComponent& shot, float distance) {
    const float ratio = std::clamp(distance / std::max(shot.range, 0.1f), 0.0f, 1.0f);
    return shot.initialSpeed * (1.0f - (1.0f - shot.finalSpeedRatio) * ratio);
}

float GourdShotRules::GetRadiusAtDistance(const GourdShotComponent& shot, float distance) {
    return shot.projectileRadius * (1.0f + (shot.finalRadiusScale - 1.0f) * GetFalloffFraction(shot, distance));
}

float GourdShotRules::GetDensityAtDistance(const GourdShotComponent& shot, float distance) {
    return 1.0f - (1.0f - shot.finalDensityRatio) * GetFalloffFraction(shot, distance);
}

int GourdShotRules::GetRepeatFrames(const GourdShotComponent& shot, int chainShots) {
    const float speedUp =
        std::min(std::max(shot.rapidMax, 1.0f),
                 1.0f + std::max(shot.rapidAccel, 0.0f) * static_cast<float>(std::max(chainShots - 1, 0)));
    const int frames = static_cast<int>(std::lround(static_cast<float>(shot.repeatFrames) / speedUp));
    return std::max({1, shot.rapidMinFrames, frames});
}

bool GourdShotRules::IsFiring(const GourdShotComponent& shot) { return shot.phase != GourdShotPhase::Idle; }

uint32_t GourdShotRules::GetActiveProjectileCount(const GourdShotComponent& shot) {
    return static_cast<uint32_t>(std::count_if(shot.projectiles.begin(), shot.projectiles.end(),
                                               [](const GourdProjectile& projectile) { return projectile.active; }));
}

} // namespace Project_TD2_2
