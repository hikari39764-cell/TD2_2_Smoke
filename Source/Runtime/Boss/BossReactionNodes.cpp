#include "BossReactionNodes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <functional>
#include <iterator>
#include <string_view>

#include "BossFrame.h"
#include "BossKasumiStep.h"
#include "BossObserver.h"
#include "BossRules.h"

namespace Project_TD2_2 {
namespace {

using namespace HIKARI::BEHAVIOR;

// 編集器の節の一覧での分類（ほかのボスの節と同じ）。
constexpr char kCategory[] = "ボス 霞武士";

const BossPlayerWatch* WatchOf(void* user) {
    const BossThought* thought = BossBehaviorNodes::ThoughtOf(user);
    return thought != nullptr && thought->boss.watch.present ? &thought->boss.watch : nullptr;
}

float NowOf(void* user) { return BossBehaviorNodes::ThoughtOf(user)->boss.clockSeconds; }

Status Result(bool holds) { return holds ? Status::Success : Status::Failure; }

// ---- プレイヤーが吸い込み中：吸い込みを minSeconds 秒以上続けている。 ----

enum : uint32_t { kHeldSeconds };
constexpr ParamDesc kAbsorbParams[] = {
    {"minSeconds", ValueType::Float, "続けている時間", 0.3, nullptr, false, nullptr,
     "吸い込みをこの時間以上続けていたら成り立つ。0 は吸い始めた瞬間から。", 0.0, 5.0, "s"},
};

Status TickAbsorbing(INodeContext& context, void*, void* user) noexcept {
    const BossPlayerWatch* watch = WatchOf(user);
    return Result(watch != nullptr && watch->absorbing &&
                  NowOf(user) - watch->absorbSince >= context.ParamFloat(kHeldSeconds));
}

// ---- プレイヤーが回復中：回復の動作の間（回復を使い始めてから動けるようになるまで）。 ----

Status TickHealing(INodeContext&, void*, void* user) noexcept {
    const BossPlayerWatch* watch = WatchOf(user);
    return Result(watch != nullptr && watch->healing);
}

// ---- プレイヤーが火の中：燃えている地面の上に minSeconds 秒以上いる。 ----

constexpr ParamDesc kFireParams[] = {
    {"minSeconds", ValueType::Float, "続けている時間", 0.0, nullptr, false, nullptr,
     "燃えている地面の上にこの時間以上いたら成り立つ。0 は踏んだ瞬間から。", 0.0, 10.0, "s"},
};

Status TickInFire(INodeContext& context, void*, void* user) noexcept {
    const BossPlayerWatch* watch = WatchOf(user);
    return Result(watch != nullptr && watch->inFire &&
                  NowOf(user) - watch->fireSince >= context.ParamFloat(kHeldSeconds));
}

// ---- プレイヤーがローリングした直後：withinSeconds 秒以内にローリングを始めていて、向きが合う。 ----

enum : uint32_t { kRolledWithin, kRolledDirection };
constexpr ParamDesc kRolledParams[] = {
    {"withinSeconds", ValueType::Float, "何秒以内", 0.5, nullptr, false, nullptr,
     "ローリングを始めてからこの時間のうちは成り立つ。", 0.0, 5.0, "s"},
    {"direction", ValueType::Text, "向き", 0.0, "any", false, "any|away|toward|sideways|left|right",
     "any：どの向きでも。away：ボスから離れる向き（後ろへ下がる）。toward：ボスへ向かう向き。sideways：横（左右どちらも）。"
     "left／right：ボスから見て左／右（プレイヤーから見ると逆）。前後と横を分ける角度はボスの項目（ローリングの向き）。"},
};

bool DirectionMatches(std::string_view wanted, BossRollDirection seen) {
    if (wanted == "away")
        return seen == BossRollDirection::Away;
    if (wanted == "toward")
        return seen == BossRollDirection::Toward;
    if (wanted == "left")
        return seen == BossRollDirection::Left;
    if (wanted == "right")
        return seen == BossRollDirection::Right;
    if (wanted == "sideways")
        return seen == BossRollDirection::Left || seen == BossRollDirection::Right;
    return true;
}

Status TickRolled(INodeContext& context, void*, void* user) noexcept {
    const BossPlayerWatch* watch = WatchOf(user);
    return Result(watch != nullptr && watch->rollSeconds >= 0.0f &&
                  NowOf(user) - watch->rollSeconds <= context.ParamFloat(kRolledWithin) &&
                  DirectionMatches(context.ParamText(kRolledDirection), watch->rollDirection));
}

// ---- 距離：プレイヤーまでの水平距離が min 以上 max 以下。 ----

enum : uint32_t { kDistanceMin, kDistanceMax };
constexpr ParamDesc kDistanceParams[] = {
    {"min", ValueType::Float, "最小", 0.0, nullptr, false, nullptr, "プレイヤーまでの水平距離がこれ以上。", 0.0, 40.0,
     "m"},
    {"max", ValueType::Float, "最大", 3.0, nullptr, false, nullptr, "プレイヤーまでの水平距離がこれ以下。", 0.0, 40.0,
     "m"},
};

Status TickDistance(INodeContext& context, void*, void* user) noexcept {
    const BossThought* thought = BossBehaviorNodes::ThoughtOf(user);
    if (thought == nullptr || thought->situation.distance < 0.0f)
        return Status::Failure;
    const float distance = thought->situation.distance;
    return Result(distance >= context.ParamFloat(kDistanceMin) && distance <= context.ParamFloat(kDistanceMax));
}

// ---- 燃えている面積：場（闘技場の円）の燃えている割合が以上〜以下 %。 ----

enum : uint32_t { kBurningAtLeast, kBurningAtMost };
constexpr ParamDesc kBurningParams[] = {
    {"atLeastPercent", ValueType::Float, "以上", 1.0, nullptr, false, nullptr,
     "闘技場の円のうち燃えている面積の割合がこれ以上。", 0.0, 100.0, "%"},
    {"atMostPercent", ValueType::Float, "以下", 100.0, nullptr, false, nullptr, "…がこれ以下。", 0.0, 100.0, "%"},
};

Status TickBurning(INodeContext& context, void*, void* user) noexcept {
    const BossThought* thought = BossBehaviorNodes::ThoughtOf(user);
    if (thought == nullptr)
        return Status::Failure;
    const float percent = thought->boss.watch.burningPercent;
    return Result(percent >= context.ParamFloat(kBurningAtLeast) && percent <= context.ParamFloat(kBurningAtMost));
}

// ---- 燃えている面積に比例した確率：燃えている割合 × 1% あたりの確率（上限まで）で成功する。呼ばれるたびに振る。 ----

enum : uint32_t { kRollPerPercent, kRollMax };
constexpr ParamDesc kBurningRollParams[] = {
    {"chancePerPercent", ValueType::Float, "1% あたりの確率", 4.0, nullptr, false, nullptr,
     "闘技場の円のうち燃えている面積 1 % ごとの確率。燃えている割合 × これ が成功する確率。", 0.0, 100.0, "%"},
    {"maxChance", ValueType::Float, "上限", 60.0, nullptr, false, nullptr, "確率の上限。", 0.0, 100.0, "%"},
};

Status TickBurningRoll(INodeContext& context, void*, void* user) noexcept {
    const BossThought* thought = BossBehaviorNodes::ThoughtOf(user);
    if (thought == nullptr)
        return Status::Failure;
    const BossComponent& boss = thought->boss;
    const float percent = boss.watch.burningPercent;
    const float chance = std::min(context.ParamFloat(kRollMax), percent * context.ParamFloat(kRollPerPercent));
    // 燃えていない間は振らない（ログも出さない）。
    if (!(chance > 0.0f))
        return Status::Failure;
    const float roll = context.Random() * 100.0f;
    const bool pass = roll < chance;
    char message[192]{};
    std::snprintf(message, sizeof(message), "BurningRoll tick=%llu percent=%.3f roll=%.2f chance=%.2f pass=%d",
                  static_cast<unsigned long long>(boss.tick), static_cast<double>(percent), static_cast<double>(roll),
                  static_cast<double>(chance), static_cast<int>(pass));
    BossLog::Write(message);
    return Result(pass);
}

// ---- 反応の間隔：間隔が過ぎていて、確率に当たれば通す。通した後ろの技が始まったら、そこから間隔を数える。 ----

enum : uint32_t {
    kReactionLabel,
    kReactionCooldown,
    kReactionChance,
    kReactionReroll,
    kReactionCutRecovery,
    kReactionDelayMin,
    kReactionDelayMax,
    kReactionDuringWalk
};
constexpr ParamDesc kReactionParams[] = {
    {"label", ValueType::Text, "名前", 0.0, "反応", false, nullptr,
     "ログ（Pick … why=reaction branch=名前）に出す名前。間隔はこの名前ごとに数える（同じ名前の節は間隔を分け合う）。"},
    {"cooldown", ValueType::Float, "間隔", 6.0, nullptr, false, nullptr, "この反応の技を出した後、この時間は通さない。",
     0.0, 120.0, "s"},
    {"chance", ValueType::Float, "確率", 100.0, nullptr, false, nullptr,
     "条件がそろった時に反応する確率。外れたら「振り直しの間」だけ振らない（毎 tick 振り直さない）。", 0.0, 100.0, "%"},
    {"rerollSeconds", ValueType::Float, "振り直しの間", 0.5, nullptr, false, nullptr,
     "確率を振った後、次に振るまでの時間。条件が続いていると、この間ごとに 1 回振る。", 0.0, 10.0, "s"},
    {"cutRecovery", ValueType::Bool, "後隙を切り上げる", 0.0, nullptr, false, nullptr,
     "オンにすると、ボスの後隙の間も反応して後隙を切り上げる。オフ（既定）は待機中だけ反応する"
     "（大技の後隙は咎められるまま残る）。"},
    {"delayMinFrames", ValueType::Int, "反応の遅れ 最小", 0.0, nullptr, false, nullptr,
     "確率に当たってから後ろの技を始めるまでの遅れの短い方（1 F は 1/60 秒）。人が見てから動くまでの間。"
     "最小・最大とも 0 なら、当たった tick に始める。",
     0.0, 60.0, "F"},
    {"delayMaxFrames", ValueType::Int, "反応の遅れ 最大", 0.0, nullptr, false, nullptr,
     "遅れの長い方。当たるたびに最小〜最大のどれかになる。待つ間に条件が崩れても反応は出る（見てから動き出した後）。"
     "待つ間にボスが待機（「後隙を切り上げる」なら後隙）でなくなったら取りやめる。",
     0.0, 60.0, "F"},
    {"duringWalk", ValueType::Bool, "移動中も反応する", 0.0, nullptr, false, nullptr,
     "オンにすると、歩きの技（横歩き・間合いを詰める）の間も反応して歩きを切り上げる。オフ（既定）は待機中だけ。"
     "技の前隙・攻撃中・後隙（「後隙を切り上げる」がオフなら）の間は、どちらでも反応しない。"},
};

// 反応の遅れを待っている間の、節ごとの状態（確率に当たってから後ろの技を始めるまで）。走り始めるたびに 0 から。
struct ReactionState {
    uint64_t lastTick;
    int32_t delay;
    int32_t waited;
    bool pending;
};

BossReactionMemory* MemoryOf(BossComponent& boss, size_t key) {
    BossBrainMemory& brain = boss.brain;
    for (size_t i = 0; i < brain.reactionCount; ++i) {
        if (brain.reactions[i].key == key)
            return &brain.reactions[i];
    }
    if (brain.reactionCount < brain.reactions.size()) {
        BossReactionMemory& memory = brain.reactions[brain.reactionCount++];
        memory = {};
        memory.key = key;
        return &memory;
    }
    if (!brain.reactionFullLogged) {
        brain.reactionFullLogged = true;
        BossLog::Warn("行動ツリーの「反応の間隔」の名前が多すぎるので、それより後の名前の反応は通しません");
    }
    return nullptr;
}

// 通した：すぐ後ろの技が始まったら、この名前の間隔を始める（BossReactionNodes::NoteStarted）。
void Arm(INodeContext& context, BossThought& thought, std::string_view label) {
    thought.reactionArmed = true;
    thought.reactionKey = std::hash<std::string_view>{}(label);
    thought.reactionCooldown = std::max(0.0f, context.ParamFloat(kReactionCooldown));
    BossBehaviorNodes::CopyText(thought.reactionLabel, sizeof(thought.reactionLabel), label);
}

// 遅れの長さ（F）。最小〜最大が 0〜0（既定）なら 0 で、乱数を使わない（遅れの無い木の技の出方を変えない）。
int32_t DelayOf(INodeContext& context) {
    const int64_t low = std::max<int64_t>(0, context.ParamInt(kReactionDelayMin));
    const int64_t high = std::max(low, context.ParamInt(kReactionDelayMax));
    if (high <= low)
        return static_cast<int32_t>(low);
    const float span = static_cast<float>(high - low);
    return static_cast<int32_t>(low + static_cast<int64_t>(std::floor(context.Random() * (span + 1.0f))));
}

// 遅れを待つ：木を進める tick（待機中、「後隙を切り上げる」なら後隙も）が続いている間だけ数える。続かなかった
// （気絶などで木を進めない tick があった）なら取りやめる。待ち終えたら、条件がもう崩れていても通す。
Status TickDelay(INodeContext& context, ReactionState& gate, BossThought& thought, std::string_view label) {
    const BossComponent& boss = thought.boss;
    char message[224]{};
    if (boss.tick != gate.lastTick + 1) {
        gate.pending = false;
        std::snprintf(message, sizeof(message), "ReactionDrop tick=%llu branch=%.*s waited=%d delay=%d",
                      static_cast<unsigned long long>(boss.tick), static_cast<int>(label.size()), label.data(),
                      gate.waited, gate.delay);
        BossLog::Write(message);
        return Status::Failure;
    }
    gate.lastTick = boss.tick;
    if (++gate.waited < gate.delay)
        return Status::Running;
    gate.pending = false;
    std::snprintf(message, sizeof(message), "ReactionGo tick=%llu branch=%.*s waited=%d",
                  static_cast<unsigned long long>(boss.tick), static_cast<int>(label.size()), label.data(),
                  gate.waited);
    BossLog::Write(message);
    Arm(context, thought, label);
    return Status::Success;
}

Status TickReaction(INodeContext& context, void* state, void* user) noexcept {
    BossThought* thought = BossBehaviorNodes::ThoughtOf(user);
    if (thought == nullptr)
        return Status::Failure;
    // 反応して技を出せるのは待機中（待ちを切り上げる）だけ。「後隙を切り上げる」節は後隙の間も、「移動中も反応する」節は
    // 歩きの技の間も。
    const bool recovery = thought->moment == BossBrainMoment::Recovery;
    const bool walking = thought->moment == BossBrainMoment::Walking;
    if (thought->moment != BossBrainMoment::Idle && !(recovery && context.ParamBool(kReactionCutRecovery)) &&
        !(walking && context.ParamBool(kReactionDuringWalk)))
        return Status::Failure;
    auto& gate = *static_cast<ReactionState*>(state);
    const std::string_view label = context.ParamText(kReactionLabel);
    if (gate.pending)
        return TickDelay(context, gate, *thought, label);
    BossComponent& boss = thought->boss;
    BossReactionMemory* memory = MemoryOf(boss, std::hash<std::string_view>{}(label));
    const float now = boss.clockSeconds;
    if (memory == nullptr || now < memory->readySeconds || now < memory->rerollSeconds)
        return Status::Failure;
    const float chance = context.ParamFloat(kReactionChance);
    const float roll = context.Random() * 100.0f;
    memory->rerollSeconds = now + std::max(0.0f, context.ParamFloat(kReactionReroll));
    const bool pass = roll < chance;
    const int32_t delay = pass ? DelayOf(context) : 0;
    char message[240]{};
    std::snprintf(message, sizeof(message),
                  "Reaction tick=%llu branch=%.*s roll=%.2f chance=%.2f pass=%d from=%s delay=%d",
                  static_cast<unsigned long long>(boss.tick), static_cast<int>(label.size()), label.data(),
                  static_cast<double>(roll), static_cast<double>(chance), static_cast<int>(pass),
                  recovery  ? "recovery"
                  : walking ? "walk"
                            : "idle",
                  delay);
    BossLog::Write(message);
    if (!pass)
        return Status::Failure;
    if (delay > 0) {
        gate = {boss.tick, delay, 0, true};
        return Status::Running;
    }
    Arm(context, *thought, label);
    return Status::Success;
}

bool ReactionProgress(INodeContext&, const void* state, void*, NodeProgress& out) noexcept {
    const auto& gate = *static_cast<const ReactionState*>(state);
    if (!gate.pending || gate.delay <= 0)
        return false;
    out.current = static_cast<float>(gate.waited);
    out.total = static_cast<float>(gate.delay);
    out.fraction = out.current / out.total;
    std::snprintf(out.text, sizeof(out.text), "遅れ %d / %d F", gate.waited, gate.delay);
    return true;
}

// ---- プレイヤーが撃とうとしている：構えに入って（連射なら前の弾を撃って）から withinFrames F 以内で、次の弾が
// minStreak 発目以上。aimedOnly なら狙いの線がボスの体を通る時だけ。 ----

enum : uint32_t { kShootWithin, kShootAimed, kShootStreak };
constexpr ParamDesc kShootParams[] = {
    {"withinFrames", ValueType::Int, "何 F 以内", 8.0, nullptr, false, nullptr,
     "撃つ構えに入ってから（押し続けの連射なら前の弾を撃ってから）この F のうちは成り立つ。構えは 6 F で、"
     "弾は初速 60 m/s なので、撃ってからでは間に合わない。",
     0.0, 60.0, "F"},
    {"aimedOnly", ValueType::Bool, "ボスへ向いている時だけ", 1.0, nullptr, false, nullptr,
     "オンなら、狙いの線がボスの体（半径＋ボスの項目「狙いの余裕」）を通る時だけ成り立つ。"},
    {"minStreak", ValueType::Int, "続けて何発目以上", 1.0, nullptr, false, nullptr,
     "次に来る弾が、続けて撃つこの発目以上なら成り立つ（1 はどの弾でも）。続けたとみなす間はボスの項目。", 1.0, 20.0},
};

Status TickShooting(INodeContext& context, void*, void* user) noexcept {
    const BossPlayerWatch* watch = WatchOf(user);
    if (watch == nullptr || !watch->shotPending)
        return Status::Failure;
    const BossComponent& boss = BossBehaviorNodes::ThoughtOf(user)->boss;
    const int64_t within = std::max<int64_t>(0, context.ParamInt(kShootWithin));
    return Result(boss.tick - watch->shotIntentTick <= static_cast<uint64_t>(within) &&
                  (!context.ParamBool(kShootAimed) || watch->shotAimed) &&
                  BossObserver::NextShotOrdinal(boss) >= context.ParamInt(kShootStreak));
}

// ---- ローリングの癖：最近 rolls 回のローリングのうち、向きが direction だった割合が atLeastPercent 以上。 ----

enum : uint32_t { kHabitDirection, kHabitPercent, kHabitRolls, kHabitMinRolls };
constexpr ParamDesc kHabitParams[] = {
    {"direction", ValueType::Text, "向き", 0.0, "away", false, "away|toward|sideways|left|right",
     "away：ボスから離れる向き。toward：ボスへ向かう向き。sideways：横（左右どちらも）。left／right：ボスから見て左／右"
     "（プレイヤーから見ると逆）。前後と横を分ける角度はボスの項目（ローリングの向き）。"},
    {"atLeastPercent", ValueType::Float, "割合（以上）", 60.0, nullptr, false, nullptr,
     "最近のローリングのうち、この向きだった割合がこれ以上なら成り立つ。", 0.0, 100.0, "%"},
    {"rolls", ValueType::Int, "最近の何回", 6.0, nullptr, false, nullptr,
     "割合を数える、最近のローリングの回数（16 まで）。", 1.0, 16.0},
    {"minRolls", ValueType::Int, "少なくとも何回", 3.0, nullptr, false, nullptr,
     "数えたローリングがこれより少ない間は成り立たない（1 回だけで癖とみなさない）。", 1.0, 16.0},
};

Status TickHabit(INodeContext& context, void*, void* user) noexcept {
    const BossPlayerWatch* watch = WatchOf(user);
    if (watch == nullptr)
        return Status::Failure;
    int counts[4]{};
    const int samples = BossObserver::CountRecentRolls(*watch, static_cast<int>(context.ParamInt(kHabitRolls)), counts);
    if (samples <= 0 || samples < context.ParamInt(kHabitMinRolls))
        return Status::Failure;
    const std::string_view wanted = context.ParamText(kHabitDirection);
    int matched = 0;
    for (const BossRollDirection direction :
         {BossRollDirection::Away, BossRollDirection::Toward, BossRollDirection::Left, BossRollDirection::Right}) {
        if (DirectionMatches(wanted, direction))
            matched += counts[static_cast<int>(direction)];
    }
    return Result(static_cast<float>(matched) * 100.0f >=
                  context.ParamFloat(kHabitPercent) * static_cast<float>(samples));
}

// ---- この技は咎められている：技（表の行の名前）の後隙に当てられた回数が atLeast 以上（戦いの間ずっと数える）。 ----

enum : uint32_t { kPunishedMove, kPunishedAtLeast };
constexpr ParamDesc kPunishedParams[] = {
    {"move", ValueType::Text, "技（表の行の名前）", 0.0, "", false, nullptr,
     "技の表の「ボス行動」の名前。変化で切り替わった技は「<変化先>（変化）」の名前で数える。"},
    {"atLeast", ValueType::Int, "回数（以上）", 2.0, nullptr, false, nullptr,
     "この技の後隙の間に、これだけ当てられていれば成り立つ。", 1.0, 50.0},
};

Status TickPunished(INodeContext& context, void*, void* user) noexcept {
    const BossThought* thought = BossBehaviorNodes::ThoughtOf(user);
    if (thought == nullptr)
        return Status::Failure;
    const int row = BossRules::FindMove(thought->boss, context.ParamText(kPunishedMove));
    if (row < 0 || static_cast<size_t>(row) >= kBossPunishRows)
        return Status::Failure;
    return Result(thought->boss.watch.punished[static_cast<size_t>(row)] >=
                  std::max<int64_t>(1, context.ParamInt(kPunishedAtLeast)));
}

// ---- 背後にいる時間：ボスの正面から atLeastDegrees 度以上の所に、minSeconds 秒以上続けている。 ----

enum : uint32_t { kStanceThreshold, kStanceSeconds };
constexpr ParamDesc kBehindParams[] = {
    {"atLeastDegrees", ValueType::Float, "正面からの角度（以上）", 120.0, nullptr, false, nullptr,
     "ボスの向き（正面）からプレイヤーへの水平の角度がこれ以上なら背後とみなす。90 は真横、180 は真後ろ。", 0.0, 180.0,
     "deg"},
    {"minSeconds", ValueType::Float, "続けている時間", 1.0, nullptr, false, nullptr,
     "背後にこの時間以上続けていれば成り立つ（10 秒まで見られる）。", 0.0, 10.0, "s"},
};

Status TickBehind(INodeContext& context, void*, void* user) noexcept {
    const BossPlayerWatch* watch = WatchOf(user);
    return Result(watch != nullptr && watch->angle >= context.ParamFloat(kStanceThreshold) &&
                  BossObserver::SecondsBehind(*watch, context.ParamFloat(kStanceThreshold)) >=
                      context.ParamFloat(kStanceSeconds));
}

// ---- 遠くにい続けている：ボスから atLeastMeters m 以上の所に、minSeconds 秒以上続けている。 ----

constexpr ParamDesc kFarParams[] = {
    {"atLeastMeters", ValueType::Float, "距離（以上）", 12.0, nullptr, false, nullptr,
     "プレイヤーまでの水平距離がこれ以上なら遠いとみなす。弾は 10 m から弱まり 15 m で消える。", 0.0, 40.0, "m"},
    {"minSeconds", ValueType::Float, "続けている時間", 3.0, nullptr, false, nullptr,
     "遠くにこの時間以上続けていれば成り立つ（10 秒まで見られる）。", 0.0, 10.0, "s"},
};

Status TickFar(INodeContext& context, void*, void* user) noexcept {
    const BossPlayerWatch* watch = WatchOf(user);
    return Result(watch != nullptr && watch->distance >= context.ParamFloat(kStanceThreshold) &&
                  BossObserver::SecondsFar(*watch, context.ParamFloat(kStanceThreshold)) >=
                      context.ParamFloat(kStanceSeconds));
}

// ---- プレイヤーの残り：ヒョウタン・精力・HP・残りの回復の数が、全部の項目に合う。 ----

enum : uint32_t {
    kRemainGourd,
    kRemainGourdLow,
    kRemainStaminaMost,
    kRemainHealthLeast,
    kRemainHealthMost,
    kRemainHealsLeast,
    kRemainHealsMost
};
constexpr ParamDesc kRemainParams[] = {
    {"gourd", ValueType::Text, "ヒョウタン", 0.0, "any", false, "any|full|notFull|low",
     "any：問わない。full：満杯（もう吸い込めない）。notFull：満杯でない。low：煙が「少ない」以下（撃てる弾が少ない）。"},
    {"gourdLowPercent", ValueType::Float, "ヒョウタンが少ない（以下）", 25.0, nullptr, false, nullptr,
     "low のとき、煙の量が容量のこの割合以下なら少ないとみなす（煙弾 1 発は SmokeArena で容量の 20 %）。", 0.0, 100.0,
     "%"},
    {"staminaAtMostPercent", ValueType::Float, "精力（以下）", 100.0, nullptr, false, nullptr,
     "精力が最大のこの割合以下なら成り立つ（100 は問わない）。", 0.0, 100.0, "%"},
    {"healthAtLeastPercent", ValueType::Float, "HP（以上）", 0.0, nullptr, false, nullptr,
     "プレイヤーの HP が最大のこの割合以上。", 0.0, 100.0, "%"},
    {"healthAtMostPercent", ValueType::Float, "HP（以下）", 100.0, nullptr, false, nullptr, "…この割合以下。", 0.0,
     100.0, "%"},
    {"healsAtLeast", ValueType::Int, "回復の残り（以上）", 0.0, nullptr, false, nullptr, "残りの回復の数がこれ以上。",
     0.0, 99.0},
    {"healsAtMost", ValueType::Int, "回復の残り（以下）", 99.0, nullptr, false, nullptr, "…これ以下。", 0.0, 99.0},
};

bool GourdMatches(std::string_view wanted, const BossPlayerWatch& watch, float lowPercent) {
    if (wanted == "any")
        return true;
    if (watch.gourdPercent < 0.0f)
        return false;
    if (wanted == "full")
        return watch.gourdFull;
    if (wanted == "notFull")
        return !watch.gourdFull;
    return watch.gourdPercent <= lowPercent;
}

Status TickRemaining(INodeContext& context, void*, void* user) noexcept {
    const BossPlayerWatch* watch = WatchOf(user);
    if (watch == nullptr || !GourdMatches(context.ParamText(kRemainGourd), *watch, context.ParamFloat(kRemainGourdLow)))
        return Status::Failure;
    return Result(watch->staminaPercent <= context.ParamFloat(kRemainStaminaMost) &&
                  watch->healthPercent >= context.ParamFloat(kRemainHealthLeast) &&
                  watch->healthPercent <= context.ParamFloat(kRemainHealthMost) &&
                  watch->healCharges >= context.ParamInt(kRemainHealsLeast) &&
                  watch->healCharges <= context.ParamInt(kRemainHealsMost));
}

// ---- 霞歩でかわせる：続けてかわせる回数を使い切った後の、かわさない間でない（c-dbd81e）。 ----

Status TickStepReady(INodeContext&, void*, void* user) noexcept {
    const BossThought* thought = BossBehaviorNodes::ThoughtOf(user);
    return Result(thought != nullptr && BossKasumiStep::CanDodge(thought->boss));
}

NodeTypeDesc Condition(const char* typeName, const char* displayName, const char* description,
                       Status (*tick)(INodeContext&, void*, void*) noexcept, void* user) {
    NodeTypeDesc desc{};
    desc.kind = NodeKind::Condition;
    desc.typeName = typeName;
    desc.displayName = displayName;
    desc.category = kCategory;
    desc.description = description;
    desc.tick = tick;
    desc.user = user;
    return desc;
}

template <size_t N>
void SetParams(NodeTypeDesc& desc, const ParamDesc (&params)[N]) {
    desc.params = params;
    desc.paramCount = static_cast<uint32_t>(N);
}

// 射撃・ローリングの癖・咎められた技・立ち位置・プレイヤーの残りの条件（c-59bd08）。どれも BossObserver の観察を読むだけ。
bool RegisterWatch(NodeRegistration& registration, void* user) {
    NodeTypeDesc shooting = Condition(
        "Project_TD2_2.BossPlayerShooting", "プレイヤーが撃とうとしている",
        "プレイヤーが撃つ構えに入ってから（押し続けの連射なら前の弾を撃ってから）「何 F 以内」で、次の弾が「続けて何発目"
        "以上」なら成功する。「ボスへ向いている時だけ」なら、狙いの線がボスの体を通る時だけ。構えを見て動く枝に使う。",
        &TickShooting, user);
    SetParams(shooting, kShootParams);
    NodeTypeDesc habit =
        Condition("Project_TD2_2.BossRollHabit", "ローリングの癖",
                  "最近の「最近の何回」のローリングのうち、「向き」だった割合が「割合（以上）」以上なら"
                  "成功する（数えた回数が「少なくとも何回」に満たない間は失敗）。",
                  &TickHabit, user);
    SetParams(habit, kHabitParams);
    NodeTypeDesc punished = Condition(
        "Project_TD2_2.BossMovePunished", "この技は咎められている",
        "「技」の後隙の間にプレイヤーに当てられた回数（戦いの間ずっと数える）が「回数（以上）」以上なら成功する。",
        &TickPunished, user);
    SetParams(punished, kPunishedParams);
    NodeTypeDesc behind = Condition("Project_TD2_2.BossPlayerBehind", "背後にいる時間",
                                    "プレイヤーがボスの正面から「正面からの角度（以上）」以上の所に、「続けている時間」"
                                    "以上いれば成功する（ボスが振り向くと途切れる）。",
                                    &TickBehind, user);
    SetParams(behind, kBehindParams);
    NodeTypeDesc farAway = Condition(
        "Project_TD2_2.BossPlayerFar", "遠くにい続けている",
        "プレイヤーがボスから「距離（以上）」以上離れた所に、「続けている時間」以上いれば成功する。", &TickFar, user);
    SetParams(farAway, kFarParams);
    NodeTypeDesc remaining =
        Condition("Project_TD2_2.BossPlayerRemaining", "プレイヤーの残り",
                  "プレイヤーのヒョウタン（満杯・煙が少ない）・精力・HP・残りの回復の数が、全部の項目に合えば成功する。"
                  "問わない項目は既定のまま（any・100 %・0〜100 %・0〜99 回）にする。",
                  &TickRemaining, user);
    SetParams(remaining, kRemainParams);
    const NodeTypeDesc stepReady = Condition(
        "Project_TD2_2.BossStepReady", "霞歩でかわせる",
        "続けてかわせる回数（ボスの項目「続けてかわせる回数」）を使い切った後の、かわさない間（「使い切った後、"
        "かわさない間」秒）でなければ成功する。射撃をかわす枝の、反応の間隔の前に置く。かわさない間は胴の煙が"
        "薄くなる。",
        &TickStepReady, user);
    return registration.Register(shooting) && registration.Register(habit) && registration.Register(punished) &&
           registration.Register(behind) && registration.Register(farAway) && registration.Register(remaining) &&
           registration.Register(stepReady);
}

} // namespace

bool BossReactionNodes::Register(NodeRegistration& registration, void* user) {
    NodeTypeDesc absorbing = Condition("Project_TD2_2.BossPlayerAbsorbing", "プレイヤーが吸い込み中",
                                       "プレイヤーがヒョウタンの吸い込みを「続けている時間」以上続けていれば成功する。",
                                       &TickAbsorbing, user);
    SetParams(absorbing, kAbsorbParams);
    const NodeTypeDesc healing = Condition(
        "Project_TD2_2.BossPlayerHealing", "プレイヤーが回復中",
        "プレイヤーが回復の動作をしている間（使い始めてから動けるようになるまで）成功する。", &TickHealing, user);
    NodeTypeDesc inFire =
        Condition("Project_TD2_2.BossPlayerInFire", "プレイヤーが火の中",
                  "プレイヤーが燃えている地面の上に「続けている時間」以上いれば成功する。", &TickInFire, user);
    SetParams(inFire, kFireParams);
    NodeTypeDesc rolled = Condition("Project_TD2_2.BossPlayerRolled", "プレイヤーがローリングした直後",
                                    "プレイヤーが「何秒以内」にローリングを始めていて、その時の向き（ボスから見て）が"
                                    "「向き」に合えば成功する。",
                                    &TickRolled, user);
    SetParams(rolled, kRolledParams);
    NodeTypeDesc distance =
        Condition("Project_TD2_2.BossDistance", "距離", "プレイヤーまでの水平距離が最小以上・最大以下なら成功する。",
                  &TickDistance, user);
    SetParams(distance, kDistanceParams);
    NodeTypeDesc burning = Condition("Project_TD2_2.BossBurning", "燃えている面積",
                                     "闘技場の円のうち燃えている地面の割合が「以上」〜「以下」なら成功する"
                                     "（燃える地面がシーンに無ければ 0 %）。",
                                     &TickBurning, user);
    SetParams(burning, kBurningParams);
    NodeTypeDesc burningRoll = Condition(
        "Project_TD2_2.BossBurningRoll", "燃えている面積に比例した確率",
        "燃えている面積の割合 × 「1% あたりの確率」（「上限」まで）の確率で成功する。呼ばれるたびに振り直すので、"
        "「中立」→「次の技」の中（技を 1 つ選ぶたびに 1 回だけ呼ばれる所）に置く。根や「プレイヤーへの反応」のように"
        "毎 tick 試される所に置くと、何度も振ってすぐに当たってしまう。",
        &TickBurningRoll, user);
    SetParams(burningRoll, kBurningRollParams);
    // 反応の遅れの間は Running を返して待つので、条件ではなく動作の節（状態を持てる）にしてある。遅れが 0 なら、
    // 呼ばれたその回に成功か失敗を返す（条件の節と同じ）。
    NodeTypeDesc reaction = Condition(
        "Project_TD2_2.BossReaction", "反応の間隔",
        "プレイヤーへの反応の門。条件の後ろ・技を実行の前に置く。待機中（「後隙を切り上げる」なら後隙の間も）、"
        "間隔が過ぎていて確率に当たれば、反応の遅れ（最小〜最大 F）を待ってから成功する。待つ間は前の条件を読み直さない。"
        "すぐ後ろの技が始まると、そこから間隔を数える。",
        &TickReaction, user);
    reaction.kind = NodeKind::Action;
    reaction.stateSize = sizeof(ReactionState);
    reaction.progress = &ReactionProgress;
    SetParams(reaction, kReactionParams);
    return registration.Register(absorbing) && registration.Register(healing) && registration.Register(inFire) &&
           registration.Register(rolled) && registration.Register(distance) && registration.Register(burning) &&
           registration.Register(burningRoll) && registration.Register(reaction) && RegisterWatch(registration, user);
}

void BossReactionNodes::NoteStarted(BossThought& thought) {
    if (!thought.reactionArmed)
        return;
    thought.reactionArmed = false;
    if (BossReactionMemory* memory = MemoryOf(thought.boss, thought.reactionKey))
        memory->readySeconds = thought.boss.clockSeconds + thought.reactionCooldown;
}

void BossReactionNodes::NoteFailed(BossThought& thought) { thought.reactionArmed = false; }

} // namespace Project_TD2_2
