#pragma once

#include <array>
#include <cstdint>

namespace Project_TD2_2 {

// ローリングの向きを覚えておく数（「ローリングの癖」の節が見る最近の回数の上限）。
inline constexpr size_t kBossRollHabitCapacity = 16;
// 立ち位置（ボスの正面からの角度・距離）を覚えておく固定 tick の数。60 tick/秒で 10 秒。「背後にいる時間」「遠くに
// い続けている」の節の秒はこれより長く見られない。
inline constexpr size_t kBossStanceHistory = 600;
// 技ごとの「後隙に当てられた回数」を数える行の数（行動ツリーの技選びと同じく、表の先頭から 64 行まで）。
inline constexpr size_t kBossPunishRows = 64;

// ボスから見たローリングの向き。左右はボスがプレイヤーへ向いた時のボスの左右（プレイヤーから見ると逆）。
enum class BossRollDirection { Away, Toward, Left, Right };

// ボスが毎 tick 見ているプレイヤーの様子（行動ツリーの条件の節が読む）。時刻は boss.clockSeconds の秒、tick は boss.tick。
// 書くのは BossObserver だけ。戦いを読み直す（シーンを開き直す）と、部品ごと作り直されて空になる。
struct BossPlayerWatch {
    bool present = false;
    bool healing = false;
    bool absorbing = false;
    float absorbSince = 0.0f;
    bool inFire = false;
    float fireSince = 0.0f;
    // 最後にローリングを始めた時刻（負はまだ見ていない）と、始めた時のボスから見た向き。
    float rollSeconds = -1.0f;
    BossRollDirection rollDirection = BossRollDirection::Away;
    // 前の tick にローリング中だったか・その経過 F（続けてもう一度転がったのを見分ける）。
    bool wasRolling = false;
    int rollFrame = 0;
    // ローリングの癖：始めた時の向きを新しい順に輪で覚える（rollHabit[(rollHabitNext − 1 − i) % 容量] が i 個前）。
    std::array<BossRollDirection, kBossRollHabitCapacity> rollHabit{};
    size_t rollHabitCount = 0;
    size_t rollHabitNext = 0;
    // 場の燃えている面積の割合 (%)。
    float burningPercent = 0.0f;

    // 射撃：次の弾が来る（構えの間か、押し続けて連射を待っている）。shotIntentTick はその弾を待ち始めた tick
    // （構えに入った tick か、連射で前の弾を撃った tick）。
    bool shotPending = false;
    bool shotWindup = false;
    uint64_t shotIntentTick = 0;
    // 狙いの線がボスの体を通るか（水平に見て、体の軸からの外れ shotMiss m が体の半径＋余裕以下）。
    bool shotAimed = false;
    float shotMiss = 0.0f;
    // 撃った弾の累計（砲撃の数え。増えたら撃った）、続けて撃った数、最後に撃った tick、その前の弾との間 (F)。
    uint64_t shotsSeen = 0;
    int shotStreak = 0;
    uint64_t shotLastTick = 0;
    int shotInterval = 0;

    // 立ち位置：ボスの正面からプレイヤーへの水平の角度（度、0＝正面、180＝真後ろ）と距離 (m) を tick ごとに輪で覚える。
    // 角度は度の整数、距離は cm の整数で持つ（プレイヤーがいない tick は 0・0）。
    std::array<uint8_t, kBossStanceHistory> stanceAngle{};
    std::array<uint16_t, kBossStanceHistory> stanceDistance{};
    size_t stanceCount = 0;
    size_t stanceNext = 0;
    // 1 固定 tick の秒（節が秒を tick の数へ直す）。
    float tickSeconds = 1.0f / 60.0f;
    float angle = 0.0f;
    float distance = 0.0f;

    // プレイヤーの残り。ヒョウタンの煙 (%、ヒョウタンが無ければ負)・満杯（吸い込める空きが無い）・精力 (%)・HP (%)・
    // 残りの回復の数。
    float gourdPercent = -1.0f;
    bool gourdFull = false;
    float staminaPercent = 100.0f;
    float healthPercent = 100.0f;
    int healCharges = 0;
    // SeeRemain のログを、満杯・回復の数・HP（1 % 刻み）が変わった時だけ出すための前の値。
    int loggedRemain = -1;

    // 咎められた記録：技（表の行）ごとに、その技の後隙の間に当てられた回数（戦いの間ずっと数える。表を読み直すと消す）。
    std::array<uint16_t, kBossPunishRows> punished{};
};

} // namespace Project_TD2_2
