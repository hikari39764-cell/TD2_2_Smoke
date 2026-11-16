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
    // 振り上げの道（0〜1）：0 より大きいと、構えへ上げる時に、型の振りのこの進みの構えを通ってから型を逆にたどる
    // （縦の振りかぶりのように、刀の向きが今の構えから大きく変わる時に、刀が体の中を通らない）。0 は直に混ぜる。
    float raiseVia = 0.0f;
    // 振り終わりの腰の落とし (m)・前傾（度）。構え（crouch・lean）から振りの進みにつれて寄せる（振り込むほど重心を前へ
    // 沈める＝気勢）。
    float crouchTo = 0.0f;
    float leanTo = 0.0f;
};

// 斬りの型の一式（規則の剣の線＝当たりと、見える刀の両方がこれで動く）。
struct BossSwingSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 横なぎ（右から左へ。返しは左右に映す）：構えで胴を逆へ 62 度捻り（80 度では構えの間に背中をプレイヤーへ向けた。
    // c-4a8ea1）、刀を真後ろまで引く（溜め）。腰を落とし、両手を
    // 体の前へ出して下へ約 13 度で薙ぎ、振り込むほど沈んで前へ傾く（爆発）。左の後ろまで振り切って止まる（残心）。プレイヤーが立つ
    // 2〜3.5 m で刀身が地上 1.6〜1.25 m を通る。振り終わりは刀を左へ向けて収める（もっと先まで回すと、胴を捻り切った
    // 左肩から左手が遠くなる）。体を 1.25 倍にした時（c-f91bd1）、支点と握りの前後は 1.25 倍にしたが、握りの高さは据え置いた
    // （上げると刀がプレイヤーの頭上を抜け、刀そのものが当たらない。大きな体が腰を落として低く薙ぐ形）。
    BossSwingShape horizontal{{0.0f, 2.5f, 0.125f},
                              {0.0f, 1.0f, 0.0f},
                              {0.0f, 1.72f, 1.6875f},
                              {0.25f, -0.22f, 0.94f},
                              -88.0f,
                              50.0f,
                              -65.0f,
                              40.0f,
                              -62.0f,
                              60.0f,
                              0.625f,
                              8.0f,
                              0.0f,
                              0.6875f,
                              14.0f};
    // 袈裟（左上から右下へ。逆袈裟は左右に映す）：両手を左肩の上へ上げ（刀は左後ろへ斜めに立つ）、約 60 度の斜めの面で
    // 斬り下ろし、手を右の腰の前まで振り抜き、手首で刀を起こして止める（手が先、刀が後から寝る）。c-4a8ea1 で to 15→32 度・
    // wristTo 5→-20 度、握り (0, 3.25, 1.75)→(0, 3.4, 2.0)：正面で止めると刀がプレイヤーを指したまま固まり、手首を前へ
    // 返すと刀が真下を向いて剣先が地面に刺さった（見せる刀は地面で止まるので、振り終わりで刀が曲がって見えた）。刀の下向きは
    // 振り終わりで 38 度まで、規則の剣先は地上 0.2 m より上。真ん中の刀の向きを右（-X）へ振り、握りを左（+X）へ 0.45 m ずらした
    // （刀が右の下へ斜めに抜け、振り終わりの刀がプレイヤーを指さない。正面のプレイヤーには刀の 2 F 後の飛ぶ斬撃が当たる）。
    // 構えの捻りは 42 度（60 度では構えの間に背中をプレイヤーへ向けた）。
    // 体を 1.25 倍にした時（c-f91bd1）に支点・握りも 1.25 倍にした：
    // 当たりの窓の間の刀身はプレイヤーの頭の上を通り、正面に立つプレイヤーには飛ぶ斬撃が当たる（刀の 3 F 後）。
    BossSwingShape kesa{{0.0f, 3.75f, 0.375f},
                        {0.874f, -0.486f, 0.0f},
                        {0.45f, 3.4f, 2.0f},
                        {-0.3f, -0.3f, 0.9f},
                        -90.0f,
                        32.0f,
                        -60.0f,
                        -25.0f,
                        42.0f,
                        -55.0f,
                        0.3125f,
                        5.0f,
                        0.45f,
                        0.6f,
                        8.0f};
    // 縦（上段から真っ向へ）：両手を頭上へ上げ（刀は後ろへ立つ）、肩を支点に前の低い所まで斬り下ろし、手首で刀を起こして止める
    // （c-4a8ea1 で to 15→25 度・wristTo 5→-15 度、握りを前へ 2.15 m：振り終わりで手が胴に入らず、刀の下向きは 33 度まで、
    // 剣先は地上 0.3 m より上）。炎走りの斬り上げはこの型を逆に回す。
    BossSwingShape vertical{{0.0f, 3.75f, 0.375f},
                            {1.0f, 0.0f, 0.0f},
                            {-0.0625f, 3.3125f, 2.15f},
                            {0.0f, -0.2f, 0.98f},
                            -100.0f,
                            25.0f,
                            -60.0f,
                            -20.0f,
                            0.0f,
                            0.0f,
                            0.3125f,
                            2.0f,
                            0.55f,
                            0.55f,
                            8.0f};
    // 炎走りの斬り上げ：縦の型のこの角度（度）からこの角度まで逆に回す（胸の前の低い構え → 前の高い所）。
    float riseFromDegrees = 15.0f;
    float riseToDegrees = -45.0f;
    // 霞の大剣の間：薙ぐ・叩きつける間に腰を落とす量 (m)・前傾（度）、大剣の向き 1 度あたりの胴の捻り（度）と上限（度）。
    // 大剣は左右へ大きく回るので、胴も大きく捻る（小さいと、回り切った大剣の柄から反対の肩が遠くなる）。
    float greatswordCrouch = 0.6875f;
    float greatswordLean = 8.0f;
    float greatswordTwistShare = 0.75f;
    float greatswordTwistMaxDegrees = 75.0f;
    // 両手の斬りの間に、動く速さで体を前・横へ傾ける上限（度。踏み込みながら振ると、傾いた胴と兜が手の通り道へ入る）。
    float speedLeanMaxDegrees = 6.0f;
};

} // namespace Project_TD2_2
