#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"
#include "../Player/PlayerComponent.h"

namespace Project_TD2_2 {

// ボスがプレイヤーを見る：回復中か・吸い込みを続けている時間・火の中にいる時間・ローリングを始めた時刻と向き、
// 場の燃えている面積。固定 tick ごとに boss.watch へ書き、行動ツリーの反応の節はそれを読むだけにする
// （節は思考の tick にしか呼ばれないので、続けている時間や「始めた瞬間」は節の側では数えられない）。
class BossObserver final {
public:
    // player が nullptr ならプレイヤーはいない（何も成り立たない）。absorbing はヒョウタンが吸い込み中か。
    static void Observe(HIKARI::WorldView& world, BossComponent& boss, const PlayerComponent* player,
                        HIKARI::GameProjectFloat3 playerCenter, bool absorbing);
};

} // namespace Project_TD2_2
