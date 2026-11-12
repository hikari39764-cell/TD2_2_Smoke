#include "BossBehaviorNodes.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <functional>
#include <iterator>
#include <string_view>

#include "BossFrame.h"
#include "BossReactionNodes.h"
#include "BossRules.h"
#include "BossSelection.h"

namespace Project_TD2_2 {
namespace {

using namespace HIKARI::BEHAVIOR;

// 編集器の節の一覧での分類。
constexpr char kCategory[] = "ボス 霞武士";

BossThought* ThoughtOf(void* user) { return BossBehaviorNodes::ThoughtOf(user); }

// ---- 待ち（横歩き）：行動の間、最小〜最大 F のどれかだけ待つ。待つ間の横歩きは実行器の待機の動き。 ----

enum : uint32_t { kWaitMinFrames, kWaitMaxFrames };
constexpr ParamDesc kWaitParams[] = {
    {"minFrames", ValueType::Int, "待ち 最小", 20.0, nullptr, false, nullptr,
     "行動の間の待ちの短い方。1 F は 1/60 秒。", 0.0, 0.0, "F"},
    {"maxFrames", ValueType::Int, "待ち 最大", 110.0, nullptr, false, nullptr,
     "行動の間の待ちの長い方。待つたびに最小〜最大のどれかになる。", 0.0, 0.0, "F"},
};

struct WaitState {
    int32_t frames;
    int32_t elapsed;
};

void StartWait(INodeContext& context, void* state, void* user) noexcept {
    auto& wait = *static_cast<WaitState*>(state);
    const float low = static_cast<float>(std::max<int64_t>(0, context.ParamInt(kWaitMinFrames)));
    const float high = std::max(low, static_cast<float>(context.ParamInt(kWaitMaxFrames)));
    wait.frames = static_cast<int32_t>(std::floor(low + (high - low) * context.Random() + 0.5f));
    if (const BossThought* thought = ThoughtOf(user)) {
        char message[96]{};
        std::snprintf(message, sizeof(message), "Wait tick=%llu frames=%d",
                      static_cast<unsigned long long>(thought->boss.tick), wait.frames);
        BossLog::Write(message);
    }
}

Status TickWait(INodeContext&, void* state, void* user) noexcept {
    const BossThought* thought = ThoughtOf(user);
    if (thought == nullptr)
        return Status::Failure;
    auto& wait = *static_cast<WaitState*>(state);
    // 待ちは待機中の tick だけを数える（後隙の間に始まっても、待機に戻ってから数える）。
    if (thought->moment == BossBrainMoment::Idle)
        ++wait.elapsed;
    return wait.elapsed >= wait.frames ? Status::Success : Status::Running;
}

bool WaitProgress(INodeContext&, const void* state, void*, NodeProgress& out) noexcept {
    const auto& wait = *static_cast<const WaitState*>(state);
    if (wait.frames <= 0)
        return false;
    out.current = static_cast<float>(std::min(wait.elapsed, wait.frames));
    out.total = static_cast<float>(wait.frames);
    out.fraction = out.current / out.total;
    std::snprintf(out.text, sizeof(out.text), "%d / %d F", std::min(wait.elapsed, wait.frames), wait.frames);
    return true;
}

// ---- 連続攻撃された：seconds 秒以内に hits 回当てられた（待機中か後隙の間だけ成り立つ）。 ----

enum : uint32_t { kPressuredHits, kPressuredSeconds, kPressuredCooldown };
constexpr ParamDesc kPressuredParams[] = {
    {"hits", ValueType::Int, "連続攻撃とみなす命中数", 2.0, nullptr, false, nullptr,
     "この回数だけ当てられたら成り立つ。"},
    {"seconds", ValueType::Float, "…を数える時間", 4.0, nullptr, false, nullptr,
     "この時間のうちに当てられた命中を数える。", 0.0, 20.0, "s"},
    {"cooldown", ValueType::Float, "反応の間隔", 6.0, nullptr, false, nullptr,
     "割り込みの技を出した後、この時間は成り立たない。", 0.0, 60.0, "s"},
};

Status TickPressured(INodeContext& context, void*, void* user) noexcept {
    BossThought* thought = ThoughtOf(user);
    if (thought == nullptr ||
        (thought->moment != BossBrainMoment::Idle && thought->moment != BossBrainMoment::Recovery))
        return Status::Failure;
    if (!BossSelection::IsPressured(thought->boss, static_cast<int>(context.ParamInt(kPressuredHits)),
                                    context.ParamFloat(kPressuredSeconds)))
        return Status::Failure;
    thought->pressureArmed = true;
    thought->pressureCooldown = context.ParamFloat(kPressuredCooldown);
    return Status::Success;
}

// ---- フェーズ・HP：いまのフェーズが phaseAtLeast 以上で、HP が healthAtMost % 以下。 ----

enum : uint32_t { kSituationPhase, kSituationHealth };
constexpr ParamDesc kSituationParams[] = {
    {"phaseAtLeast", ValueType::Int, "フェーズ（以上）", 1.0, nullptr, false, nullptr,
     "いまのフェーズがこの数以上なら成り立つ。"},
    {"healthAtMost", ValueType::Float, "HP（以下）", 100.0, nullptr, false, nullptr,
     "ボスの HP がこの割合以下なら成り立つ。", 0.0, 100.0, "%"},
};

Status TickSituation(INodeContext& context, void*, void* user) noexcept {
    const BossThought* thought = ThoughtOf(user);
    if (thought == nullptr)
        return Status::Failure;
    return thought->situation.phase >= context.ParamInt(kSituationPhase) &&
                   thought->situation.healthPercent <= static_cast<double>(context.ParamFloat(kSituationHealth))
               ? Status::Success
               : Status::Failure;
}

// ---- 分身を出せる：霞分身の再使用の時間が過ぎていて、分身が 1 体も出ていない。 ----

Status TickCloneReady(INodeContext&, void*, void* user) noexcept {
    const BossThought* thought = ThoughtOf(user);
    if (thought == nullptr)
        return Status::Failure;
    const BossComponent& boss = thought->boss;
    return boss.clockSeconds >= boss.brain.cloneReadySeconds && boss.activeClones == 0 && !boss.cloneSpawnPending
               ? Status::Success
               : Status::Failure;
}

// ---- 分身を撃たれた：プレイヤーが分身を撃った後、本体がまだ突進で来ていない（待機中か後隙の間だけ成り立つ）。 ----

Status TickCloneStruck(INodeContext&, void*, void* user) noexcept {
    const BossThought* thought = ThoughtOf(user);
    if (thought == nullptr ||
        (thought->moment != BossBrainMoment::Idle && thought->moment != BossBrainMoment::Recovery))
        return Status::Failure;
    return thought->boss.brain.cloneStruck ? Status::Success : Status::Failure;
}

// ---- 技を実行：表の 1 行を出し、終わるまで走る。コンボは表（コンボ先・コンボ確率）のとおりにこの節がつなぐ。 ----
// 親の Selector の order が random のとき、score（重み）に比例した確率で選ばれる。

enum : uint32_t { kMoveName, kMovePick };
constexpr ParamDesc kMoveParams[] = {
    {"move", ValueType::Text, "技（表の行の名前）", 0.0, "", false, nullptr,
     "技の表（Data/Boss の CSV）の「ボス行動」の名前をそのまま書く。"},
    {"pick", ValueType::Text, "選び方", 0.0, "neutral", false, "neutral|interrupt|always",
     "neutral：中立の技選び（距離重みで重みを付ける）。interrupt：連続攻撃への割り込み。"
     "always：重みを付けず、発生条件を満たせば出す。"},
};

enum class PickMode { Neutral, Interrupt, Always };

PickMode ModeOf(std::string_view text) {
    return text == "interrupt" ? PickMode::Interrupt : text == "always" ? PickMode::Always : PickMode::Neutral;
}

const char* ModeName(PickMode mode) {
    return mode == PickMode::Interrupt ? "interrupt" : mode == PickMode::Always ? "always" : "neutral";
}

float WeightOf(PickMode mode, const BossThought& thought, int row) {
    switch (mode) {
    case PickMode::Neutral: return BossSelection::NeutralWeight(thought.boss, row, thought.situation);
    case PickMode::Interrupt:
        return BossSelection::InterruptWeight(thought.boss, row, thought.situation,
                                              thought.moment == BossBrainMoment::Recovery);
    default: return BossSelection::CanPerform(thought.boss, row, thought.situation) ? 1.0f : 0.0f;
    }
}

// 節に書いた名前の行。表に無い名前は、名前ごとに一度だけ知らせる。
int RowOf(BossComponent& boss, std::string_view name) {
    const int row = BossRules::FindMove(boss, name);
    if (row >= 0 || !boss.tableLoaded)
        return row;
    BossBrainMemory& memory = boss.brain;
    const size_t key = std::hash<std::string_view>{}(name);
    const auto known = memory.unknownNames.begin() + static_cast<ptrdiff_t>(memory.unknownCount);
    if (std::find(memory.unknownNames.begin(), known, key) != known ||
        memory.unknownCount >= memory.unknownNames.size())
        return row;
    memory.unknownNames[memory.unknownCount++] = key;
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "行動ツリーの節が技「%.*s」を出そうとしましたが、技の表にその名前の行がありません",
                  static_cast<int>(name.size()), name.data());
    BossLog::Warn(message);
    return row;
}

// 表では始動技なのに、いま選んだ技選びに節が無い行を知らせる（表を読むたびに一度）。
void WarnUncovered(BossThought& thought) {
    BossComponent& boss = thought.boss;
    if (boss.brain.coverageChecked)
        return;
    boss.brain.coverageChecked = true;
    for (int row = 0; row < std::min(64, BossRules::GetMoveCount(boss)); ++row) {
        const BossMove* move = BossRules::GetMove(boss, row);
        if (move->use != BossMoveUse::Start || ((boss.brain.scoredRows >> row) & 1u) != 0u ||
            !BossSelection::CanPerform(boss, row, thought.situation))
            continue;
        char message[256]{};
        std::snprintf(message, sizeof(message),
                      "技「%s」は表では始動技ですが、行動ツリーの技選びに節が無いので選ばれません", move->name.c_str());
        BossLog::Warn(message);
    }
}

struct MoveState {
    // 何連目か（0 が始動）。
    int32_t chain;
    float fraction;
    bool requested;
    // コンボでつないだ技の名前（編集器に出す）。
    char label[32];
};

// 頼めた時は true。頼めなかった時は、すぐ前の「反応の間隔」が通った分を取り消す（ほかの枝の技を反応として数えない）。
bool RequestRow(BossThought& thought, PickMode mode, int row, bool& continues, float& weight) {
    BossComponent& boss = thought.boss;
    bool requested = false;
    // 攻撃中が終わった tick は、走っている節がコンボを決める。新しく始まる節は技を出せない。
    if (row >= 0 && thought.moment != BossBrainMoment::ActFinished) {
        weight = WeightOf(mode, thought, row);
        // 後隙を切り上げて出す技が、いまの技のコンボ先なら、コンボの続きとして数える。
        continues = thought.moment == BossBrainMoment::Recovery && BossSelection::IsComboOfCurrent(boss, row);
        requested = weight > 0.0f && BossRules::RequestMove(boss, row, continues);
    }
    if (!requested)
        BossReactionNodes::NoteFailed(thought);
    return requested;
}

Status RequestFirst(INodeContext& context, MoveState& run, BossThought& thought) {
    BossComponent& boss = thought.boss;
    const std::string_view name = context.ParamText(kMoveName);
    const PickMode mode = ModeOf(context.ParamText(kMovePick));
    bool continues = false;
    float weight = 0.0f;
    if (!RequestRow(thought, mode, RowOf(boss, name), continues, weight))
        return Status::Failure;
    run.requested = true;
    if (mode == PickMode::Neutral)
        WarnUncovered(thought);
    if (mode == PickMode::Interrupt && thought.pressureArmed) {
        BossSelection::UsePressure(boss, thought.pressureCooldown);
        thought.pressureArmed = false;
    }
    // 反応の枝の技は why=reaction と枝の名前（反応の間隔の「名前」）を出す。調整の時にどの枝が出たかを見る。
    const bool reaction = thought.reactionArmed;
    char branch[80]{};
    if (reaction)
        std::snprintf(branch, sizeof(branch), " branch=%s", thought.reactionLabel);
    BossReactionNodes::NoteStarted(thought);
    char message[384]{};
    std::snprintf(message, sizeof(message),
                  "Pick tick=%llu move=%.*s why=%s from=%s continues=%d dist=%.3f weight=%.5f last=%d moveHits=%d "
                  "phase=%d%s",
                  static_cast<unsigned long long>(boss.tick), static_cast<int>(name.size()), name.data(),
                  reaction ? "reaction" : ModeName(mode),
                  thought.moment == BossBrainMoment::Recovery ? "recovery" : "idle", static_cast<int>(continues),
                  static_cast<double>(thought.situation.distance), static_cast<double>(weight), boss.brain.lastStarted,
                  boss.brain.moveHits, thought.situation.phase, branch);
    BossLog::Write(message);
    return Status::Running;
}

Status TickMove(INodeContext& context, void* state, void* user) noexcept {
    BossThought* thought = ThoughtOf(user);
    if (thought == nullptr)
        return Status::Failure;
    BossComponent& boss = thought->boss;
    auto& run = *static_cast<MoveState*>(state);
    if (!run.requested)
        return RequestFirst(context, run, *thought);
    // 頼んだ技（とそのコンボ）が終わった。気絶などで打ち切られた時もここへ来る。
    if (BossRules::GetState(boss) != BossState::Act)
        return Status::Success;
    if (thought->moment == BossBrainMoment::ActFinished) {
        const float roll = context.Random();
        const int next = BossSelection::PickCombo(boss, thought->situation, roll, context.Random());
        if (next >= 0 && BossRules::RequestMove(boss, next, true)) {
            const BossMove* move = BossRules::GetMove(boss, next);
            ++run.chain;
            run.fraction = static_cast<float>(run.chain + 1) / static_cast<float>(std::max(1, boss.comboMaxChain));
            BossBehaviorNodes::CopyText(run.label, sizeof(run.label), move->name);
            char message[224]{};
            std::snprintf(message, sizeof(message), "Combo tick=%llu to=%s chain=%d roll=%.3f",
                          static_cast<unsigned long long>(boss.tick), move->name.c_str(), run.chain,
                          static_cast<double>(roll));
            BossLog::Write(message);
        }
        return Status::Running;
    }
    return thought->moment == BossBrainMoment::Recovery && thought->recoveryDone ? Status::Success : Status::Running;
}

float ScoreMove(INodeContext& context, void* user) noexcept {
    BossThought* thought = ThoughtOf(user);
    if (thought == nullptr)
        return 0.0f;
    const int row = RowOf(thought->boss, context.ParamText(kMoveName));
    if (row < 0)
        return 0.0f;
    const PickMode mode = ModeOf(context.ParamText(kMovePick));
    if (mode == PickMode::Neutral && row < 64)
        thought->boss.brain.scoredRows |= uint64_t{1} << row;
    return WeightOf(mode, *thought, row);
}

bool MoveProgress(INodeContext&, const void* state, void*, NodeProgress& out) noexcept {
    const auto& run = *static_cast<const MoveState*>(state);
    if (!run.requested || run.chain <= 0)
        return false;
    out.fraction = std::clamp(run.fraction, 0.0f, 1.0f);
    std::snprintf(out.text, sizeof(out.text), "コンボ %d: ", run.chain);
    const size_t used = std::strlen(out.text);
    BossBehaviorNodes::CopyText(out.text + used, sizeof(out.text) - used, run.label);
    return true;
}

} // namespace

