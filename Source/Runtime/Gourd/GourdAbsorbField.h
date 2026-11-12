#pragma once

#include "HIKARI/Project/FrameContext.h"
#include "GourdComponent.h"

namespace Project_TD2_2 {

// 吸い込み中に場の霧を動かす力の並び。前方の扇の奥から口の前へ霧を運び、口元で回収する。
class GourdAbsorbField final {
public:
    // 描画フレームごとに、吸い込み中だけ呼ぶ。seconds は吸い込みの経過（脈動の位相）。
    static void Submit(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdComponent& gourd, float seconds);
    // 吸い込みが場から消した霧の累計（口元・口の上・扇の中の力の合計。密度 × m³）。減ることはない。
    // 読めなければ false。
    static bool ReadRemoved(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdComponent& gourd,
                            double& outTotal);
};

} // namespace Project_TD2_2
