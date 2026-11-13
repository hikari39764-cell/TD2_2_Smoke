#pragma once

#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "BossMoveCondition.h"

namespace Project_TD2_2 {

// 動きの型。表の「動きの型」列（表示名か型の名前）で決まる。体の動かし方と斬撃の出方がこれで変わる。
enum class BossMoveType {
    Dash,
    Jump,
    SlashR,
    SlashL,
    Overhead,
    Kick,
    Knife,
    String,
    Ambush,
    Clone,
    SlashH,
    SlashV,
    WalkSide,
    WalkIn,
    Backstep,
    // 引爆：溜めてから、燃えている地面を足元から外へ順に爆ぜさせる（判定サイズ＝爆ぜたセルから当たる距離、
    // 斬撃速度＝前線の広がる速さ）。燃えている所が無ければ出せない。
    Detonate,
    // 天降り斬り：屈んで高く跳び、空中で溜めてから、プレイヤーのいた所へ落ちて着地の周りに当てる
    // （判定サイズ＝着地点からの半径。攻撃中＝落ちる F（ボスの skyfallDiveFrames）＋着地の当たりの F）。
    Skyfall,
    Count
};

// 被弾の種類：のけぞり／吹っ飛び／最後の一撃だけ吹っ飛び。
enum class BossKnock { Flinch, Blow, LastBlow };

// 連撃の一撃の振り方。横は左右を往復、縦は上から下、袈裟・逆袈裟は斜め。
enum class BossStrikeDirection { Horizontal, Vertical, Kesa, ReverseKesa };

// 技の出し方（試作の use）。
enum class BossMoveUse {
    // 中立の技選びで始動する。発生条件に連続攻撃も書いてあれば、割り込みにも出る。
    Start,
    // コンボ先としてだけ出る（役割が「コンボ用」）。
    Combo,
    // 割り込み専用（発生条件が連続攻撃で、距離重みが空欄）。
    Interrupt,
    // 特別な出し方（霞分身・引爆）。中立の技選びとコンボには出ず、行動ツリーの枝が名前で出す。
    Special,
};

// 霞隠れの区間の役目：隠れる（当たらない）／現れる（背後や横へ回り込んで構える）。ほかの型は Normal。
enum class BossSegmentKind { Normal, Hide, Appear };

// 技が地面に残す火の形（表の「残す火」列の 1 項目：線／跡／交差／池／輪）。どこへいつ塗るかは BossBurnMarks。
// 線＝飛ぶ斬撃が通った地面、跡＝瞬間突進でボスが通った床、交差＝一撃の狙った所の X、池＝円、輪＝回し蹴りの輪の終わり。
enum class BossBurnShape { None, Line, Trail, Cross, Pool, Ring };

// 1 行に書ける火の数。表の 1 行に 2〜3 個あれば足りる（増やすと BossMove が大きくなるだけ）。
inline constexpr size_t kBossBurnMarkCapacity = 4;

// 「残す火」の 1 項目（形[:一撃番号][@フェーズN]）と、その「火の幅 m」「火の秒数」。
struct BossBurnMark {
    BossBurnShape shape = BossBurnShape::None;
    // 火を残す一撃（1 から数える）。0 は、その形が付くどの一撃も。
    int strike = 0;
    // このフェーズ以上の時だけ残す。
    int minPhase = 1;
    // 線・跡・交差は線の幅、池は直径、輪は帯の幅 (m)。
    float width = 0.0f;
    float seconds = 0.0f;
};

// 「攻撃中」の 1 区間。hit の区間で剣を振り（斬撃を出し）、それ以外は構えや移動。
struct BossSegment {
    int length = 1;
    bool hit = false;
    int strike = 0;
    bool feint = false;
    // 移動の行：出すたびにこの範囲から長さを決める。
    int rangeMin = 0;
    int rangeMax = 0;
    BossSegmentKind kind = BossSegmentKind::Normal;
};

// 一撃ぶんの値。連撃で別の行を参照した一撃は、その行の値が入る。
struct BossStrikeData {
    BossStrikeDirection direction = BossStrikeDirection::Horizontal;
    bool feint = false;
    float size = 0.0f;
    float speed = 0.0f;
    float homing = 0.0f;
    float step = 0.0f;
    bool blow = false;
    int damage = 0;
    // 参照した行（-1 は参照なし）。
    int reference = -1;
};

// 表の 1 行を読んだ結果。F は 1/60 秒の固定 tick。
struct BossMove {
    std::string name;
    std::string description;
    std::string role;
    std::string condition;
    std::string comboText;
    BossMoveType type = BossMoveType::SlashH;
    int pre = 0;
    int post = 0;
    int act = 0;
    std::vector<BossSegment> segments;
    std::vector<BossStrikeData> strikes;
    float size = 0.0f;
    float speed = 0.0f;
    float homing = 0.0f;
    float step = 0.0f;
    float side = 0.0f;
    int damage = 0;
    bool damageAuto = false;
    BossKnock knock = BossKnock::Flinch;
    // 残す火（「残す火」「火の幅 m」「火の秒数」）。行そのものの値で、連撃が参照した別の行の火は持ってこない。
    // 読めなかった項目は入れない（burnMarkCount は読めた数）。
    std::array<BossBurnMark, kBossBurnMarkCapacity> burnMarks{};
    int burnMarkCount = 0;
    // 追尾を止める F：一撃を放つこの F 前から放つまで、ボスはプレイヤーの方へ向き直らない（0 は放つまで追う）。
    // 放つ直前に横へよければ外れる、という読み合いを作る。
    int trackStopFrames = 0;
    // 溜めの揺らぎ F：出すたびに前隙の溜め（構えたまま止まる間）へ足す F の候補。同じ確率で 1 つ。空は足さない。
    std::vector<int> windupExtras;
    // 崩せる被弾数：前隙（溜めを含む）の間にこれだけ当てられると技が崩れ、気絶する。0 は崩れない。
    int breakHits = 0;
    // 距離重み（空欄は -1）とコンボ確率（空欄は -1）。どちらも技の選び方（行動ツリー）が使う。
    float distanceWeight = -1.0f;
    float comboChance = -1.0f;
    bool comboOnly = false;
    BossMoveUse use = BossMoveUse::Start;
    // 発生条件を読んだもの。
    BossMoveCondition when;
    // コンボ先の行（「コンボ先」の名前に合う行。1 つの名前が何行にも合えば全部）。
    std::vector<int> comboTargets;
    int hitCount = 0;
    // 実行器が動かせる型か。動かせない型の行は読み込むが、出そうとすると断る。
    bool executable = false;
    std::vector<std::string> warnings;
};

// 表を読むときに使う共通の値（ボスのコンポーネントの項目）。
struct BossTiming {
    int swingFrames = 10;
    int gapSmallFrames = 10;
    int gapMediumFrames = 20;
    int gapLargeFrames = 44;
    int followFrames = 6;
    int feintFrames = 24;
    int kamaeFrames = 20;
    int raiseFrames = 12;
    int swingInFrames = 6;
    // 天降り斬りの攻撃中のうち、落ちる F（残りが着地の当たり）。
    int skyfallDiveFrames = 10;
    float damageBase = 6.0f;
    float damagePerFrame = 0.15f;
};

// 動きの型の既定値（Data/Boss/kasumi_bushi_move_types.csv の 1 行）。表の空欄はこの値になる。
struct BossTypeDefaults {
    std::string label;
    int pre = 0;
    std::string act;
    int post = 0;
    float size = 0.0f;
    float damage = 0.0f;
    float speed = 0.0f;
    float homing = 0.0f;
    float step = 0.0f;
    bool loaded = false;
};

// 技の表。読み方は試作「霞の剣客」の行動表と同じ（攻撃中の「30F/5F/35F」、連撃の「小横/大縦振り/中遅横」、移動の「40~100F」）。
class BossMoveTable final {
public:
    // 型の既定値の表を読む。読めない行は warnings に入れて飛ばす。
    bool LoadTypes(std::string_view csv, std::vector<std::string>& warnings);
    // 技の表を読む。先に LoadTypes を済ませておく。読めない値は行の warnings に入れ、既定値で動かす。
    bool LoadMoves(std::string_view csv, const BossTiming& timing, std::vector<std::string>& warnings);
    const std::vector<BossMove>& GetMoves() const { return moves_; }
    // 名前が完全に一致する行。無ければ -1。
    int Find(std::string_view name) const;
    static const char* TypeKey(BossMoveType type);
    // 残す火の形の名前（ログ用。none／line／trail／cross／pool／ring）。
    static const char* BurnKey(BossBurnShape shape);
    static bool IsMoveType(BossMoveType type);
    // 剣を振る型（振っている間に斬撃をかたどり、振り抜いた瞬間に飛ばす）。
    static bool IsSwingType(BossMoveType type);
    // 実行器が動かせる型（いまは全部の型）。
    static bool IsExecutable(BossMoveType type);
    // 一撃 strike の値。strike が範囲外なら最後の一撃。
    static const BossStrikeData& Strike(const BossMove& move, int strike);

private:
    BossTypeDefaults types_[static_cast<int>(BossMoveType::Count)];
    std::vector<BossMove> moves_;
};

} // namespace Project_TD2_2
