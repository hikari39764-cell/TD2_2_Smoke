#include "BossReactionNodes.h"

#include <algorithm>
#include <cstdio>
#include <functional>
#include <iterator>
#include <string_view>

#include "BossFrame.h"

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
    {"direction", ValueType::Text, "向き", 0.0, "any", false, "any|away|toward|sideways",
     "any：どの向きでも。away：ボスから離れる向き（後ろへ下がる）。toward：ボスへ向かう向き。sideways：横。"
     "前後と横を分ける角度はボスの項目（ローリングの向き）。"},
};

bool DirectionMatches(std::string_view wanted, BossRollDirection seen) {
    if (wanted == "away")
        return seen == BossRollDirection::Away;
    if (wanted == "toward")
        return seen == BossRollDirection::Toward;
    if (wanted == "sideways")
        return seen == BossRollDirection::Sideways;
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

// ---- 反応の間隔：間隔が過ぎていて、確率に当たれば通す。通した後ろの技が始まったら、そこから間隔を数える。 ----

enum : uint32_t { kReactionLabel, kReactionCooldown, kReactionChance, kReactionReroll, kReactionCutRecovery };
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

Status TickReaction(INodeContext& context, void*, void* user) noexcept {
    BossThought* thought = BossBehaviorNodes::ThoughtOf(user);
    if (thought == nullptr)
        return Status::Failure;
    // 反応して技を出せるのは待機中（待ちを切り上げる）だけ。「後隙を切り上げる」節は後隙の間も。
    const bool recovery = thought->moment == BossBrainMoment::Recovery;
    if (thought->moment != BossBrainMoment::Idle && !(recovery && context.ParamBool(kReactionCutRecovery)))
        return Status::Failure;
    BossComponent& boss = thought->boss;
    const std::string_view label = context.ParamText(kReactionLabel);
    const size_t key = std::hash<std::string_view>{}(label);
    BossReactionMemory* memory = MemoryOf(boss, key);
    const float now = boss.clockSeconds;
    if (memory == nullptr || now < memory->readySeconds || now < memory->rerollSeconds)
        return Status::Failure;
    const float chance = context.ParamFloat(kReactionChance);
    const float roll = context.Random() * 100.0f;
    memory->rerollSeconds = now + std::max(0.0f, context.ParamFloat(kReactionReroll));
    const bool pass = roll < chance;
    char message[224]{};
    std::snprintf(message, sizeof(message), "Reaction tick=%llu branch=%.*s roll=%.2f chance=%.2f pass=%d from=%s",
                  static_cast<unsigned long long>(boss.tick), static_cast<int>(label.size()), label.data(),
                  static_cast<double>(roll), static_cast<double>(chance), static_cast<int>(pass),
                  recovery ? "recovery" : "idle");
    BossLog::Write(message);
    if (!pass)
        return Status::Failure;
    thought->reactionArmed = true;
    thought->reactionKey = key;
    thought->reactionCooldown = std::max(0.0f, context.ParamFloat(kReactionCooldown));
    BossBehaviorNodes::CopyText(thought->reactionLabel, sizeof(thought->reactionLabel), label);
    return Status::Success;
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
    NodeTypeDesc reaction = Condition(
        "Project_TD2_2.BossReaction", "反応の間隔",
        "プレイヤーへの反応の門。条件の後ろ・技を実行の前に置く。待機中（「後隙を切り上げる」なら後隙の間も）、"
        "間隔が過ぎていて確率に当たれば成功する。すぐ後ろの技が始まると、そこから間隔を数える。",
        &TickReaction, user);
    SetParams(reaction, kReactionParams);
    return registration.Register(absorbing) && registration.Register(healing) && registration.Register(inFire) &&
           registration.Register(rolled) && registration.Register(distance) && registration.Register(burning) &&
           registration.Register(reaction);
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
