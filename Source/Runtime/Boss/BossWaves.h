#pragma once

#include "BossFrame.h"

namespace Project_TD2_2 {

struct BurnExplosion;

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
    // 放った斬撃の id を返す（放てなければ 0）。
    static uint64_t SpawnSlash(BossFrame& frame, const BossSlashLaunch& launch);
    // 剣そのものが当たった：同じ一撃の、もう放ってある斬撃は当たらなくする。
    static void SpendWave(BossComponent& boss, uint64_t waveId);
    static void SpawnKnives(BossFrame& frame, int moveIndex, HIKARI::GameProjectFloat3 from,
                            HIKARI::GameProjectFloat3 aim);
    // 回し蹴りの霞の輪：ボスの足元から、表の判定サイズ（最大半径）まで斬撃速度で広がる。
    static void SpawnRing(BossFrame& frame, int moveIndex);
    // 飛ばす・当てる・吸い込みの扇に入ったものを吸わせる（DeliverCatch で渡し分に足す）。
    static void Step(BossFrame& frame);
    // 吸い取った斬撃・ナイフ 1 つを、ヒョウタンへの渡し分（boss.caught）へ足す：煙・数・火の刃なら火弾の知らせ。
    // 口へ入りきった tick に呼ぶ（c-3341a7）。
    static void DeliverCatch(BossComponent& boss, const BossWave& wave);
    // 口へ引き込み中の斬撃・ナイフが、入りきった時にヒョウタンへ入れる煙の合計（捕まえられる空きから先に引く）。
    static float GetPullingSmoke(const BossComponent& boss);
    // 刃や斬撃がプレイヤーの体の軸に触れたか（触れたら true）。
    static bool Touches(const BossFrame& frame, HIKARI::GameProjectFloat3 a, HIKARI::GameProjectFloat3 b,
                        float thickness);
    // プレイヤーに当てる。無敵なら *dodged を立てて（null なら知らせず）false。direction は攻撃が押す向き。
    // 一撃が当たるのは 1 回だけ：呼んだ側は、当たっても無敵で抜けられても、その一撃をもう当てない。
    // type は Slash か Explosion。当たれば、爆発か、火の付いた一撃（fire）ならプレイヤーが炎上する。
    static bool HitPlayer(BossFrame& frame, int damage, HIKARI::GameProjectFloat3 direction, bool blow, int move,
                          int strike, const char* source, PlayerDamageType type, bool fire, bool* dodged = nullptr);
    // 場の霧へ少しずつ入れる煙を置く（煙の単位）。waitFrames tick 待ってから、frames tick かけて出し切る。
    // mergeDistance が正なら、その距離より近い「まだ待っている煙」があれば、新しい枠を使わずにそこへ足す。
    static void AddDeposit(BossComponent& boss, HIKARI::GameProjectFloat3 position, float smoke, float radius,
                           int frames, int waitFrames = 0, float mergeDistance = 0.0f);
    // 爆発の枠へ 1 つ入れる。枠は輪で使い回し、埋まっていれば一番古いものを上書きする。
    // 爆発の跡に残す煙（blast.leavesFog）は、ここで置く（枠が上書きされても煙は残る）。
    static void AddBlast(BossComponent& boss, const BossBlast& blast);
    // 引爆の前線が 1 tick に爆ぜさせた塊（BurnFieldRules::Explode／ExplodeDisc の結果）の跡に煙を置く。
    // 塊ごとに、爆ぜた面積 × detonationFogSmokePerArea。合計は limit まで（前線が先に通った塊から）。置いた煙の合計を返す。
    // source はログの名前。
    static float AddDetonationFog(BossComponent& boss, const BurnExplosion& explosion, float limit, const char* source);
    static void StepDeposits(BossComponent& boss);
    static void Clear(BossComponent& boss);
};

} // namespace Project_TD2_2
