#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"

namespace Project_TD2_2 {

// 天降り斬りの見た目：跳ぶ所の霞、一緒に落ちる刃、着地の周りの塊と炎の輪、落ちる所を決めた瞬間の剣の光。
// 規則が書く boss.skyfall を読むだけ（当たりや落ちる所は変えない）。地面には何も予告しない。
// 状態は boss.skyfallVisual に持つ。
class BossSkyfallVisuals final {
public:
    // 爆発の枠へ塊を入れるので、枠を出す所（BossVisuals）より前に呼ぶ。
    static void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds);
    static void Stop(HIKARI::WorldView& world, BossComponent& boss);
};

} // namespace Project_TD2_2
