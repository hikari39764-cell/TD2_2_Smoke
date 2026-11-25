#pragma once

#include <array>

#include "../Boss/BossComponent.h"

namespace Project_TD2_2 {

// 部品 1 つの置き場所（世界座標）。
struct BossBodyPlacement {
    HIKARI::GameProjectFloat3 position{};
    HIKARI::GameProjectQuaternion rotation{};
    float scale = 1.0f;
    // 局所の Z の向きだけに掛ける倍率（籠手の筒の長さを詰める）。
    float lengthScale = 1.0f;
};

// 体に付く効果（刀光・刀の霞・手の光・胴の火と煙・目・変わり目の火）を置く所の一式。体そのものは骨格の体
// （BossAnimBody）か登場の役者が描き、ここはその骨から置き場所を受け取る（BossAnimBody::ApplyPose・
// BossIntroVisuals::ApplyPose）。
struct BossBodyPose {
    BossBodyPlacement sword;
    BossBodyPlacement rightHand;
    BossBodyPlacement rightCuff;
    BossBodyPlacement leftHand;
    BossBodyPlacement leftCuff;
    BossBodyHandShape leftShape = BossBodyHandShape::Open;
    // 両手を描くか（登場で、手が煙から現れる前は描かない）。
    bool showHands = true;
    // 刀身を、灼けた刀身（火の技・フェーズ 2）で出す。
    bool bladeHot = false;
    // 剣先の、地面からの高さ (m)。
    float swordTipClearance = 1000.0f;
    // 鎧：胴（胸当て）、兜（目の光も同じ置き場所）、肩当て（右・左）、草摺（腰の周りに tassetCount 枚）。
    BossBodyPlacement torso;
    BossBodyPlacement head;
    std::array<BossBodyPlacement, 2> sode;
    std::array<BossBodyPlacement, kBossBodyTassetCapacity> tassets;
    int tassetCount = 0;
    // 目の光を火の色で出す（フェーズ 2）。気絶・撃破の間は目の光を出さない。
    bool eyesFire = false;
    bool eyesLit = true;
    // 鎧を灼けた鎧で出す（フェーズ 2 の本体）。
    bool armorHot = false;
};

class BossBodyRig final {
public:
    // 骨から置き場所を受け取る前の一式：どの部品も体の物体の所と向きに置き、大きさ・灼け・目の光をボスの状態から決める。
    static BossBodyPose Base(const BossComponent& boss, const HIKARI::GameProjectTransform& body);
};

} // namespace Project_TD2_2
