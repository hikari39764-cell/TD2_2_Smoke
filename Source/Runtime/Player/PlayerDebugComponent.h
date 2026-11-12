#pragma once

#include "HIKARI/Project/GameComponent.h"

#include "PlayerComponent.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kPlayerDebugType = "Project_TD2_2.PlayerDebug";
inline constexpr std::string_view kPlayerDebugSystem = "Project_TD2_2.PlayerDebugSystem";

class PlayerDebugComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    // 正式シーンに検証用の被弾やログを持ち込まないため、既定では無効にする。
    bool enabled = false;
    int damageAtTick = -1;
    float damageAmount = 20.0f;
    PlayerDamageKind damageKind = PlayerDamageKind::Stagger;
    HIKARI::GameProjectFloat3 damageDirection{0.0f, 0.0f, -1.0f};
    int damageRepeatInterval = 0;
    int damageRepeatCount = 1;
    int traceEveryFrames = 1;
    int consumeStaminaAtTick = -1;
    float consumeStaminaAmount = 20.0f;
    bool consumeAllowInsufficient = true;
    int lockAtTick = -1;
    int lockFrames = 36;
    int recoilAtTick = -1;
    HIKARI::GameProjectFloat3 recoilDirection{0.0f, 0.0f, -1.0f};
    float recoilDistance = 2.0f;
    int recoilFrames = 8;

    uint64_t elapsedTicks = 0;
    uint64_t processedTick = 0;
    int damageAttempts = 0;
};

} // namespace Project_TD2_2
