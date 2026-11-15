#pragma once

#include "HIKARI/Project/WorldView.h"
#include "../Boss/BossComponent.h"

namespace Project_TD2_2 {

// ボスの外形の模型を出す。置き場所は BossBodyRig が決め、ここは模型のノードを 1 つずつ、表示専用の模型として
// 毎フレーム頼むだけ（物体・当たり・動画は作らない。頼まなかったフレームは消える）。
class BossBodyVisuals final {
public:
    static void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss);
};

} // namespace Project_TD2_2
