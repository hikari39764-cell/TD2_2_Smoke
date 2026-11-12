#pragma once

#include "HIKARI/Project/GameComponent.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kDamageableType = "Project_TD2_2.Damageable";

class DamageableComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    float maxHealth = 1000.0f;
    bool invulnerable = false;
    bool traceEveryHit = false;

    // 実行時の記録は保存しない。敵の種類や被弾後の動作には依存しない。
    bool initialized = false;
    float currentHealth = 0.0f;
    float lastDamage = 0.0f;
    HIKARI::GameProjectFloat3 lastDirection{};
    uint64_t hitCount = 0;
};

} // namespace Project_TD2_2
