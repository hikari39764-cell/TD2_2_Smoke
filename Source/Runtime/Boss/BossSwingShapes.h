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

    // 横なぎ（右から左へ。返しは左右に映す）：構えで胴を逆へ捻り切り、刀を真後ろまで引く（溜め）。腰を落とし、両手を
    // 体の前へ出して下へ約 13 度で薙ぎ、振り込むほど沈んで前へ傾く（爆発）。左の後ろまで振り切って止まる（残心）。プレイヤーが立つ
    // 2〜3.5 m で刀身が地上 1.6〜1.25 m を通る。振り終わりは刀を左へ向けて収める（もっと先まで回すと、胴を捻り切った
    // 左肩から左手が遠くなる）。
    BossSwingShape horizontal{{0.0f, 2.0f, 0.1f},
                              {0.0f, 1.0f, 0.0f},
                              {0.0f, 1.72f, 1.35f},
                              {0.25f, -0.22f, 0.94f},
                              -88.0f,
                              50.0f,
                              -65.0f,
                              40.0f,
                              -80.0f,
                              60.0f,
                              0.5f,
                              8.0f,
                              0.0f,
                              0.55f,
                              14.0f};
    // 袈裟（左上から右下へ。逆袈裟は左右に映す）：両手を左肩の上へ上げ（刀は左後ろへ斜めに立つ）、約 60 度の斜めの面で
    // 斬り下ろして右の腰の前で収める。正面 2.5〜3.5 m に立つプレイヤーの胸の高さを刀身が通る（それより近いと頭上を通り、
    // 飛ぶ斬撃で当たる）。剣先は地面の上で止まる。
    BossSwingShape kesa{{0.0f, 3.0f, 0.3f},
                        {0.874f, -0.486f, 0.0f},
                        {0.0f, 2.6f, 1.4f},
                        {0.2f, -0.3f, 0.9f},
                        -90.0f,
                        15.0f,
                        -60.0f,
                        5.0f,
                        60.0f,
                        -45.0f,
                        0.25f,
                        5.0f,
                        0.45f,
                        0.35f,
                        8.0f};
    // 縦（上段から真っ向へ）：両手を頭上へ上げ（刀は後ろへ立つ）、肩を支点に胸の前まで斬り下ろす。炎走りの斬り上げは
    // この型を逆に回す。
    BossSwingShape vertical{{0.0f, 3.0f, 0.3f},
                            {1.0f, 0.0f, 0.0f},
                            {-0.05f, 2.65f, 1.45f},
                            {0.0f, -0.2f, 0.98f},
                            -100.0f,
                            15.0f,
                            -60.0f,
                            5.0f,
                            0.0f,
                            0.0f,
                            0.25f,
                            2.0f,
                            0.55f,
                            0.3f,
                            8.0f};
    // 炎走りの斬り上げ：縦の型のこの角度（度）からこの角度まで逆に回す（胸の前の低い構え → 前の高い所）。
    float riseFromDegrees = 15.0f;
    float riseToDegrees = -45.0f;
    // 霞の大剣の間：薙ぐ・叩きつける間に腰を落とす量 (m)・前傾（度）、大剣の向き 1 度あたりの胴の捻り（度）と上限（度）。
    // 大剣は左右へ大きく回るので、胴も大きく捻る（小さいと、回り切った大剣の柄から反対の肩が遠くなる）。
    float greatswordCrouch = 0.55f;
    float greatswordLean = 8.0f;
    float greatswordTwistShare = 0.75f;
    float greatswordTwistMaxDegrees = 75.0f;
    // 両手の斬りの間に、動く速さで体を前・横へ傾ける上限（度。踏み込みながら振ると、傾いた胴と兜が手の通り道へ入る）。
    float speedLeanMaxDegrees = 6.0f;
};

} // namespace Project_TD2_2
