#pragma once

#include "HIKARI/Project/GameComponent.h"

namespace Project_TD2_2 {

// 霞歩（横っ飛び）とかわしの、人が調整する値。ボスの部品（BossComponent）が持ち、項目は「霞歩」の節に出る。
// 向き・F・後隙（着地の止まり）は技の表の行（型 kasumiStep）。距離と高さは表の列が空欄の時だけここを使う。
// 体の大きさに付く値（体が変わったら一緒に変える）：distance・height・fireClearance・playerGap・edgeMargin。
// いまの値は身長 6.75 m・半径 1.35 m の体のもの（距離 4.5 m は半径の約 3.3 倍）。
struct BossKasumiStepSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 跳ぶ距離 (m) と、跳ぶ弧の高さ (m)。
    float distance = 4.5f;
    float height = 0.6f;
    // 斜め前・斜め後ろの角度（真横から前・後ろへ、度）。
    float diagonalDegrees = 45.0f;
    // 真横（左右・弾の線から離れる側）へ跳ぶ時に、プレイヤーを中心に回る向きへ曲げる割合（1 で跳ぶ前と同じ距離に降りる。
    // 0 でまっすぐ横へ：跳ぶたびにプレイヤーから遠ざかる）。
    float orbit = 1.0f;
    // 跳び出しの鋭さ（1 で一定の速さ。大きいほど速く跳び出して、着地の手前でゆっくりになる）。
    float easePower = 1.6f;
    // 跳ぶ間にプレイヤーへ向き直る速さ (度/秒)。
    float turnRate = 360.0f;
    // 行き先の決め方：戦場の縁（Boss.arenaRadius）からこれだけ内側までに降りる (m)。プレイヤーの中心からこれより近くへは
    // 降りない (m)。行き先とその周りのこの半径 (m)・道の真ん中が燃えていたら、その向きへは跳ばない。
    float edgeMargin = 1.0f;
    float playerGap = 3.0f;
    float fireClearance = 1.35f;
    // 前隙で屈む量・着地で沈む量（屈みの割合 0〜1。Boss.crouchSquash で縦に縮む）。
    float takeoffCrouch = 0.4f;
    float landCrouch = 0.6f;
    // 跳ぶ向きへ体（鎧・刀・手）を傾ける角度（跳ぶ途中でいちばん傾く、度）。
    float leanDegrees = 10.0f;
    // 弾をかわす霞歩の、跳び始めから当たらない F（0 は体が動いた分だけでよける）。
    int guardFrames = 0;
    // 着地で足元の霧を押す大きさ（当たった所の爆発の押しに対する倍。0 で押さない）。
    float landPush = 0.8f;
    // 続けてかわせる回数・続けたとみなす間 (秒。前のかわしからこれより空けば数え直す)・使い切った後にかわさない秒。
    int dodgeMax = 2;
    float dodgeChainSeconds = 8.0f;
    float dodgeLockSeconds = 5.0f;
    // かわさない間は、様子見の霞歩も出さない（息が切れている）。
    bool restWhileSpent = true;
    // かわさない間の胴の煙の量（ふだんに対する倍。薄く静かになって、見て分かる）。
    float spentSmoke = 0.35f;
};

// 霞歩のいまの様子。規則（BossKasumiStep）が固定 tick ごとに書く。保存しない。
struct BossKasumiStepState {
    // 跳ぶ所と行き先（地面の高さ）、跳ぶ高さ、選んだ向き（ログ用）、弾をかわす霞歩か、着地したか。技を始めた tick に決める。
    HIKARI::GameProjectFloat3 from{};
    HIKARI::GameProjectFloat3 to{};
    float height = 0.0f;
    int direction = 0;
    bool dodge = false;
    bool landed = false;
    // 続けてかわした回数、最後にかわした時刻、かわさない間の終わり（どれも boss.clockSeconds の秒）。
    int dodges = 0;
    float lastDodgeSeconds = -1000.0f;
    float lockedUntilSeconds = -1000.0f;
};

} // namespace Project_TD2_2
