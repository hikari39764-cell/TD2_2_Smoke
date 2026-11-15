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

// 鎧と待機の構えを置くのに要る入力（本体と霞分身で共通。分身のために BossComponent を写さない）。
struct BossBodyRigInput {
    // 設定は本体の Boss.body の 1 つだけを使う。
    const BossBodySettings& settings;
    // 体の物体の世界の変換（原点は体の中心）と、体の高さの半分 (m)。
    const HIKARI::GameProjectTransform& body;
    float halfHeight = 2.0f;
    // 揺らぎ・息の時計 (秒) と、垂れた部品の振れ（度。StepSway の値。分身は 0）。
    float clock = 0.0f;
    HIKARI::GameProjectFloat3 sway{};
    // 胴の捻り・兜の向き（体の向きからの rad）。
    float twist = 0.0f;
    float headYaw = 0.0f;
    // フェーズ 2（灼けた刀身・火の目）か、目の光を出すか。
    bool phaseTwo = false;
    bool eyesLit = true;
    // 刀の模型と手の模型の拡大。
    float swordScale = 1.0f;
    float handScale = 1.0f;
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
    // 本体の入力（鎧を置くのに使う）。
    static BossBodyRigInput InputOf(const BossComponent& boss, const HIKARI::GameProjectTransform& body);
    // 待機の構え（下段）の全身。動きの状態を持たない霞分身が使う（刀・手は本体の待機と同じ置き方）。
    static BossBodyPose SolveIdle(const BossBodyRigInput& input);
};

} // namespace Project_TD2_2
