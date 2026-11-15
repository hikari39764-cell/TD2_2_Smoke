#pragma once

#include "../Boss/BossComponent.h"

namespace Project_TD2_2 {

// 部品 1 つの置き場所（世界座標）。
struct BossBodyPlacement {
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectQuaternion rotation{};
    float scale = 1.0f;
};

// 左手の形（模型のノードを差し替える。指の骨は無い）。
enum class BossBodyHandShape { Fist, Open, Seal };

// 外形の部品の置き場所の一式。
struct BossBodyPose {
    BossBodyPlacement sword;
    BossBodyPlacement rightHand;
    BossBodyPlacement rightCuff;
    BossBodyPlacement leftHand;
    BossBodyPlacement leftCuff;
    BossBodyHandShape leftShape = BossBodyHandShape::Open;
    // 刀身を、灼けた刀身（火の技・フェーズ 2）で出す。
    bool bladeHot = false;
};

// 外形の部品の置き場所を決める規則。ボスの状態（剣の姿勢・向き・高さ）から計算するだけで、エンジンへは何も頼まない。
// 模型の部品の軸の約束：
// - 刀・握りの右手：原点＝握りの中央、+Z＝剣先、-Y＝刃、+Y＝棟。
// - 左手：原点＝手首、+Z＝指先、+Y＝手の甲。
// - 籠手の筒：原点＝手首、-Z＝肘の向き、+Y＝腕の甲の側。
class BossBodyRig final {
public:
    static BossBodyPose Solve(const BossComponent& boss);
};

} // namespace Project_TD2_2
