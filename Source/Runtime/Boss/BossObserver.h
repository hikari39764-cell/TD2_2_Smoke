#pragma once

#include <cstdint>

#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"
#include "../Player/PlayerComponent.h"

namespace Project_TD2_2 {

// ボスが見るプレイヤーの道具（ヒョウタンと砲撃）。BossSystem が固定 tick の始めに、公開の規則の関数
// （GourdRules・GourdShotRules）から集めて渡す。
struct BossGearSight {
    // ヒョウタンが吸い込みの構えをしているか（煙が入るかどうかに関わらず）。
    bool absorbing = false;
    bool hasGourd = false;
    // 煙の量 ÷ 容量 (%)。満杯＝吸い込める空きが無い。
    float smokePercent = 0.0f;
    bool gourdFull = false;
    // 砲撃：構えの間か、押し続けて次の弾を待っているか。撃った弾の累計。構えの間と押し続けている間の狙い。
    bool shotWindup = false;
    bool shotChaining = false;
    uint64_t shotsFired = 0;
    HIKARI::GameProjectFloat3 aimOrigin{};
    HIKARI::GameProjectFloat3 aimDirection{0.0f, 0.0f, 1.0f};
};

// ボスがプレイヤーを見る：回復中か・吸い込みを続けている時間・火の中にいる時間・ローリングを始めた時刻と向きと癖、
// 射撃の構え・狙い・続けて撃った数、立ち位置（ボスの正面からの角度と距離）の続き、プレイヤーの残り（ヒョウタン・精力・
// HP・回復の数）、技ごとの咎められた回数、場の燃えている面積。固定 tick ごとに boss.watch へ書き、行動ツリーの条件の節は
// それを読むだけにする（節は思考の tick にしか呼ばれないので、続けている時間や「始めた瞬間」は節の側では数えられない）。
class BossObserver final {
public:
    // player が nullptr ならプレイヤーはいない（何も成り立たない）。tickSeconds は 1 固定 tick の秒。
    static void Observe(HIKARI::WorldView& world, BossComponent& boss, const PlayerComponent* player,
                        HIKARI::GameProjectFloat3 playerCenter, const BossGearSight& gear, float tickSeconds);
    // ボスが hits 回当てられた。recoveryRow はその時に後隙だった技の行（後隙でなければ -1）。後隙なら咎められた回数に足す。
    static void NotePunished(BossComponent& boss, int recoveryRow, uint64_t hits);

    // 以下は読むだけ（行動ツリーの節・技の向きを決める所が使う）。
    // 最近 rolls 回のローリングの向きを数える（counts は BossRollDirection の順）。数えた回数（覚えている数まで）を返す。
    static int CountRecentRolls(const BossPlayerWatch& watch, int rolls, int (&counts)[4]);
    // プレイヤーがボスの正面から atLeastDegrees 度以上の所に、いままで続けて何秒いるか。
    static float SecondsBehind(const BossPlayerWatch& watch, float atLeastDegrees);
    // プレイヤーがボスから atLeastMeters m 以上の所に、いままで続けて何秒いるか。
    static float SecondsFar(const BossPlayerWatch& watch, float atLeastMeters);
    // 次に来る弾が、続けて撃つ何発目か（前の弾から watchShotChainFrames F を過ぎていれば 1）。
    static int NextShotOrdinal(const BossComponent& boss);
};

} // namespace Project_TD2_2
