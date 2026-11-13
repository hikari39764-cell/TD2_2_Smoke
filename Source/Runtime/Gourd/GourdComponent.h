#pragma once

#include <array>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"
#include "../Combat/Feedback.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kGourdType = "Project_TD2_2.Gourd";
inline constexpr std::string_view kGourdSystem = "Project_TD2_2.GourdSystem";

// 吸い込みの段階：構え（吸う力はまだ出ない）→ 吸う → 後隙。どの段階でもゆっくり歩けて、向きも変えられる。
// ローリングはどの段階でも出せて、吸い込みを後隙なしで打ち切る。
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
    float gatherAbsorption = 2.0f;
    float gatherSmokeShare = 0.25f;
    bool absorbWhenFull = true;
    float absorbCatchRadius = 1.1f;
    float absorbCatchHeight = 0.7f;
    float absorbCatchRate = 30.0f;
    float absorbForwardDistance = 1.3f;
    float absorbHeightOffset = -0.45f;
    // 吸ってすぐ次の行動へ移れるように、構えと後隙は短く、その間も歩ける（c-28900b。元の値は調整ガイド）。
    int absorbWindupFrames = 5;
    int absorbMinFrames = 10;
    int absorbRecoveryFrames = 8;
    int absorbBufferFrames = 12;
    float absorbMoveScale = 0.5f;
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
    // ボスの斬撃・ナイフを吸い取った瞬間の手応え（c-d330c0）。止め・手柄の振動・視野角の引き。
    int catchHitStopFrames = 3;
    float catchHitStopScale = 0.05f;
    float catchRumbleLow = 0.5f;
    float catchRumbleHigh = 0.8f;
    int catchRumbleFrames = 6;
    float catchFovDegrees = 1.5f;
    int catchFovFrames = 8;
    // 吸い取った瞬間に口の位置で一度だけ出すエフェクト（口へ縮む歪みの輪・冷たい白の閃き・口へ飛び込む光の粒）。
    // 空なら何も出さない。
    HIKARI::GameProjectVfxAssetReference catchEffect{"a3d5c1e09b7f4e28b6140f2d8c9e7a51", "Assets/Vfx/GourdCatch.hvfx"};
    // 吸い取った時、飲み込んだようにヒョウタンが一度膨らんで戻る（膨らむ割合と、戻るまでの秒）。
    float catchPulseScale = 0.4f;
    float catchPulseSeconds = 0.18f;
    int traceEveryFrames = 0;

    // 累積回読値と所持量は別々に持ち、消費や容量超過を次の回読で戻さない。
    bool initialized = false;
    bool poseInitialized = false;
    bool absorbHeld = false;
    // 動けない間に押された吸い込みを覚えておく残り F（押した F を含む。0 で無し）。
    // 動けるようになった最初の F に構える。
    int absorbBuffer = 0;
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
    // 吸い取った数の累計（ボスの規則が増やす）と、手応えを出し終えた分。差が出たら次の描画フレームで見せる。
    uint64_t catchCount = 0;
    uint64_t catchObservedCount = 0;
    // 膨らみを出し終えた分と、膨らみの残り秒。
    uint64_t catchPulseObserved = 0;
    float catchPulseLeft = 0.0f;
    Feedback::CameraShake catchShake{};
    float absorbBlend = 0.0f;
    // 実際に吸えている速さ（煙／秒、ならした値）。口の演出の強さに使う。
    float intakeRate = 0.0f;
    float absorbSeconds = 0.0f;
    float mouthIdleSeconds = 0.0f;
    // 霧を消す力ごとの、前に読んだ累計（0 は口元、1 は口の上、2 からは扇の力）と、増えた分を足し続けた合計。
    // 力ごとに差を取るので、力の数が変わっても（輪の数を実行中に変えた時など）古い累計を二重に数えない。
    std::array<double, 24> removedByForce{};
    double removedSum = 0.0;
};

} // namespace Project_TD2_2