void BossBehaviorNodes::CopyText(char* target, size_t capacity, std::string_view text) {
    size_t length = std::min(text.size(), capacity - 1);
    while (length > 0 && length < text.size() && (static_cast<unsigned char>(text[length]) & 0xC0u) == 0x80u)
        --length;
    std::memcpy(target, text.data(), length);
    target[length] = '\0';
}

std::shared_ptr<BossBehaviorNodes> BossBehaviorNodes::Acquire() {
    // 節の種類の名前はプロセスで 1 つしか登録できないので、システムが何個あっても 1 つの登録を分け合う。
    static std::weak_ptr<BossBehaviorNodes> shared;
    std::shared_ptr<BossBehaviorNodes> nodes = shared.lock();
    if (nodes == nullptr) {
        nodes = std::shared_ptr<BossBehaviorNodes>(new BossBehaviorNodes());
        if (!nodes->Register())
            return nullptr;
        shared = nodes;
    }
    return nodes;
}

bool BossBehaviorNodes::Register() {
    api_ = FindBehaviorApi();
    if (api_ == nullptr)
        return false;

    NodeTypeDesc wait{};
    wait.kind = NodeKind::Action;
    wait.typeName = "Project_TD2_2.BossWait";
    wait.displayName = "待ち（横歩き）";
    wait.category = kCategory;
    wait.description = "行動の間の待ち。最小〜最大 F のどれかだけ待って成功する。待つ間、ボスはプレイヤーを見たまま"
                       "横に歩く（歩く速さはボスの「移動の速さ」）。";
    wait.params = kWaitParams;
    wait.paramCount = static_cast<uint32_t>(std::size(kWaitParams));
    wait.stateSize = sizeof(WaitState);
    wait.start = &StartWait;
    wait.tick = &TickWait;
    wait.progress = &WaitProgress;
    wait.user = this;

    NodeTypeDesc move{};
    move.kind = NodeKind::Action;
    move.typeName = "Project_TD2_2.BossMove";
    move.displayName = "技を実行";
    move.category = kCategory;
    move.description = "技の表の 1 行を出し、後隙が終わるまで走る。コンボ先があれば、表のコンボ確率でつなぐ。"
                       "出せない技（実行器がまだ動かせない・発生条件を満たさない）は失敗する。親の Selector の "
                       "Order が random なら、選び方に応じた重みに比例した確率で選ばれる。";
    move.params = kMoveParams;
    move.paramCount = static_cast<uint32_t>(std::size(kMoveParams));
    move.stateSize = sizeof(MoveState);
    move.tick = &TickMove;
    move.score = &ScoreMove;
    move.progress = &MoveProgress;
    move.user = this;

    NodeTypeDesc pressured{};
    pressured.kind = NodeKind::Condition;
    pressured.typeName = "Project_TD2_2.BossPressured";
    pressured.displayName = "連続攻撃された";
    pressured.category = kCategory;
    pressured.description = "決めた時間のうちに決めた回数だけ当てられていれば成功する（待機中と後隙の間だけ）。"
                            "この後ろで「技を実行（選び方 interrupt）」が始まると、反応の間隔が過ぎるまで成功しない。";
    pressured.params = kPressuredParams;
    pressured.paramCount = static_cast<uint32_t>(std::size(kPressuredParams));
    pressured.tick = &TickPressured;
    pressured.user = this;

    NodeTypeDesc situation{};
    situation.kind = NodeKind::Condition;
    situation.typeName = "Project_TD2_2.BossSituation";
    situation.displayName = "フェーズ・HP";
    situation.category = kCategory;
    situation.description = "いまのフェーズが「フェーズ（以上）」以上で、HP が「HP（以下）」以下なら成功する。"
                            "技ごとの発生条件は技の表の列が決めるので、ここは木の枝を切り替えるのに使う。";
    situation.params = kSituationParams;
    situation.paramCount = static_cast<uint32_t>(std::size(kSituationParams));
    situation.tick = &TickSituation;
    situation.user = this;

    NodeTypeDesc cloneReady{};
    cloneReady.kind = NodeKind::Condition;
    cloneReady.typeName = "Project_TD2_2.BossCloneReady";
    cloneReady.displayName = "分身を出せる";
    cloneReady.category = kCategory;
    cloneReady.description =
        "霞分身の再使用（ボスの「分身の再使用」秒）が過ぎていて、分身が 1 体も出ていなければ成功する。"
        "この後ろに「技を実行（霞分身、選び方 always）」を置く。HP の条件は技の表の発生条件。";
    cloneReady.tick = &TickCloneReady;
    cloneReady.user = this;

    NodeTypeDesc cloneStruck{};
    cloneStruck.kind = NodeKind::Condition;
    cloneStruck.typeName = "Project_TD2_2.BossCloneStruck";
    cloneStruck.displayName = "分身を撃たれた";
    cloneStruck.category = kCategory;
    cloneStruck.description = "プレイヤーが分身を撃った後、本体がまだ突進（瞬間突進の型）を出していなければ成功する"
                              "（待機中と後隙の間だけ）。この後ろに「技を実行（突進横なぎ、選び方 always）」を置く。";
    cloneStruck.tick = &TickCloneStruck;
    cloneStruck.user = this;

    return registration_.Register(wait) && registration_.Register(move) && registration_.Register(pressured) &&
           registration_.Register(situation) && registration_.Register(cloneReady) &&
           registration_.Register(cloneStruck) && BossReactionNodes::Register(registration_, this);
}

TickOutcome BossBehaviorNodes::Think(BossThought& thought, HIKARI::GameProjectObjectHandle object, float deltaSeconds) {
    thought_ = &thought;
    const TickOutcome outcome = api_->tickObject(object, deltaSeconds, nullptr);
    thought_ = nullptr;
    return outcome;
}

void BossBehaviorNodes::CopyLastError(char* buffer, uint32_t capacity) const {
    if (capacity > 0 && api_->copyLastError(buffer, capacity) == 0)
        buffer[0] = '\0';
}

} // namespace Project_TD2_2
