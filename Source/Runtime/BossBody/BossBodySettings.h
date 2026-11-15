#pragma once

#include <string>

#include "HIKARI/Project/GameComponent.h"

namespace Project_TD2_2 {

// ボスの外形（模型の部品）の、人が調整する値。ボスの部品（BossComponent）が持ち、項目は「外形」の節に出る。
// 模型は 1 つの資産（glTF）で、部品がノードに分かれて入っている。コードがノードごとに置き場所を決めて出す。
struct BossBodySettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 外形の模型の資産（GUID）。空なら何も出さない。
    std::string model{"b1a52cf03fe7473cbbe74c310e9c5c29"};
    bool visible = true;
    // 刀の模型の、握りの中央（ノードの原点）から剣先までの長さ。剣の長さ（Boss.swordLength）に合うように拡大する。
    // 模型を作るスクリプトが出す値（KATANA_LENGTH）に合わせる。
    float swordModelLength = 2.305f;
    // 握りの中央を、剣の付け根（Boss.swordShoulder）から剣の向きへどれだけ先に置くか。
    float swordGripAlong = 0.07f;
    // 手の大きさ（刀の拡大に掛ける。1 で刀と同じ拡大）。
    float handScale = 1.0f;
    // 握りの右手の模型の形（刀の座標、模型の実寸）：手首の位置・肘の向き・手の甲の向き。籠手の筒を手首に付けるのに使う。
    // 模型を作るスクリプトが出す値（GRIP_WRIST・GRIP_ELBOW・GRIP_BACK）に合わせる。
    HIKARI::GameProjectFloat3 gripWrist{-0.0249f, 0.2105f, -0.0907f};
    HIKARI::GameProjectFloat3 gripElbow{0.3367f, 0.8333f, -0.4384f};
    HIKARI::GameProjectFloat3 gripBack{-0.9272f, 0.3746f, 0.0f};
    // 肩の位置（体の足元から、局所 m。右肩。左肩は左右を反転）。籠手の筒は、手首からこの肩の方へ伸びる
    // （腕は無いが、筒が肩を向くと腕があるように読める）。
    HIKARI::GameProjectFloat3 shoulder{-0.80f, 3.05f, 0.0f};
    // 筒の向きに、握りの手の模型の肘の向きを混ぜる割合（0 で肩をまっすぐ向く、1 で手の模型どおり）。
    float cuffGripBlend = 0.25f;
    // 左手（刀を持たない手）の、何もしていない時の置き場所：手首の位置（体の足元から、局所 m）と、
    // 指先・手の甲の向き（体の向きに対する局所の向き。長さは問わない）。
    HIKARI::GameProjectFloat3 leftRestWrist{1.0f, 1.80f, 0.35f};
    HIKARI::GameProjectFloat3 leftRestFingers{0.10f, -0.85f, 0.50f};
    HIKARI::GameProjectFloat3 leftRestBack{0.85f, 0.25f, 0.45f};
};

// 外形の実行時の状態（保存しない）。
struct BossBodyState {
    // 模型を出せなかった知らせは、1 回だけ記録に出す（毎フレーム出さない）。
    bool submitWarned = false;
};

} // namespace Project_TD2_2
