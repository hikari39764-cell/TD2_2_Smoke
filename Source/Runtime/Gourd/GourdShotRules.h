#pragma once

#include "GourdShotComponent.h"

namespace Project_TD2_2 {

class GourdShotRules final {
public:
    static float GetFalloffFraction(const GourdShotComponent& shot, float distance);
    static float GetDamageAtDistance(const GourdShotComponent& shot, float distance);
    static float GetSpeedAtDistance(const GourdShotComponent& shot, float distance);
    static float GetRadiusAtDistance(const GourdShotComponent& shot, float distance);
    static float GetDensityAtDistance(const GourdShotComponent& shot, float distance);
    // chainShots 発目を撃った後、次を撃つまでの F。押し続けるほど短くなる。
    static int GetRepeatFrames(const GourdShotComponent& shot, int chainShots);
    static bool IsFiring(const GourdShotComponent& shot);
    static uint32_t GetActiveProjectileCount(const GourdShotComponent& shot);
};

} // namespace Project_TD2_2
