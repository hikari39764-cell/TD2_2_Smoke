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
inline constexpr size_t kGourdFogDepositCapacity = 12;
inline constexpr size_t kGourdFireZoneCapacity = 12;
// 同時に残す一度きりの炎の域の枠。流体域は 1 フレームに 6 個まで（均衡画質）で、場の霧が 1 つ使う。
// 砲撃の域（砲口・命中・弾）は合わせて kGourdShotDomainBudget 個までにし、新しい域を出す前に古いものから止める。
// 止めた域も、そのフレームの間は数に入るので、上限より 1 つ少なくしておく。
inline constexpr size_t kGourdMuzzleLiveCapacity = 1;
inline constexpr size_t kGourdImpactLiveCapacity = 2;
inline constexpr size_t kGourdShotDomainBudget = 4;

enum class GourdShotPhase { Idle, Windup, Recovery };

struct GourdProjectile {
    bool active = false;
    bool finishTrailPending = false;
    uint64_t id = 0;
    uint64_t spawnedTick = 0;
    HIKARI::GameProjectFloat3 origin{};
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectFloat3 direction{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectObjectHandle ignoredPlayer{};
    float distance = 0.0f;
    float speed = 0.0f;
    float radius = 0.0f;
    float densityScale = 1.0f;
    // 発射した高さ（地面から）。負は地面が見つからなかった。足元の霧を割る位置に使う。
    float groundClearance = -1.0f;
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

// 撃った煙を場の霧へ戻す予約。近くに炎の域がある間は待ち、消えてから地面へ落とす。
struct GourdFogDeposit {
    bool active = false;
    HIKARI::GameProjectFloat3 ground{};
    // 落とし始めの高さ（地面から）。終点の煙は爆ぜた高さから地面へ降ろす。
    float dropHeight = 0.0f;
    float smokeLeft = 0.0f;
    float smokePerFrame = 0.0f;
    // この固定 tick の間に戻す速さ（煙／秒）。0 は待機中。描画側が読む。
    float rate = 0.0f;
    int delayFrames = 0;
    int age = 0;
};

// 炎の域（砲口・命中）が残っている場所。戻す煙はこの近くを避ける。
struct GourdFireZone {
    int framesLeft = 0;
    HIKARI::GameProjectFloat3 position{};
};

class GourdShotComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    bool enabled = true;
    int windupFrames = 12;
    // 長押し連射の最初の間隔。n 発目の後の間隔は
    // max(rapidMinFrames, round(repeatFrames / min(rapidMax, 1 + rapidAccel × (n − 1))))。
    int repeatFrames = 24;
    float rapidAccel = 0.5f;
    float rapidMax = 3.0f;
    int rapidMinFrames = 4;
    int recoveryFrames = 56;
    // 最後の発射からこの F 以降は、攻撃を離して吸い込みを押すと後隙を取り消して吸い込みの構えへ移れる。
    int absorbCancelFrames = 6;
    int inputBufferFrames = 12;
    float staminaCost = 20.0f;
    float smokeCost = 20.0f;
    float recoilDistance = 0.7f;
    int recoilFrames = 8;
    // 反動の進み方。2 で初め速くあとゆっくり、1 で等速。
    float recoilEasePower = 2.0f;
    // 構えの間と長押しの間に、撃つ向きへ振り向く速さ（度/秒。573 ≒ 10 rad/s）。
    float aimTurnSpeed = 573.0f;
    // ロックしていない時はカメラの正面へ撃つ。下向きはこの値（方向の y 成分）までにして、足元へ撃ち込まない。
    float aimMinVertical = -0.05f;
    float gourdRecoilDistance = 0.48f;
    int gourdRecoilFrames = 8;
    float gourdWindupPull = 0.14f;
    float damage = 40.0f;
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
    float fogReturnSmoke = 20.0f;
    float fogReturnTrailShare = 0.3f;
    float fogReturnTrailSpacing = 2.5f;
    float fogReturnRadius = 1.4f;
    float fogReturnHeight = 0.35f;
    int fogReturnFrames = 40;
    int fogReturnDelayFrames = 6;
    int fogReturnFallFrames = 14;
    float fogReturnSink = 1.5f;
    float fogReturnWander = 0.5f;
    float fogReturnSwirl = 1.2f;
    float fogReturnClearance = 3.4f;
    int impactFireFrames = 48;
    int muzzleFireFrames = 18;
    float shellWakeStrength = 22.0f;
    float shellWakeRadius = 0.8f;
    float shellWakeSpread = 0.5f;
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
    // 押し続けている間の何発目か。離すと 0 に戻り、次の長押しは最初の間隔から。
    int chainShots = 0;
    bool chainHeld = false;
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
    uint32_t fireZoneCursor = 0;
    float fogReturned = 0.0f;
    float fogReturnQueued = 0.0f;
    std::array<GourdProjectile, kGourdProjectileCapacity> projectiles{};
    std::array<GourdShotResidue, kGourdResidueCapacity> residues{};
    std::array<GourdShotBurst, kGourdBurstCapacity> bursts{};
    std::array<GourdFogDeposit, kGourdFogDepositCapacity> fogDeposits{};
    std::array<HIKARI::GameProjectVfxPlaybackHandle, kGourdMuzzleLiveCapacity> muzzleLive{};
    std::array<HIKARI::GameProjectVfxPlaybackHandle, kGourdImpactLiveCapacity> impactLive{};
    uint32_t muzzleLiveCursor = 0;
    uint32_t impactLiveCursor = 0;
    std::array<GourdFireZone, kGourdFireZoneCapacity> fireZones{};
};

} // namespace Project_TD2_2
