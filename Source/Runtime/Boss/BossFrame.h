#pragma once

#include <cstdio>

#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"
#include "../Player/PlayerComponent.h"

namespace Project_TD2_2 {

// 1 固定 tick ぶんの、ボスの処理に要るもの。参照はこの tick の中だけで使う。
struct BossFrame {
    HIKARI::WorldView& world;
    BossComponent& boss;
    // 同じ tick に見つかったプレイヤー（いなければ nullptr）。
    PlayerComponent* player = nullptr;
    HIKARI::GameProjectFloat3 playerCenter{};
    float dt = 1.0f / 60.0f;
};

namespace BossLog {

inline void Write(const char* message) { HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Info, "Boss", message); }

inline void Warn(const char* message) {
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Warning, "Boss", message);
}

} // namespace BossLog

// 見た目の乱数（浮く高さ・移動の長さ・左右）。技の選び方の乱数とは別。
inline float BossRandom(BossComponent& boss) {
    boss.random = boss.random * 1103515245u + 12345u;
    return static_cast<float>((boss.random >> 8) & 0xFFFFFFu) / 16777216.0f;
}

} // namespace Project_TD2_2
