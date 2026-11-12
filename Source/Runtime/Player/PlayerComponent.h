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
    float maxStamina = 100.0f;
    int staminaDelayFrames = 40;
    float staminaPerSecond = 45.0f;
    int rollFrames = 30;
    int rollMoveFrames = 22;
    float rollDistance = 4.2f;
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
    uint64_t lockTargetId = 0;
    uint64_t cameraId = 0;
    float lockTargetWeight = 0.45f;
    float lockYawSpeed = 540.0f;
    float lockPitch = 17.0f;
    float lockPitchSpeed = 240.0f;
    float lockDistanceScale = 0.18f;
    float lockMaximumDistanceOffset = 2.0f;

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
    HIKARI::GameProjectFloat3 recoilPerFrame{};
    HIKARI::GameProjectFloat3 moveInput{};
    HIKARI::GameProjectFloat3 cameraForward{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 facing{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 actionDirection{};
    HIKARI::GameProjectObjectHandle lockTarget{};
    bool lockedOn = false;
    bool rollFogPending = false;
    bool healApplied = false;
    bool motionAccepted = true;
    bool cameraAccepted = true;
    bool fluidAccepted = true;
    bool hitStopAccepted = true;
    int pendingHitStopFrames = 0;
    float externalMoveScale = 1.0f;
    float externalTurnSpeed = 0.0f;
    bool externalActionBlock = false;
};

} // namespace Project_TD2_2
