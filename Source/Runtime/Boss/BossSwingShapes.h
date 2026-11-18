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
    // 戻り（c-999264）：振り終わりの行き過ぎから下ろした構えへ、止まらずに戻る 1 本の道。道を中段（BossSwingSettings の
    // return…。刀を体の前へ戻し、剣先をプレイヤーへ向ける）へ寄せる強さ（0〜1。0 は振り終わりからそのまま構えへ）。
    // 刃は斬る向きのまま（棟が先に戻る）。
    float returnLift = 1.0f;
    // 戻りの速さが最も大きくなる所（戻りの長さに対する割合）。小さいほど振りの勢いのまま素早く戻り始め、長く減速して
    // 構えに収まる。
    float returnPeakShare = 0.3f;
    // 刃の面：刀身の軸のまわりに、斬る向き（振りの面の中で動く向き）を回す角度（度。左右に映した型では逆へ回す）。
    float edgeRollDegrees = 0.0f;
};

// 斬りの型の一式（規則の剣の線＝当たりと、見える刀の両方がこれで動く）。
struct BossSwingSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 横なぎ（右から左へ。返しは左右に映す）：構えで胴を逆へ 62 度捻り（80 度では構えの間に背中をプレイヤーへ向けた。
    // c-4a8ea1）、刀を真後ろまで引く（溜め）。腰を落とし、両手を
    // 体の前へ出して下へ約 13 度で薙ぎ、振り込むほど沈んで前へ傾く（爆発）。左の後ろまで振り切って止まる（残心）。プレイヤーが立つ
    // 2〜3.5 m で刀身が地上 1.6〜1.25 m を通る。振り終わりは刀を左へ向けて収める（もっと先まで回すと、胴を捻り切った
    // 左肩から左手が遠くなる）。体を大きくした時（c-f91bd1 で 1.25 倍、c-a30eaf・c-e97b18 でさらに 1.35 倍）、支点と握りの前後は
    // 倍率どおりにしたが、握りの高さは 1.85 m までにとどめ、刀の下向きを約 17 度へ深くし、腰を深く落とす（1.2〜1.35 m）（倍率どおり上げると刀が
    // 1.7 m のプレイヤーの頭上を抜け、刀そのものが当たらない。大きな体が腰を深く落として低く薙ぐ形）。
    BossSwingShape horizontal{{0.0f, 3.375f, 0.16875f},
                              {0.0f, 1.0f, 0.0f},
                              {0.0f, 1.85f, 2.278125f},
                              {0.25f, -0.29f, 0.92f},
                              -88.0f,
                              50.0f,
                              -65.0f,
                              40.0f,
                              -62.0f,
                              60.0f,
                              1.2f,
                              11.0f,
                              0.0f,
                              1.35f,
                              17.0f,
                              1.0f,
                              0.3f,
                              0.0f};
    // 袈裟（左上から右下へ。逆袈裟は左右に映す）：両手を左肩の上へ上げ（刀は左後ろへ斜めに立つ）、約 60 度の斜めの面で
    // 斬り下ろし、手を右の腰の前まで振り抜き、手首で刀を起こして止める（手が先、刀が後から寝る）。c-4a8ea1 で to 15→32 度・
    // wristTo 5→-20 度、握り (0, 3.25, 1.75)→(0, 3.4, 2.0)：正面で止めると刀がプレイヤーを指したまま固まり、手首を前へ
    // 返すと刀が真下を向いて剣先が地面に刺さった（見せる刀は地面で止まるので、振り終わりで刀が曲がって見えた）。刀の下向きは
    // 振り終わりで 38 度まで、規則の剣先は地上 0.2 m より上。真ん中の刀の向きを右（-X）へ振り、握りを左（+X）へ 0.45 m ずらした
    // （刀が右の下へ斜めに抜け、振り終わりの刀がプレイヤーを指さない。正面のプレイヤーには刀の 2 F 後の飛ぶ斬撃が当たる）。
    // 構えの捻りは 42 度（60 度では構えの間に背中をプレイヤーへ向けた）。
    // 体を大きくした時（c-f91bd1 で 1.25 倍、c-a30eaf・c-e97b18 でさらに 1.35 倍）に支点・握り・腰の落としも倍率どおりにした：
    // 当たりの窓の間の刀身はプレイヤーの頭の上を通り、正面に立つプレイヤーには飛ぶ斬撃が当たる（刀の 3 F 後）。
    BossSwingShape kesa{{0.0f, 5.0625f, 0.50625f},
                        {0.874f, -0.486f, 0.0f},
                        {0.6075f, 4.59f, 2.7f},
                        {-0.3f, -0.3f, 0.9f},
                        -90.0f,
                        32.0f,
                        -60.0f,
                        -25.0f,
                        42.0f,
                        -55.0f,
                        0.421875f,
                        5.0f,
                        0.45f,
                        0.81f,
                        8.0f,
                        1.0f,
                        0.3f,
                        0.0f};
    // 縦（上段から真っ向へ）：両手を頭上へ上げ（刀は後ろへ立つ）、肩を支点に前の低い所まで斬り下ろし、手首で刀を起こして止める
    // （c-4a8ea1 で to 15→25 度・wristTo 5→-15 度、握りを前へ 2.15 m：振り終わりで手が胴に入らず、刀の下向きは 33 度まで、
    // 剣先は地上 0.3 m より上）。炎走りの斬り上げはこの型を逆に回す。c-a30eaf・c-e97b18 で支点・握り・腰の落としを 1.35 倍にした。
    BossSwingShape vertical{{0.0f, 5.0625f, 0.50625f},
                            {1.0f, 0.0f, 0.0f},
                            {-0.084375f, 4.471875f, 2.9025f},
                            {0.0f, -0.2f, 0.98f},
                            -100.0f,
                            25.0f,
                            -60.0f,
                            -20.0f,
                            0.0f,
                            0.0f,
                            0.421875f,
                            2.0f,
                            0.55f,
                            0.7425f,
                            8.0f,
                            1.0f,
                            0.3f,
                            0.0f};
    // 大上段（c-b1824c。上段溜めだけの型）：手を頭のずっと後ろまで振りかぶり（刀身は頭の後ろで水平に真後ろへ）、胴を左へ
    // 捻って上体を反らせた構えから、肩を支点に頭上を通って前の地面まで大きく斬り下ろし、振り終わりで深く沈んで前へ
    // 折れる（沈み 0.95 m・前傾 10 度。1.45 m・18 度では胴が両手の上へ落ちて拳が胴に 0.8 m 入った）。縦振り・遅い縦斬り
    // （縦の型）より振りかぶりが深く、体の反りと沈みが大きい。振り終わりの手首 0 度で刃は前下 40 度ほどへ向く（-22 度では
    // 水平から 17 度下までしか下りず、着地の距離のプレイヤーの頭の上を通った）。
    BossSwingShape overhead{{0.0f, 5.0625f, 0.50625f},
                            {1.0f, 0.0f, 0.0f},
                            {-0.084375f, 4.471875f, 2.9025f},
                            {0.0f, -0.2f, 0.98f},
                            -130.0f,
                            24.0f,
                            -40.0f,
                            0.0f,
                            22.0f,
                            -8.0f,
                            0.15f,
                            -10.0f,
                            0.55f,
                            0.95f,
                            10.0f,
                            1.0f,
                            0.35f,
                            0.0f};
    // 散歩の構え（c-44fdc5）：散歩の姿勢で横へ歩く間、両手で刀を体の前に横たえる（剣先はボスの右へ、刃は前）。
    // 横歩きの始めの strollGuardFrames F で、いまの構えから移る。
    HIKARI::GameProjectFloat3 strollGuardGrip{0.25f, 3.1f, 1.7f};
    HIKARI::GameProjectFloat3 strollGuardDirection{-0.93f, 0.08f, 0.36f};
    float strollGuardCrouch = 0.3f;
    float strollGuardLean = 4.0f;
    float strollGuardTwist = 12.0f;
    int strollGuardFrames = 14;
    // 退き溜めの構え（c-95444e）：後ろへ跳ぶ間に刀を後ろへ引き、着地してから長く溜める間、両手の握りを右肩の後ろへ引いて
    // 刀を右後ろの高い所へ大きく引き絞り（刃は上）、胴を右へ捻り込んで深く沈む（左手はプレイヤーを狙う）。溜めるにつれて
    // さらに retreatChargeCoilDegrees 度だけ後ろへ倒し、沈みを retreatChargeCrouchTo まで深める。溜めの終わりの
    // retreatHeaveFrames F で、引き絞った刀を頭上の構え（縦の型）へ担ぎ上げ、止まらずに縦に斬り下ろす。上段の構え
    // （頭上に振りかぶる）と違い、手が肩の高さで後ろにあり、体が横を向くので、構えだけで見分けられる。刀は右の外へ斜めに
    // 立てる（真後ろへ向けると、正面のゲームのカメラから体に隠れて見えなかった）。
    HIKARI::GameProjectFloat3 retreatChargeGrip{-1.9f, 5.1f, -0.8f};
    // 退く間に刀を引く道の真ん中の握り（下ろした構えから右の外を回して引く。直に混ぜると右の大袖を刀が通った）。
    HIKARI::GameProjectFloat3 retreatDrawVia{-2.8f, 4.7f, 0.0f};
    HIKARI::GameProjectFloat3 retreatChargeDirection{-0.82f, 0.45f, -0.35f};
    float retreatChargeCrouch = 0.6f;
    float retreatChargeCrouchTo = 1.0f;
    float retreatChargeLean = -4.0f;
    float retreatChargeTwist = -40.0f;
    float retreatChargeCoilDegrees = 12.0f;
    int retreatHeaveFrames = 12;
    // 戻りの中段（c-999264）：振り終わりから構えへ戻る道が寄る所。両手で握り（体の足元から、局所 m）、剣先をプレイヤーの
    // 胸へ向け（局所の向き）、腰を少し落として前へ傾く。「戻り始めた＝この連段は終わり」と読める形。
    HIKARI::GameProjectFloat3 returnGrip{-0.3f, 3.5f, 2.0f};
    HIKARI::GameProjectFloat3 returnDirection{0.08f, -0.32f, 0.94f};
    float returnCrouch = 0.35f;
    float returnLean = 4.0f;
    // 突進（c-2dd9a2。退き払いは左右に映す）：刀を後ろ下へ引きずり（手は右の腰の低い所、刀身は手よりずっと遅れて右後ろへ。
    // 真後ろだと正面のプレイヤーから体に隠れて見えないので、20 度ほど外へ出す。
    // 剣先は地面すれすれ）、体を深く沈めて胴を右へ捻った構えから、軸を後ろへ少し倒した面で、腰・胴・腕・刀の順に体ごと回し、
    // 低い所から前を通って左の高い所へ払い上げる（刀が手を追い越す）。真ん中の握り・向きは横なぎに近く、プレイヤーの胴を通る。
    // 構えへは振りの真ん中（体の前）を通って上げる（raiseVia 0.5。退き払いで右から左の腰へ収める時、刀が背中を通らない）。
    // 拳が胴に入らないよう、握りを横なぎより前へ（2.5 m）、沈みと前傾は振り込むほど浅くする。
    BossSwingShape dash{{0.0f, 3.375f, 0.16875f},
                        {0.0f, 0.985f, -0.174f},
                        {0.0f, 1.85f, 2.5f},
                        {0.25f, -0.22f, 0.94f},
                        -100.0f,
                        60.0f,
                        -60.0f,
                        45.0f,
                        -66.0f,
                        64.0f,
                        1.45f,
                        12.0f,
                        0.5f,
                        1.1f,
                        7.0f,
                        1.0f,
                        0.3f,
                        0.0f};
    // 炎走りの斬り上げ：縦の型のこの角度（度）からこの角度まで逆に回す（胸の前の低い構え → 前の高い所）。
    float riseFromDegrees = 15.0f;
    float riseToDegrees = -45.0f;
    // 霞の大剣の間：薙ぐ・叩きつける間に腰を落とす量 (m)・前傾（度）、大剣の向き 1 度あたりの胴の捻り（度）と上限（度）。
    // 大剣は左右へ大きく回るので、胴も大きく捻る（小さいと、回り切った大剣の柄から反対の肩が遠くなる）。
    float greatswordCrouch = 0.928125f;
    float greatswordLean = 8.0f;
    float greatswordTwistShare = 0.75f;
    float greatswordTwistMaxDegrees = 75.0f;
    // 両手の斬りの間に、動く速さで体を前・横へ傾ける上限（度。踏み込みながら振ると、傾いた胴と兜が手の通り道へ入る）。
    float speedLeanMaxDegrees = 6.0f;
};

} // namespace Project_TD2_2
