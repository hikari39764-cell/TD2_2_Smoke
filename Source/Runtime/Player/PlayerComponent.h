#pragma once

#include "HIKARI/Project/GameComponent.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kPlayerType = "Project_TD2_2.Player";
inline constexpr std::string_view kPlayerSystem = "Project_TD2_2.PlayerSystem";

enum class PlayerState { Normal, Roll, Heal, Stagger, Knockback, Down, GetUp, Dead };
enum class PlayerDamageKind { Stagger, Knockback };

class PlayerComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    float maxHealth = 100.0f;
    float moveSpeed = 5.2f;
    // 通常・回復中に向きを変える速さ。試作の 12 rad/秒。
    float turnSpeed = 688.0f;
    float maxStamina = 100.0f;
    int staminaDelayFrames = 40;
    float staminaPerSecond = 45.0f;
    int rollFrames = 30;
    int rollMoveFrames = 22;
    float rollDistance = 4.2f;
    // 進んだ距離 = rollDistance × (1 − (1 − 進み)^rollEasePower)。1 で等速、大きいほど出だしが速い。
    float rollEasePower = 1.6f;
    int rollInvincibleFrames = 14;
    float rollStamina = 25.0f;
    int inputBufferFrames = 12;
    int staggerFrames = 26;
    int staggerStepOneFrames = 9;
    int staggerStepTwoFrames = 9;
    float staggerStepOne = 0.75f;
    float staggerStepTwo = 0.3f;
    float staggerStepThree = 0.12f;
    int staggerHitStopFrames = 5;
    float knockbackDistance = 6.0f;
    int knockbackFrames = 30;
    // 吹っ飛びの放物線の頂点の高さ。knockbackFrames の半分で頂点に届き、終わりに着地する。
    float knockbackHeight = 1.3f;
    int knockbackHitStopFrames = 8;
    int downFrames = 40;
    int downRollFrame = 14;
    int getUpFrames = 22;
    int maxHealCharges = 4;
    float healAmount = 40.0f;
    int healFrames = 60;
    int healApplyFrame = 36;
    float healMoveSpeed = 1.5f;
    float maximumFallSpeed = 55.0f;
    float rollFogRadius = 1.0f;
    float rollFogStrength = 3.0f;
    float rollFogHeightOffset = -0.45f;
    // 歩き出し・止まる時に速さを変える時間（0 で一瞬）。回避・被弾などの動作には掛からない。
    float moveAccelSeconds = 0.08f;
    float moveDecelSeconds = 0.06f;
    uint64_t lockTargetId = 0;
    uint64_t cameraId = 0;
    // カメラ。姿勢は PlayerCamera が作る（Gameplay Camera の CameraFollow の値は使わない。画角だけカメラの値）。
    // 軸＝体の中心から cameraPivotHeight 上の点。視点は軸から cameraDistance 後ろ、cameraPitch だけ見下ろす。
    float cameraDistance = 6.0f;
    float cameraPivotHeight = 0.95f;
    float cameraPitch = 16.0f;
    // 肩越し。視点と注視点を一緒に横へずらす（負で左。プレイヤーが画面の中心より少し右に見える）。
    float cameraSideOffset = -0.6f;
    float cameraFollowSeconds = 0.09f;
    float cameraVerticalSeconds = 0.35f;
    float cameraVerticalDeadZone = 0.5f;
    float cameraMouseSensitivity = 0.12f;
    float cameraStickYawSpeed = 170.0f;
    float cameraStickPitchSpeed = 110.0f;
    float cameraMinPitch = -15.0f;
    float cameraMaxPitch = 55.0f;
    // 見上げた時に、視点が軸からどこまで下がってよいか (m)。地面へ潜らないようにする。
    float cameraEyeFloor = 1.5f;
    bool cameraTrace = false;
    // ロックオン中のカメラ。対象の向きへ lockYawSeconds でならして回り、回る速さは lockYawMaxSpeed まで。
    float lockYawSeconds = 0.16f;
    float lockYawMaxSpeed = 300.0f;
    float lockPitch = 17.0f;
    // 対象が高い所にいる時に見上げる割合（0 で見上げない、1 で対象の高さの角度だけ）。
    float lockPitchFollow = 0.6f;
    float lockPitchSeconds = 0.28f;

    // 実行時のデータは保存しない。ルールは PlayerRules と PlayerSystem が所有する。
    bool initialized = false;
    PlayerState state = PlayerState::Normal;
    int stateFrame = 0;
    bool advanceStateNextTick = false;
    float health = 0.0f;
    float stamina = 0.0f;
    int healCharges = 0;
    int staminaAge = 0;
    int rollBuffer = 0;
    int healBuffer = 0;
    int actionLockFrames = 0;
    int recoilFramesLeft = 0;
    int recoilFrames = 0;
    float recoilDistance = 0.0f;
    float recoilEasePower = 1.0f;
    HIKARI::GameProjectFloat3 recoilDirection{};
    HIKARI::GameProjectFloat3 moveInput{};
    HIKARI::GameProjectFloat3 cameraForward{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 facing{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 actionDirection{};
    HIKARI::GameProjectObjectHandle lockTarget{};
    bool lockedOn = false;
    // ロック対象を変える入力（Q）が押された。対象の候補を知っている側（ボスの分身）が使ったら消す。
    bool lockSwitchRequested = false;
    // ロック中のカメラが見る点を、対象の原点からずらす量（世界 m）。霧へ沈んだボスを見続ける時など、外部が毎 tick 入れる。
    HIKARI::GameProjectFloat3 lockPivotOffset{};
    bool rollFogPending = false;
    bool healApplied = false;
    bool motionAccepted = true;
    bool cameraAccepted = true;
    bool fluidAccepted = true;
    bool hitStopAccepted = true;
    int pendingHitStopFrames = 0;
    float externalMoveScale = 1.0f;
    float externalTurnSpeed = 0.0f;
    bool externalHealBlock = false;
    // 射撃の構え中に向く方向。毎 tick 外部が要求し、PlayerSystem が使ったら消す。
    bool aimTurnPending = false;
    float aimTurnSpeed = 0.0f;
    HIKARI::GameProjectFloat3 aimTurnDirection{0.0f, 0.0f, 1.0f};
    // いまの歩く速度（歩き出し・止まる時のならし）。
    HIKARI::GameProjectFloat3 moveVelocity{};
    // カメラの状態（PlayerCamera が持つ）。角度は rad。yaw は水平の向き、pitch は見下ろす角度。
    bool cameraReady = false;
    float cameraYaw = 0.0f;
    float cameraPitchNow = 0.0f;
    float cameraYawVelocity = 0.0f;
    float cameraPitchVelocity = 0.0f;
    float cameraFovRadians = 0.0f;
    HIKARI::GameProjectFloat3 cameraPivot{};
    HIKARI::GameProjectFloat3 cameraPivotVelocity{};
};

} // namespace Project_TD2_2
