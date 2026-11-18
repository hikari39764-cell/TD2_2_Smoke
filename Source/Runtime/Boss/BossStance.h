#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "HIKARI/Project/PropertyFields.h"

namespace Project_TD2_2 {

class BossComponent;
class BossMoveTable;

// 間合いの帯（c-19e3b0）：中心どうしの水平距離で、密着・中・遠・圏外の 4 つ。境はボスの項目（stanceBand…）。
enum class BossBand { Close, Mid, Far, Out, Count };

// ボスの姿勢（c-19e3b0）：攻勢＝連段を出す／散歩＝射程の中で刀を構えて横へゆっくり歩く（自分からはほとんど出さない。
// 受け返しは c-44fdc5）／離脱＝圏外へ下がって見る。
enum class BossStanceKind { Aggro, Stroll, Withdraw, Count };

// 技の種類（重みの動き）：遠距離（剣気の技）・接近（詰める・密着の技）・受け返し（散歩の反撃。c-44fdc5）。
enum class BossMoveClass { Far, Near, Counter, Count };

// 姿勢と帯の重みの表（Data/Boss/kasumi_bushi_stance.csv）の 1 行：その姿勢・帯で出せる技と、その重み・種類・冷却。
struct BossStanceRow {
    int phase = 1;
    BossStanceKind stance = BossStanceKind::Aggro;
    BossBand band = BossBand::Mid;
    std::string move;
    // 技の表の行（読んだ後に名前から決める。無ければ -1）。
    int row = -1;
    BossMoveClass kind = BossMoveClass::Near;
    float weight = 1.0f;
    float cooldown = 0.0f;
};

// 姿勢と帯の重みの表。
class BossStanceTable final {
public:
    // 表を読む。読めない値は warnings に入れて飛ばす。技の名前は Resolve で技の表の行へ結び付ける。
    bool Load(std::string_view csv, std::vector<std::string>& warnings);
    void Resolve(const BossMoveTable& moves, std::vector<std::string>& warnings);
    const std::vector<BossStanceRow>& GetRows() const { return rows_; }

private:
    std::vector<BossStanceRow> rows_;
};

// 姿勢の決め方の値（ボスの項目。調整ガイド g-56cc52）。
struct BossStanceSettings {
    void Describe(HIKARI::PropertyFields& fields);

