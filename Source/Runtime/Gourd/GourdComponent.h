#pragma once

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kGourdType = "Project_TD2_2.Gourd";
inline constexpr std::string_view kGourdSystem = "Project_TD2_2.GourdSystem";

// 吸い込みの段階：構え（動けない）→ 吸う（ほぼ動けない・向きは変えられる）→ 後隙（動けない）。
enum class GourdAbsorbPhase { Idle, Windup, Absorbing, Recovery };

class GourdComponent final : public HIKARI::IGameComponent {
  public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    bool enabled = true;
    uint64_t playerId = 0;
    uint64_t cameraId = 0;
    HIKARI::GameProjectFloat3 shoulderOffset{0.65f, 0.45f, -0.1f};
    float followFrequency = 14.0f;
    float teleportDistance = 10.0f;
    float height = 0.5f;
    float radius = 0.18f;
    float absorbRadius = 1.0f;
    float absorbStrength = 28.0f;
    float absorption = 60.0f;
    float gatherRadius = 1.8f;
    float gatherStrength = 12.0f;
    int gatherRings = 3;
    float gatherFanDegrees = 60.0f;
    float gatherRange = 7.5f;
    float gatherFarScale = 1.0f;
    float gatherSwirl = 0.6f;
    float gatherPulse = 0.6f;
    float gatherPulseFrequency = 1.4f;
    float gatherLift = 0.3f;
    float gatherNearRadius = 0.0f;
    bool absorbWhenFull = false;
    float absorbCatchRadius = 1.1f;
    float absorbCatchHeight = 0.7f;
    float absorbCatchRate = 30.0f;
    float absorbForwardDistance = 1.3f;
    float absorbHeightOffset = -0.45f;
    int absorbWindupFrames = 24;
    int absorbRecoveryFrames = 30;
    float absorbMoveScale = 0.06f;
    float absorbTurnSpeed = 720.0f;
    float capacity = 100.0f;
    float initialSmoke = 100.0f;
    float smokePerVolume = 4.0f;
    HIKARI::GameProjectVfxAssetReference absorbEffect{"5f8903664eb0a5691bcc566b125bc81e",
                                                      "Assets/Vfx/GourdAbsorb.hvfx"};
    float absorbEffectScale = 1.0f;
    float absorbFullIntake = 40.0f;
    float absorbIdleEffect = 0.35f;
    float absorbFullEffect = 0.2f;
    float absorbGlowHeat = 0.0f;
    float absorbGlowRadius = 1.3f;
    float absorbGlowReach = 1.4f;
    int absorbRetireFrames = 12;
    float absorbFovDegrees = 2.0f;
    int absorbFovEaseFrames = 10;
    float absorbShakeAmplitude = 0.004f;
    float absorbShakeFrequency = 17.0f;
    float absorbPulseScale = 0.12f;
    float absorbPulseFrequency = 5.0f;
    float absorbRumble = 0.16f;
    int fullRumbleFrames = 8;
    float fullRumble = 0.7f;
    int traceEveryFrames = 0;

    // 累積回読値と所持量は別々に持ち、消費や容量超過を次の回読で戻さない。
    bool initialized = false;
    bool poseInitialized = false;
    bool absorbHeld = false;
    bool absorbing = false;
    GourdAbsorbPhase absorbPhase = GourdAbsorbPhase::Idle;
    int absorbPhaseFrames = 0;
    bool playerValid = false;
    bool transformAccepted = true;
    bool fluidAccepted = true;
    bool readAccepted = true;
    uint32_t fluidSubmissions = 0;
    float storedSmoke = 0.0f;
    double lastRemovedTotal = 0.0;
    double removedDelta = 0.0;
    uint64_t processedTick = 0;
    uint64_t renderFrameCount = 0;
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectFloat3 followPosition{};
    HIKARI::GameProjectFloat3 followVelocity{};
    // 肩への追従と砲撃の反動を分離し、反動後も元の追従位置へ戻す。
    HIKARI::GameProjectFloat3 recoilOffset{};
    HIKARI::GameProjectFloat3 recoilVelocity{};
    HIKARI::GameProjectFloat3 recoilPerFrame{};
    int recoilFramesLeft = 0;
    bool aimOverridePending = false;
    HIKARI::GameProjectFloat3 aimOverrideDirection{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 aimDirection{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 mouthPosition{};
    HIKARI::GameProjectFloat3 sinkPosition{};
    // 見た目と手応えだけの状態。吸い込み・残量の規則はここを読まない。
    HIKARI::GameProjectVfxPlaybackHandle absorbVfx{};
    bool vfxAccepted = true;
    bool wasFull = false;
    float absorbBlend = 0.0f;
    // 実際に吸えている速さ（煙／秒、ならした値）。口の演出の強さに使う。
    float intakeRate = 0.0f;
    float absorbSeconds = 0.0f;
    float mouthIdleSeconds = 0.0f;
};

} // namespace Project_TD2_2
