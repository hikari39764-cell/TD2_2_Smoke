#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"

namespace Project_TD2_2 {

// 炎走りの見た目。規則（BossFlameRun）が boss.flameRun に書いた前線を読んで、場の火の域の流体で見せる
// （専用の流体域は持たない。5 本が同時に走っても、域の数は増えない）。
// ・前隙：地面を引きずる剣先から、地を這う小さな炎。
// ・放った瞬間：走り始めの所に小さな火球。
// ・走る間：前線の所へ、上と前へ速さのある燃料を入れ続ける（同じ番号で動かすので、エンジンが通った線に沿って広げ、
//   炎の波頭が地面を走る）。同じ向きの力で、場の霧を前へ割る。通った後の火の線は、燃える地面が出す。
// ・走り切った瞬間：先端に火球。
class BossFlameRunVisuals final {
public:
    // 描画フレームごとに呼ぶ。
    static void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss);
};

} // namespace Project_TD2_2
