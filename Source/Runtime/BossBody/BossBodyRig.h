#pragma once

#include <array>

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
    // 鎧：胴、兜（目の光も同じ置き場所）、大袖（右・左）、草摺（腰の周りに tassetCount 枚）。
    BossBodyPlacement torso;
    BossBodyPlacement head;
    std::array<BossBodyPlacement, 2> sode;
    std::array<BossBodyPlacement, kBossBodyTassetCapacity> tassets;
    int tassetCount = 0;
    // 目の光を火の色で出す（フェーズ 2）。気絶・撃破の間は目の光を出さない。
    bool eyesFire = false;
    bool eyesLit = true;
};

// 外形の部品の置き場所を決める規則。ボスの状態（剣の姿勢・向き・高さ）と体の物体の変換から計算するだけで、
// エンジンへは何も頼まない。模型の部品の軸の約束：
// - 刀・握りの右手：原点＝握りの中央、+Z＝剣先、-Y＝刃、+Y＝棟。
// - 左手：原点＝手首、+Z＝指先、+Y＝手の甲。
// - 籠手の筒：原点＝手首、-Z＝肘の向き、+Y＝腕の甲の側。
// - 胴・兜：向きは体と同じ。大袖・草摺：原点＝吊る所、+Z＝外向き、-Y へ垂れる。
class BossBodyRig final {
public:
    // 垂れた部品の振れを 1 フレーム進める（体の動きに遅れて振れ、行き過ぎて戻る。撃たれたら弾かれる）。
    static void StepSway(BossComponent& boss, const HIKARI::GameProjectTransform& body, float seconds);
    // body は体の物体の世界の変換（傾き・屈みが入っている。鎧はこれに付いて動く）。
    static BossBodyPose Solve(const BossComponent& boss, const HIKARI::GameProjectTransform& body);
};

} // namespace Project_TD2_2