    // 帯の境 (m)：密着 < closeMeters ≦ 中 < midMeters ≦ 遠 < farMeters ≦ 圏外。
    float closeMeters = 6.0f;
    float midMeters = 10.0f;
    float farMeters = 15.0f;
    // 攻勢で出す連段の組の数（最小〜最大のどれか）。出し終えたら離脱か散歩へ。
    int combosMin = 2;
    int combosMax = 3;
    // 連段の後に離脱へ移る確率（%。残りは散歩）。プレイヤーに当てた後はこちら（追い打ちしない）。
    float withdrawChance = 20.0f;
    float withdrawAfterHitChance = 35.0f;
    // 散歩の長さ（秒）と、圏外で見る長さ（秒）。どちらも最小〜最大のどれか。
    float strollMinSeconds = 1.2f;
    float strollMaxSeconds = 2.5f;
    float observeMinSeconds = 0.8f;
    float observeMaxSeconds = 2.0f;
    // 長い間：この回数（最小〜最大のどれか）の攻勢ごとに 1 度、散歩・見る長さをこちらにする（秒）。
    int longPauseEveryMin = 3;
    int longPauseEveryMax = 4;
    float longPauseMinSeconds = 2.5f;
    float longPauseMaxSeconds = 4.0f;
    // 離脱で後ろへ跳ぶ回数の上限（跳んでも圏外へ出られなければ、射程の中で散歩へ）。
    int withdrawStepsMax = 2;
    // 重みの動き：直前と違う種類に掛ける倍率（遠距離と接近が交互に出る）、最近出した技に掛ける倍率（新しい順に 3 つ）。
    float classSwitchScale = 1.8f;
    float sameClassScale = 0.55f;
    float recentScale0 = 0.25f;
    float recentScale1 = 0.5f;
    float recentScale2 = 0.75f;
    // 散歩・見るの後の攻勢の始めの技は、前隙がこの F 以上の技だけ（遅い始まり）。
    int slowStartFrames = 36;
    // 圏外で、突進（瞬間突進の型）を続けて出さない（前の攻勢の始めが突進なら、今度は歩いて入る）。
    bool noDashTwice = true;
    // 散歩・見るの切り上げ（c-58fcf0。プレイヤーの動きに応じてすぐ攻勢へ。歩きの途中でも切る）。散歩に入って
    // engageAfterSeconds 秒の後、
    // ・寄って来た：プレイヤー自身がボスへ approachMeters 以上寄り（散歩の間で一番引いた所から）、中の帯より近い
    // ・張り付いている：密着の帯に closeSeconds 秒いる
    // ・離れた：プレイヤー自身がボスから chaseMeters 以上離れ（散歩の間で一番寄った所から）、遠の帯より遠い（追う）
    // （寄る・離れるは距離の増減でなく、プレイヤーの動きのボスへ向かう成分を積んで見る。ボスが間合いを保つ歩きでは動かない）
    // ・撃った・当てられた：散歩の間にプレイヤーが撃つか、ボスが当てられた（遠くから撃ち続けさせない。散歩に入ってすぐでも
    //   切る。最初の弾は反応の枝の霞歩が先に受ける。霞歩が出ない時・出した後もまだ散歩なら、ここで攻勢へ）
    // 攻勢で歩いて詰めている間に撃たれた・当てられた時も、歩くのをやめて今の帯の技を出す。
    // 見る（離脱の後）の間は、プレイヤーが中の帯へ入るか、撃つ・当てられたら攻勢へ。切り上げた攻勢の始めは遅い始まりに限らない。
    float engageAfterSeconds = 0.5f;
    float approachMeters = 1.5f;
    float closeSeconds = 0.8f;
    float chaseMeters = 3.0f;
    bool observeEngage = true;
    // 散歩の歩き：keepMeters の間合いを保つように前後へ寄り（ずれ 1 m につき keepGain、最大で歩く速さの keepSpeedScale 倍）、
    // プレイヤーが横へ mirrorSpeed m/秒より速く動いたら、逆の向きへ回る（向き合ったまま回り合う）。
    float keepMeters = 7.5f;
    float keepGain = 0.5f;
    float keepSpeedScale = 0.8f;
    float mirrorSpeed = 1.5f;
};

// 姿勢の覚え（BossComponent が持つ）。
struct BossStanceMemory {
    BossStanceKind stance = BossStanceKind::Withdraw;
    bool started = false;
    // いまの姿勢に入った時刻と、散歩・見る を終える時刻（秒）。
    float since = 0.0f;
    float until = 0.0f;
    // 離脱で圏外に着いて「見る」に入ったか。
    bool observing = false;
    int withdrawSteps = 0;
    // 攻勢で歩いて詰めている（帯が中より近くなるまで、技を選び直さずに歩き続ける）。approachLast は前に歩きを頼んだ時の
    // 距離（詰まらなくなったら＝闘技場の縁で止められたら、歩くのをやめて今の帯の技を選ぶ。c-58fcf0）。
    bool approaching = false;
    float approachLast = 1.0e9f;
    // 歩いて詰め始めた時刻と、その時の撃った弾の累計（詰める途中で撃たれた・当てられたら、歩くのをやめて今の帯の技を出す）。
    // noWalkOnce は次の技選びで歩きの行を出さない。
    float approachSince = 0.0f;
    uint64_t approachShots = 0;
    bool noWalkOnce = false;
    // 攻勢で出した連段の組の数と、今回の目標。
    int combos = 0;
    int combosTarget = 1;
    // 攻勢の回数と、次の長い間までの回数。
    int aggroCount = 0;
    int nextLongPause = 2;
    bool longPause = false;
    // 攻勢の始めの技を遅い始まりに限るか（散歩・見るの後）。
    bool slowStart = true;
    // 直前に出した技の種類と、最近出した技（新しい順。表の行、無ければ -1）。
    BossMoveClass lastClass = BossMoveClass::Count;
    std::array<int, 3> recent{-1, -1, -1};
    // 前の攻勢の始めの技が突進だったか。
    bool lastOpenerDash = false;
    // 技の表の行ごとに、次に出せる時刻（秒）。
    std::vector<float> readyAt;
    // 攻勢を始めた時のプレイヤーへの当たりの数（連段で当てたかを見る）。
    unsigned playerHitsAtStart = 0;
    // 散歩の間にプレイヤー自身がボスへ寄った距離の積み（離れると減る）と、その最小・最大、密着の帯に入った時刻（入って
    // いなければ負）。切り上げに使う。積みは散歩の歩き（BossMoveExecutor）が足す。
    float playerIn = 0.0f;
    float playerInMin = 0.0f;
    float playerInMax = 0.0f;
    float closeSince = -1.0f;
    // 散歩・離脱に入った時の、プレイヤーの撃った弾の累計（増えたら撃った）。
    uint64_t shotsAtStart = 0;
};

// 何を頼むか（BossStance::Decide の結果）。
struct BossStanceChoice {
    int row = -1;
    // 攻撃の技（攻勢の技選び）。歩き・跳びは false。
    bool attack = false;
    BossMoveClass kind = BossMoveClass::Near;
    float weight = 0.0f;
};

// 姿勢を進め、次に頼む技を決める（行動ツリーの節「姿勢で技を選ぶ」から、待機の時に呼ぶ）。乱数は木の乱数を渡す
// （種で再現できる）。
class BossStance final {
public:
    static BossBand BandOf(const BossStanceSettings& s, float distance);
    static const char* BandName(BossBand band);
    static const char* StanceName(BossStanceKind stance);
    static const char* ClassName(BossMoveClass kind);
    static BossStanceChoice Decide(BossComponent& boss, int phase, float distance, float random0, float random1);
    // 頼んだ技が終わった（コンボを含めて）時に呼ぶ：覚えを進める。
    static void Finished(BossComponent& boss, const BossStanceChoice& choice, float random);
    // 散歩・見るの歩きの間に呼ぶ：プレイヤーの動き（寄る・張り付く・離れる・見ている所へ入る）で攻勢へ移ったら true
    // （呼んだ側は歩きを切り上げて Decide で技を頼む）。
    static bool Engage(BossComponent& boss, float distance);
    // 行動ツリーの節から姿勢を変える（反応の枝など）。いまと同じなら何もしない。
    static void Force(BossComponent& boss, BossStanceKind stance, const char* reason);
    // 節の項目の文字（aggro・stroll・withdraw か、攻勢・散歩・離脱）を姿勢へ。読めなければ false。
    static bool Read(std::string_view text, BossStanceKind& out);
};

} // namespace Project_TD2_2
