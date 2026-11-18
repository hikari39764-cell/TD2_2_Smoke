#pragma once

#include <array>
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
    int combosMin = 1;
    int combosMax = 2;
    // 連段の後に離脱へ移る確率（%。残りは散歩）。プレイヤーに当てた後はこちら（追い打ちしない）。
    float withdrawChance = 45.0f;
    float withdrawAfterHitChance = 65.0f;
    // 散歩の長さ（秒）と、圏外で見る長さ（秒）。どちらも最小〜最大のどれか。
    float strollMinSeconds = 2.0f;
    float strollMaxSeconds = 4.0f;
    float observeMinSeconds = 1.5f;
    float observeMaxSeconds = 4.0f;
    // 長い間：この回数（最小〜最大のどれか）の攻勢ごとに 1 度、散歩・見る長さをこちらにする（秒）。
    int longPauseEveryMin = 2;
    int longPauseEveryMax = 3;
    float longPauseMinSeconds = 4.0f;
    float longPauseMaxSeconds = 6.0f;
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
    // 攻勢で歩いて詰めている（帯が中より近くなるまで、技を選び直さずに歩き続ける）。
    bool approaching = false;
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
    // 行動ツリーの節から姿勢を変える（反応の枝など）。いまと同じなら何もしない。
    static void Force(BossComponent& boss, BossStanceKind stance, const char* reason);
    // 節の項目の文字（aggro・stroll・withdraw か、攻勢・散歩・離脱）を姿勢へ。読めなければ false。
    static bool Read(std::string_view text, BossStanceKind& out);
};

} // namespace Project_TD2_2
