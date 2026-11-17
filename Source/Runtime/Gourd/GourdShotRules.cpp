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

float GourdShotRules::GetProjectileDamage(const GourdShotComponent& shot, const GourdProjectile& projectile) {
    return GetDamageAtDistance(shot, projectile.distance) *
           (projectile.fire ? std::max(0.0f, shot.fireShotDamageScale) : 1.0f);
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

bool GourdShotRules::IsWindingUp(const GourdShotComponent& shot) { return shot.phase == GourdShotPhase::Windup; }

bool GourdShotRules::IsChaining(const GourdShotComponent& shot) {
    // 後隙を延ばして連射を待つのと同じ見方（GourdShotSystem の EndRecovery）。
    return shot.phase == GourdShotPhase::Recovery && (shot.attackHeld || shot.attackBuffer > 0) &&
           shot.processedTick <= shot.nextFireTick;
}

uint64_t GourdShotRules::GetShotsFired(const GourdShotComponent& shot) { return shot.shotsFired; }

HIKARI::GameProjectFloat3 GourdShotRules::GetAimOrigin(const GourdShotComponent& shot) { return shot.aimOrigin; }

HIKARI::GameProjectFloat3 GourdShotRules::GetAimDirection(const GourdShotComponent& shot) { return shot.aimDirection; }

} // namespace Project_TD2_2
