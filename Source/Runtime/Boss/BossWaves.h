#pragma once

#include "BossFrame.h"

namespace Project_TD2_2 {

// 放った斬撃をどう飛ばすか（振った剣の弧から決めた値）。
struct BossSlashLaunch {
    HIKARI::GameProjectFloat3 from{};
    HIKARI::GameProjectFloat3 direction{0.0f, 0.0f, 1.0f};
    HIKARI::GameProjectFloat3 across{1.0f, 0.0f, 0.0f};
    // 振った弧の長さ。放った直後はこの幅から判定サイズまで伸びる（弧と斬撃を地続きにする）。
    float startWidth = 0.0f;
    // 振っている剣がもう当たっていたら、飛ぶ斬撃は当たらない（同じ一撃）。
    bool noHit = false;
    int move = -1;
    int strike = 0;
};

// 霞の斬撃とナイフ：飛ばす・プレイヤーに当てる・通った跡に残り煙を置く。
class BossWaves final {
public:
    static void SpawnSlash(BossFrame& frame, const BossSlashLaunch& launch);
    static void SpawnKnives(BossFrame& frame, int moveIndex, HIKARI::GameProjectFloat3 from,
                            HIKARI::GameProjectFloat3 aim);
    static void Step(BossFrame& frame);
    // 刃や斬撃がプレイヤーの体の軸に触れたか（触れたら true）。
    static bool Touches(const BossFrame& frame, HIKARI::GameProjectFloat3 a, HIKARI::GameProjectFloat3 b,
                        float thickness);
    // プレイヤーに当てる。無敵なら dodged を立てて false。direction は攻撃が押す向き。
    static bool HitPlayer(BossFrame& frame, int damage, HIKARI::GameProjectFloat3 direction, bool blow, int move,
                          int strike, const char* source, bool& dodged);
    // 場の霧へ少しずつ入れる煙を置く（煙の単位）。
    static void AddDeposit(BossComponent& boss, HIKARI::GameProjectFloat3 position, float smoke, float radius);
    static void StepDeposits(BossComponent& boss);
    static void Clear(BossComponent& boss);
};

} // namespace Project_TD2_2
