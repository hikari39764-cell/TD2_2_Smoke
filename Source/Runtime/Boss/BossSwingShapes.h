#pragma once

#include <string_view>

#include "HIKARI/Project/PropertyFields.h"
#include "HIKARI/Project/ValueTypes.h"

namespace Project_TD2_2 {

// 両手の振りの型。支点 pivot を通る軸 axis のまわりに、握りと刀を一緒に回す（刀は支点から外を向くので、振っても体に
// 入りにくい）。0 度の時（振りの真ん中）の握り grip と刀の向き direction を書き、構え fromDegrees から振り終わり
// toDegrees まで回す。刀は手首の分だけ手より遅れて・先へ回る（構えで wristFromDegrees、振り終わりで wristToDegrees）。
// 体は振りに合わせて捻り（twistFrom → twistTo）、腰を落とし（crouch）、前へ傾く（lean）。位置は体の足元から、局所 m
// （+X が左、+Z が前）。逆の向きの振り（返し）は、型を左右に映して使う。
struct BossSwingShape {
    void Describe(HIKARI::PropertyFields& fields, std::string_view key, std::string_view label);

    HIKARI::GameProjectFloat3 pivot{};
    HIKARI::GameProjectFloat3 axis{0.0f, 1.0f, 0.0f};
    HIKARI::GameProjectFloat3 grip{};
    HIKARI::GameProjectFloat3 direction{0.0f, 0.0f, 1.0f};
    float fromDegrees = -60.0f;
    float toDegrees = 60.0f;
    float wristFromDegrees = 0.0f;
    float wristToDegrees = 0.0f;
    float twistFromDegrees = 0.0f;
    float twistToDegrees = 0.0f;
    float crouch = 0.0f;
    float lean = 0.0f;
};

// 斬りの型の一式（規則の剣の線＝当たりと、見える刀の両方がこれで動く）。
struct BossSwingSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 横なぎ（右から左へ。返しは左右に映す）：腰を落とし、両手を体の前へ出して、下へ約 13 度で薙ぐ。プレイヤーが立つ
    // 2〜3.5 m で刀身が地上 1.6〜1.25 m を通る。振り終わりは刀を左へ向けて収める（もっと先まで回すと、胴を捻り切った
    // 左肩から左手が遠くなる）。
    BossSwingShape horizontal{{0.0f, 2.0f, 0.1f},
                              {0.0f, 1.0f, 0.0f},
                              {0.0f, 1.72f, 1.35f},
                              {0.25f, -0.22f, 0.94f},
                              -70.0f,
                              40.0f,
                              -55.0f,
                              25.0f,
                              -60.0f,
                              40.0f,
                              0.35f,
                              6.0f};
};

} // namespace Project_TD2_2
