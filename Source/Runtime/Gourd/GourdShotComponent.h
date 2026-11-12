#pragma once

#include <array>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kGourdShotType = "Project_TD2_2.GourdShot";
inline constexpr std::string_view kGourdShotSystem = "Project_TD2_2.GourdShotSystem";
inline constexpr size_t kGourdProjectileCapacity = 8;
inline constexpr size_t kGourdResidueCapacity = 8;

enum class GourdShotPhase { Idle, Windup, Recovery };

struct GourdProjectile {
    bool active = false;
    bool finishTrailPending = false;
    uint64_t id = 0;
    uint64_t spawnedTick = 0;
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectFloat3 direction{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectObjectHandle ignoredPlayer{};
    float distance = 0.0f;
    float speed = 0.0f;
    float radius = 0.0f;
    float densityScale = 1.0f;
    HIKARI::GameProjectVfxPlaybackHandle trail{};
};

struct GourdShotResidue {
    bool active = false;
    bool burstPending = false;
    int framesLeft = 0;
    HIKARI::GameProjectFloat3 position{};
};

class GourdShotComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    bool enabled = true;
    int windupFrames = 12;
    int repeatFrames = 24;
    int recoveryFrames = 36;
    int inputBufferFrames = 12;
    float staminaCost = 20.0f;
    float smokeCost = 20.0f;
    float recoilDistance = 2.0f;
    int recoilFrames = 8;
    float gourdRecoilDistance = 0.3f;
    int gourdRecoilFrames = 8;
    float damage = 80.0f;
    float initialSpeed = 60.0f;
    float range = 15.0f;
    float falloffStart = 10.0f;
    float finalDamageRatio = 0.2f;
    float finalSpeedRatio = 0.25f;
    float projectileRadius = 0.18f;
    float finalRadiusScale = 3.0f;
    float finalDensityRatio = 0.15f;
    int pelletsPerShot = 1;
    float spreadDegrees = 0.0f;
    float muzzleRadius = 0.65f;
    float muzzleStrength = 4.0f;
    float muzzleSmokeRate = 40.0f;
    float impactRadius = 1.0f;
    float impactStrength = 6.0f;
    float residueRadius = 0.75f;
    float residueSmokeRate = 18.0f;
    int residueFrames = 180;
    float fallbackTrailRate = 24.0f;
    HIKARI::GameProjectVfxAssetReference trailEffect{};
    int traceEveryFrames = 0;

    GourdShotPhase phase = GourdShotPhase::Idle;
    bool attackHeld = false;
    int attackBuffer = 0;
    uint64_t processedTick = 0;
    uint64_t beginTick = 0;
    uint64_t nextFireTick = 0;
    uint64_t recoveryEndTick = 0;
    uint64_t lastShotTick = 0;
    uint64_t shotsFired = 0;
    uint64_t nextProjectileId = 1;
    int blockedReason = 0;
    bool muzzlePending = false;
    HIKARI::GameProjectFloat3 muzzlePosition{};
    HIKARI::GameProjectFloat3 muzzleDirection{0.0f, 0.0f, 1.0f};
    bool vfxAccepted = true;
    bool fluidAccepted = true;
    uint32_t fluidSubmissions = 0;
    uint32_t droppedVisuals = 0;
    uint32_t residueCursor = 0;
    std::array<GourdProjectile, kGourdProjectileCapacity> projectiles{};
    std::array<GourdShotResidue, kGourdResidueCapacity> residues{};
};

} // namespace Project_TD2_2
