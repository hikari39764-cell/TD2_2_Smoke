#pragma once

#include <array>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"
#include "GourdFeedback.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kGourdShotType = "Project_TD2_2.GourdShot";
inline constexpr std::string_view kGourdShotSystem = "Project_TD2_2.GourdShotSystem";
inline constexpr size_t kGourdProjectileCapacity = 8;
inline constexpr size_t kGourdResidueCapacity = 8;
inline constexpr size_t kGourdBurstCapacity = 8;

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
    // 炎の域。当たった後も閃きと冷却の数 F だけ残し、枠を再利用する時は古い域を先に止める。
    HIKARI::GameProjectVfxPlaybackHandle shell{};
    HIKARI::GameProjectVfxPlaybackHandle staleShell{};
    int flashFramesLeft = 0;
    int fadeFramesLeft = 0;
};

struct GourdShotResidue {
    bool active = false;
    bool burstPending = false;
    float burstScale = 0.0f;
    int framesLeft = 0;
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectVfxPlaybackHandle smoke{};
};

// 一度だけ鳴らすエフェクトの予約。規則（固定 tick）が積み、表現（描画フレーム）が再生して消す。
struct GourdShotBurst {
    bool pending = false;
    bool muzzle = false;
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectFloat3 direction{0.0f, 1.0f, 0.0f};
    float scale = 1.0f;
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
    float gourdRecoilDistance = 0.48f;
    int gourdRecoilFrames = 8;
    float gourdWindupPull = 0.14f;
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
    float muzzleStrength = 7.0f;
    float muzzleSmokeRate = 0.0f;
    float impactRadius = 2.4f;
    float impactStrength = 16.0f;
    float impactGroundReach = 5.0f;
    float impactGroundOffset = 0.3f;
    float residueRadius = 0.75f;
    float residueSmokeRate = 0.0f;
    int residueFrames = 24;
    HIKARI::GameProjectFloat3 smokeColor{0.86f, 0.88f, 0.92f};
    float trailSmokeScale = 0.0f;
    float fallbackTrailRate = 6.0f;
    HIKARI::GameProjectVfxAssetReference trailEffect{};
    HIKARI::GameProjectVfxAssetReference shellEffect{"80ac20536e6b4989b654e6ba7b223c24",
                                                     "Assets/Vfx/GourdShotFlame.hvfx"};
    HIKARI::GameProjectVfxAssetReference muzzleEffect{"a464fd20f18f7a9f5426b1050bb22283",
                                                      "Assets/Vfx/GourdShotMuzzle.hvfx"};
    HIKARI::GameProjectVfxAssetReference impactEffect{"45c172ae00b625b1e245690a8a313e3d",
                                                      "Assets/Vfx/GourdShotImpact.hvfx"};
    float shellScale = 1.0f;
    int shellFlashFrames = 3;
    int shellFadeFrames = 8;
    float muzzleScale = 1.0f;
    float impactScale = 1.0f;
    float impactFarScale = 0.6f;
    float impactStandoff = 0.6f;
    GourdFeedback feedback{};
    int traceEveryFrames = 0;

    GourdShotPhase phase = GourdShotPhase::Idle;
    bool attackHeld = false;
    bool attackPressed = false;
    int attackBuffer = 0;
    uint64_t processedTick = 0;
    uint64_t beginTick = 0;
    uint64_t nextFireTick = 0;
    uint64_t recoveryEndTick = 0;
    uint64_t lastShotTick = 0;
    uint64_t shotsFired = 0;
    uint64_t dryFires = 0;
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
    uint32_t burstCursor = 0;
    uint64_t burstsPlayed = 0;
    std::array<GourdProjectile, kGourdProjectileCapacity> projectiles{};
    std::array<GourdShotResidue, kGourdResidueCapacity> residues{};
    std::array<GourdShotBurst, kGourdBurstCapacity> bursts{};
};

} // namespace Project_TD2_2
