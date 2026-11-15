#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossBodyRig.h"
#include "../Boss/BossComponent.h"

namespace Project_TD2_2 {

// 技の点描の演出（見た目だけ）：刀の振りの刀光など、見える刀・体に付く飾り。
// 規則の剣の線・当たり・F 数は読むだけで変えない。地面や画面への予告は出さない。
class BossBodyAccents final {
public:
    // 外形の置き場所（pose）が決まった後に呼ぶ。
    static void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                       const BossBodyPose& pose, float seconds);
    static void Stop(HIKARI::WorldView& world, BossComponent& boss);
};

} // namespace Project_TD2_2
