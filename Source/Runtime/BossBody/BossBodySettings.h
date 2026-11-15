#pragma once

#include <string>

#include "HIKARI/Project/GameComponent.h"

namespace Project_TD2_2 {

// ボスの外形（模型の部品）の、人が調整する値。ボスの部品（BossComponent）が持ち、項目は「外形」の節に出る。
// 模型は 1 つの資産（glb）で、部品がノードに分かれて入っている。コードがノードごとに置き場所を決めて出す。
struct BossBodySettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 外形の模型の資産（GUID）。空なら何も出さない。
    std::string model{"b1a52cf03fe7473cbbe74c310e9c5c29"};
    bool visible = true;
    // 刀の模型の、握りの中央（ノードの原点）から剣先までの長さ。剣の長さ（Boss.swordLength）に合うように拡大する。
    float swordModelLength = 2.45f;
    // 握りの中央を、剣の付け根（Boss.swordShoulder）から剣の向きへどれだけ先に置くか。
    float swordGripAlong = 0.07f;
    // 手の大きさ（刀の拡大に掛ける。1 で刀と同じ拡大）。
    float handScale = 1.0f;
};

// 外形の実行時の状態（保存しない）。
struct BossBodyState {
    // 模型を出せなかった知らせは、1 回だけ記録に出す（毎フレーム出さない）。
    bool submitWarned = false;
};

} // namespace Project_TD2_2
