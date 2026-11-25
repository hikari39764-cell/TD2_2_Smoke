#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BurnFieldComponent.h"

namespace Project_TD2_2 {

// 燃える地面の部品だけを相手にする規則。ほかの機能が場面の燃える地面に触る入口は BurnField。
class BurnFieldRules final {
public:
    // 格子が出来ていなければ、持ち主の物体の位置で作る（BurnFieldSystem が最初の固定 tick に呼ぶ）。
    static void Initialize(HIKARI::ObjectView object, BurnFieldComponent& field);
};

} // namespace Project_TD2_2
