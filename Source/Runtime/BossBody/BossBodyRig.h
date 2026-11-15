#pragma once

#include "../Boss/BossComponent.h"

namespace Project_TD2_2 {

// 部品 1 つの置き場所（世界座標）。
struct BossBodyPlacement {
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectQuaternion rotation{};
    float scale = 1.0f;
};

// 外形の部品の置き場所の一式。
struct BossBodyPose {
    BossBodyPlacement sword;
    BossBodyPlacement rightHand;
};

// 外形の部品の置き場所を決める規則。ボスの状態（剣の姿勢・向き・高さ）から計算するだけで、エンジンへは何も頼まない。
// 模型の部品の軸の約束：原点＝握りの中央、+Z＝剣先、-Y＝刃、+Y＝棟。
class BossBodyRig final {
public:
    static BossBodyPose Solve(const BossComponent& boss);
};

} // namespace Project_TD2_2
