#pragma once

#include <cstdio>

#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"
#include "../Player/Player.h"

namespace Project_TD2_2 {

class BossIntroComponent;
class BossPhaseShiftComponent;

// 1 固定 tick ぶんの、ボスの処理に要るもの。参照はこの tick の中だけで使う。
struct BossFrame {
    HIKARI::WorldView& world;
    BossComponent& boss;
    // 同じ tick に見つかったプレイヤー（いなければ nullptr）。
    Player* player = nullptr;
    HIKARI::GameProjectFloat3 playerCenter{};
    float dt = 1.0f / 60.0f;
    // ヒョウタンが吸い込み中（満杯かどうかは問わない。捕まえられるかは catchRoom も見る）。
    bool absorbing = false;
    // 飛ぶ斬撃・ナイフをあと取り込める煙の量（ヒョウタンの空きから、口へ引き込み中の分を引いた値。満杯でも吸える設定なら
    // float の最大）。捕まえるたびに、その斬撃が入れる煙を引く（同じ tick のほかのボスとも分け合う）。nullptr は 0 と同じ。
    float* catchRoom = nullptr;
    // ヒョウタンの口の位置（捕まえた斬撃・ナイフを引き込む先）。ヒョウタンが無ければ hasMouth が偽。
    HIKARI::GameProjectFloat3 mouth{};
    bool hasMouth = false;
    // 同じ物体の登場の部品（無ければ nullptr：登場をしない。c-eb3b08）。
    BossIntroComponent* intro = nullptr;
    // 同じ物体のフェーズの変わり目の演出の部品（無ければ nullptr：演出をしない。c-3a0f87）。
    BossPhaseShiftComponent* shift = nullptr;
};

// プレイヤーの真下の、ボスの地面の高さの点（ボスから見たプレイヤーの水平の向き・距離を測る）。
inline HIKARI::GameProjectFloat3 PlayerGround(const BossFrame& frame) {
    return {frame.playerCenter.x, frame.boss.ground.y, frame.playerCenter.z};
}

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
