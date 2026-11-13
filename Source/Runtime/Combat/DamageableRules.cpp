#include "DamageableRules.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ProjectLog.h"

namespace Project_TD2_2 {

void DamageableRules::Initialize(DamageableComponent& target) {
    if (target.initialized)
        return;
    target.currentHealth = GetMaxHealth(target);
    target.initialized = true;
}

bool DamageableRules::ApplyDamage(DamageableComponent& target, float amount, HIKARI::GameProjectFloat3 direction) {
    Initialize(target);
    if (target.invulnerable || !IsAlive(target) || !std::isfinite(amount) || amount <= 0.0f ||
        !std::isfinite(direction.x) || !std::isfinite(direction.y) || !std::isfinite(direction.z)) {
        return false;
    }
    const float previousHealth = GetHealth(target);
    const float scale = std::isfinite(target.incomingDamageScale) ? std::max(0.0f, target.incomingDamageScale) : 1.0f;
    const float scaled = amount * scale;
    target.currentHealth = std::max(0.0f, previousHealth - scaled);
    target.lastDamage = scaled;
    target.lastRawDamage = amount;
    target.baseDamageTotal += static_cast<double>(amount);
    const double length = std::hypot(static_cast<double>(direction.x), static_cast<double>(direction.y),
                                     static_cast<double>(direction.z));
    target.lastDirection = length > 0.0 ? HIKARI::GameProjectFloat3{static_cast<float>(direction.x / length),
                                                                    static_cast<float>(direction.y / length),
                                                                    static_cast<float>(direction.z / length)}
                                        : HIKARI::GameProjectFloat3{};
    ++target.hitCount;
    if (target.traceEveryHit) {
        char message[384]{};
        std::snprintf(message, sizeof(message),
                      "Hit amount=%.6f applied=%.6f health=%.6f maxHealth=%.6f count=%llu direction=(%.6f,%.6f,%.6f)",
                      static_cast<double>(amount), static_cast<double>(previousHealth - target.currentHealth),
                      static_cast<double>(target.currentHealth), static_cast<double>(GetMaxHealth(target)),
                      static_cast<unsigned long long>(target.hitCount), static_cast<double>(target.lastDirection.x),
                      static_cast<double>(target.lastDirection.y), static_cast<double>(target.lastDirection.z));
        HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "Damageable", message);
    }
    return true;
}

bool DamageableRules::IsAlive(const DamageableComponent& target) { return GetHealth(target) > 0.0f; }

float DamageableRules::GetHealth(const DamageableComponent& target) {
    return target.initialized ? std::clamp(target.currentHealth, 0.0f, GetMaxHealth(target)) : GetMaxHealth(target);
}

float DamageableRules::GetMaxHealth(const DamageableComponent& target) {
    return std::isfinite(target.maxHealth) ? std::max(0.0f, target.maxHealth) : 0.0f;
}

float DamageableRules::GetLastDamage(const DamageableComponent& target) { return target.lastDamage; }

HIKARI::GameProjectFloat3 DamageableRules::GetLastDirection(const DamageableComponent& target) {
    return target.lastDirection;
}

uint64_t DamageableRules::GetHitCount(const DamageableComponent& target) { return target.hitCount; }

void DamageableRules::RequestFlash(DamageableComponent& target, int frames) {
    target.pendingFlashFrames = std::max(target.pendingFlashFrames, frames);
}

void DamageableRules::SetIncomingDamageScale(DamageableComponent& target, float scale) {
    target.incomingDamageScale = std::isfinite(scale) ? std::max(0.0f, scale) : 1.0f;
}

double DamageableRules::GetBaseDamageTotal(const DamageableComponent& target) { return target.baseDamageTotal; }

} // namespace Project_TD2_2
