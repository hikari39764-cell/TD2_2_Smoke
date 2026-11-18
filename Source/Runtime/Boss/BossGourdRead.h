#pragma once

#include <array>
#include <cstddef>

#include "HIKARI/Project/PropertyFields.h"
#include "BossStance.h"

namespace Project_TD2_2 {

class BossComponent;
struct BossFrame;
struct BossMove;
struct BossWave;

// ヒョウタンの読み（c-14392d）：弾と守りが同じヒョウタンなので、ボスはプレイヤーの残りを見て技の選び方を変える。
// 満杯（もう吸えない）なら遠距離の剣気で押し、空に近い（1 発ぶんに足りない）なら遅くて吸いやすい剣気を出し
// （補給できずに詰ませない。接近は上げない）、続けて剣気を吸われたら決めた秒数だけ実体の刀で来る。
// 吸い込みを構え続けているのを見て選んだ剣気の技は、構えを一拍止めてから出す。同じ側へ転がる癖があれば、横の剣気を
// その側へ寄せる。どれも入・切と強さはボスの項目（調整ガイド g-56cc52）、吸いやすい剣気の印は姿勢の表の列。
enum class BossGourdState { Off, Mid, Full, Low };

struct BossGourdReadSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 全体の入・切。
    bool enabled = true;
    // 満杯（もう吸えない）：遠距離・接近の重みに掛ける倍率。
    bool fullEnabled = true;
    float fullFarScale = 2.5f;
    float fullNearScale = 1.0f;
    // 空に近い：煙が弾 lowShots 発ぶんより少ない。表で「吸いやすい」の印の行・ほかの遠距離・接近に掛ける倍率。
    // 接近は上げない（1 以下）。lowGuaranteeSeconds 秒、吸いやすい剣気を出さずに空に近いままなら、次の攻勢の技は
    // 出せる吸いやすい剣気だけから選ぶ（0 で使わない）。
    bool lowEnabled = true;
    float lowShots = 1.0f;
    float lowEasyScale = 3.0f;
    float lowFarScale = 0.6f;
    float lowNearScale = 0.7f;
    float lowGuaranteeSeconds = 12.0f;
    // 続けて吸われた：chainWindowSeconds 秒のうちに剣気を chainCount 本吸われたら、chainSeconds 秒、遠距離・接近の
    // 重みにこの倍率を掛ける（空に近い間は掛けない）。
    bool chainEnabled = true;
    int chainCount = 2;
    float chainWindowSeconds = 6.0f;
    float chainSeconds = 8.0f;
    float chainFarScale = 0.3f;
    float chainNearScale = 2.0f;
    // 続けて吸われた間は、霞の剣気（吸える飛ぶ斬撃）を出す技を攻勢の技選びの候補から外す（実体の刀で来る）。見分けは
    // 技の表：刀を振る型で、当て方が「実」でなく、どれかの一撃の斬撃速度が 0 より大きい技（表の種類が接近でも外す）。
    bool chainNoHaze = true;
    // 一拍止め：吸い込みを holdAbsorbSeconds 秒以上構え続けている時に攻勢が遠距離の技を選んだら、holdChance % で
    // 構えを holdMinFrames〜holdMaxFrames F 止めてから出す（ローリングの無敵 15 F と重ならない長さ）。一度出したら
    // holdCooldownSeconds 秒は混ぜない。
    bool holdEnabled = true;
    float holdAbsorbSeconds = 0.4f;
    float holdChance = 50.0f;
    float holdCooldownSeconds = 5.0f;
    int holdMinFrames = 18;
    int holdMaxFrames = 24;
    // 横の剣気の寄せ：最近 leanRolls 回のローリングのうち（leanMinRolls 回以上数えてから）、ボスから見て左か右が
    // leanAtLeastPercent % 以上なら、横の剣気の向きをその側へ leanMeters m（プレイヤーの所で。leanMaxDegrees 度まで）寄せる。
    bool leanEnabled = true;
    int leanRolls = 6;
    int leanMinRolls = 3;
    float leanAtLeastPercent = 60.0f;
    float leanMeters = 2.0f;
    float leanMaxDegrees = 15.0f;
};

// 読みの覚え（BossComponent が持つ）。時刻は boss.clockSeconds の秒。
struct BossGourdReadMemory {
    // 空に近くなった時刻（空に近くなければ負）と、吸いやすい剣気を最後に選んだ時刻。
    float lowSince = -1.0f;
    float lastEasySeconds = -1.0e9f;
    // 剣気を吸われた時刻（輪で覚える）と、接近へ寄せる終わりの時刻。
    std::array<float, 8> catches{};
    size_t catchCount = 0;
    size_t catchNext = 0;
    float chainUntil = -1.0f;
    // 一拍止め：次に混ぜられる時刻と、次に始まる技（表の行）に足す F。
    float holdReadyAt = 0.0f;
    int pendingHold = 0;
    int pendingRow = -1;
};

// いまの読み（攻勢の技選びのたびに読む）。
struct BossGourdReading {
    BossGourdState state = BossGourdState::Off;
    bool chain = false;
    bool guarantee = false;
};

class BossGourdRead final {
public:
    // 毎 tick（観察の後）：空に近くなった時刻を覚える。
    static void Observe(BossComponent& boss);
    static BossGourdReading Read(const BossComponent& boss);
    // 姿勢の表の 1 行（歩きでない）の重みに掛ける倍率。
    static float Scale(const BossComponent& boss, const BossGourdReading& reading, const BossStanceRow& row);
    // 技が霞の剣気（吸える飛ぶ斬撃）を出すか：刀を振る型・当て方が「実」でない・どれかの一撃の斬撃速度が 0 より大きい。
    static bool ThrowsHaze(const BossMove& move);
    // 姿勢の表の 1 行を、いまの読みで候補から外すか（続けて吸われた間の霞の剣気。gourdChainNoHaze）。
    static bool Excluded(const BossComponent& boss, const BossGourdReading& reading, const BossStanceRow& row);
    // 攻勢の技を選んだ：吸いやすい剣気の時刻を覚え、一拍止めを振る（待機・歩きの間に選んだ技だけ）。
    static void NotePicked(BossComponent& boss, const BossStanceRow& row, bool attack);
    // 剣気を吸われた（捕まえた tick）。
    static void NoteCatch(BossComponent& boss);
    // 技を始める：その技に一拍止めが付いていれば、足す F を返して消す（ほかの技なら 0 を返して消す）。
    static int TakeHold(BossComponent& boss, int row);
    // 斬撃を放った：横の剣気なら、ローリングの癖の側へ向きを寄せる。
    static void LeanSlash(const BossFrame& frame, BossWave& wave);
    static const char* StateName(BossGourdState state);
};

} // namespace Project_TD2_2
