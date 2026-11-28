#pragma once

#include "HIKARI/Project/GameComponent.h"

namespace Project_TD2_2 {


// 霞歩のいまの様子。規則（BossKasumiStep）が固定 tick ごとに書く。保存しない。
struct BossKasumiStepState {
    // 跳ぶ所と行き先（地面の高さ）、跳ぶ高さ、選んだ向き（ログ用）、弾をかわす霞歩か、着地したか。技を始めた tick に決める。
    HIKARI::GameProjectFloat3 from{};
    HIKARI::GameProjectFloat3 to{};
    float height = 0.0f;
    int direction = 0;
    bool dodge = false;
    bool landed = false;
    // 続けてかわした回数、最後にかわした時刻、かわさない間の終わり（どれも boss.clockSeconds の秒）。
    int dodges = 0;
    float lastDodgeSeconds = -1000.0f;
    float lockedUntilSeconds = -1000.0f;
};

} // namespace Project_TD2_2
