#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"

namespace Project_TD2_2 {

// ボスの霞を場の霧の域へ入れる（新しい流体域は作らない）。剣が描く弧・飛ぶ斬撃・消えた所・通った跡。
// 入れた煙は場の霧と同じなので、ヒョウタンの吸い込みでそのまま吸える。量は煙の単位で数え、
// smokePerVolume（ヒョウタンの換算：煙 ÷ 密度 × m³）で注入の強さへ直す。
class BossSmoke final {
public:
    // 描画フレーム（PreRender）。固定 tick で決めた 1 秒あたりの量を、そのまま注入の強さにする。
    static void Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float smokePerVolume);
};

} // namespace Project_TD2_2
