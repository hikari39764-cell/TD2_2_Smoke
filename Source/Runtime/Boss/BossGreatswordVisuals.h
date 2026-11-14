#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"

namespace Project_TD2_2 {

// 霞の大剣の見た目：大剣の模型の輪郭へ流体を流し込むエフェクトを、握りの位置と剣の向きに合わせて持って行く。
// 薙ぎ終わった瞬間に、剣が通った扇の場の霧を外へ押す。
// 縦割りは、振りかぶる間に刃をふくらませ、叩きつけた瞬間に剣先へ塊を出して刃の線で場の霧を左右へ割る。地を走る
// 噴き上がりは、前線と一緒に縦の刃（斬撃のエフェクト）を走らせ、足元の霧を割り、炎の大剣なら炎を吹き上げる。
// 規則が書く boss.greatsword を読むだけ（規則は変えない）。
// 状態は boss.greatswordVisual に持つ。
class BossGreatswordVisuals final {
public:
    static void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds);
    static void Stop(HIKARI::WorldView& world, BossComponent& boss);
};

} // namespace Project_TD2_2
