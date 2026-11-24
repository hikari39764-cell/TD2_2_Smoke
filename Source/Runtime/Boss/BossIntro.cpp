#include "BossIntro.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/SequenceView.h"
#include "BossFrame.h"
#include "BossMath.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 固定 tick の数（1 秒あたり）。演出の資産の秒を tick に直す。
constexpr float kTicksPerSecond = 60.0f;
// 戦いの流れ（Flow）がボスを止めるより先に 1 tick 目が来ても、すぐには始めない。止められないまま、この tick を
// 過ぎたら（Flow の無い場面）始める。
constexpr uint64_t kHoldGraceTicks = 3;
// プレイヤーがこれより近い時は、向きを変えない（向きが決まらない）。
constexpr float kFaceMinDistance = 0.5f;
// 演出の資産を流す時の優先度（ほかの演出より勝つ）。
constexpr int32_t kSequencePriority = 10;
// 演出の資産が流れ終わったのを見落としても、資産の長さからこの秒を過ぎたら演技を終える（時間の停止・スローの分の余り）。
constexpr float kShowMarginSeconds = 3.0f;
// 資産の時刻が、長さのこの秒手前まで来たら、一息の段へ入る（資産を止めて、カメラを受け取る）。終わりまで流すと、最後の
// カットが切れたフレームに、カメラがゲームのカメラへ一度切り替わってしまう（最後の姿で待たせても、カットはもう無い）。
constexpr float kShowEndLead = 0.1f;

const char* PhaseName(BossIntroPhase phase) {
    switch (phase) {
    case BossIntroPhase::Scattered: return "Scattered";
    case BossIntroPhase::Show: return "Show";
    case BossIntroPhase::Settle: return "Settle";
    case BossIntroPhase::Done: return "Done";
    }
    return "Unknown";
}

void Enter(BossComponent& boss, BossIntroPhase phase) {
    boss.introState.phase = phase;
    boss.introState.frame = 0;
    boss.introState.started = true;
    char message[128]{};
    std::snprintf(message, sizeof(message), "Intro tick=%llu phase=%s frames=%d",
                  static_cast<unsigned long long>(boss.tick), PhaseName(phase), BossIntro::PhaseFrames(boss, phase));
    BossLog::Write(message);
}

// 流している演出の資産を止める（流れ終わった・流していない時は何もしない）。
void StopSequence(HIKARI::WorldView& world, BossIntroState& state) {
    if (state.sequence.IsValid() && world.GetSequence().IsActive(state.sequence))
        world.GetSequence().Stop(state.sequence);
}

// 飛ばす：組み上がった待機の構えへ一度に進む（途中の状態で止まらない）。演出の資産も止める。
void Skip(HIKARI::WorldView& world, BossComponent& boss) {
    BossIntroState& state = boss.introState;
    char message[128]{};
    std::snprintf(message, sizeof(message), "Intro skip tick=%llu from=%s", static_cast<unsigned long long>(boss.tick),
                  PhaseName(state.phase));
    BossLog::Write(message);
    state.skipped = true;
    StopSequence(world, state);
    Enter(boss, BossIntroPhase::Done);
}

// 演技を始める：演出の資産を流し（時計はその資産の時刻）、事件段を読む。断られたら、登場をやめて構えて立つ。
void StartShow(HIKARI::WorldView& world, BossComponent& boss) {
    const BossIntroSettings& s = boss.intro;
    BossIntroState& state = boss.introState;
    state.sequence = {};
    HIKARI::GameProjectSequencePlayOptions options{};
    options.playbackRate = std::clamp(s.playbackRate, 0.01f, 8.0f);
    options.priority = kSequencePriority;
    // 流れ終わっても、最後のカメラの姿勢のまま待たせる（こちらが一息の段へ入る tick に止める）。待たせないと、流れ
    // 終わったフレームから一息の段へ入る tick までの間、カメラがゲームのカメラへ一度切り替わってしまう。
    options.holdAtEnd = 1u;
    if (!s.sequence.empty())
        state.sequence = world.GetSequence().Play(s.sequence, options);
    HIKARI::GameProjectSequencePlaybackSnapshot snapshot{};
    if (!state.sequence.IsValid() || !world.GetSequence().GetSnapshot(state.sequence, snapshot)) {
        char message[160]{};
        std::snprintf(message, sizeof(message), "[BossIntro] 演出の資産を流せません（%s）。登場をしません",
                      s.sequence.empty() ? "空" : s.sequence.c_str());
        BossLog::Warn(message);
        Enter(boss, BossIntroPhase::Done);
        return;
    }
    state.showTicks = std::max(1, static_cast<int>(std::ceil((snapshot.durationSeconds + kShowMarginSeconds) *
                                                             kTicksPerSecond / options.playbackRate)));
    Enter(boss, BossIntroPhase::Show);
    BossIntro::ReadCues(world, state.sequence, state);
    char message[128]{};
    std::snprintf(message, sizeof(message), "IntroSequence handle=%llu cues=%d seconds=%.2f",
                  static_cast<unsigned long long>(state.sequence.value), state.cueCount,
                  static_cast<double>(snapshot.durationSeconds));
    BossLog::Write(message);
}

} // namespace

bool BossIntro::BarShown(const BossComponent& boss) {
    // HP の条は、演技が済んでから（戦いの体に替わった時から）出す。
    return !Cinematic(boss) || boss.introState.phase == BossIntroPhase::Settle;
}

int BossIntro::PhaseFrames(const BossComponent& boss, BossIntroPhase phase) {
    switch (phase) {
    case BossIntroPhase::Show: return std::max(1, boss.introState.showTicks);
    case BossIntroPhase::Settle:
        return std::max(0, static_cast<int>(std::lround(boss.intro.settleSeconds * kTicksPerSecond)));
    default: return 0;
    }
}

HIKARI::GameProjectTransform BossIntro::StageTransform(const BossComponent& boss) {
    const BossIntroState& state = boss.introState;
    HIKARI::GameProjectTransform transform{};
    transform.position = Add(state.origin, Vec3{0.0f, boss.bodyHalfHeight, 0.0f});
    transform.rotation = YawLeanSway(state.yaw, 0.0f, 0.0f);
    transform.scale = {1.0f, 1.0f, 1.0f};
    return transform;
}

HIKARI::GameProjectTransform BossIntro::GroundTransform(const BossComponent& boss) {
    HIKARI::GameProjectTransform transform = StageTransform(boss);
    transform.position = boss.introState.origin;
    return transform;
}

void BossIntro::Begin(BossComponent& boss, bool seen) {
    BossIntroState& state = boss.introState;
    state = {};
    state.origin = boss.ground;
    state.yaw = boss.yaw;
    state.cues.assign(HIKARI::kGameProjectSequenceMaximumEventClips, {});
    // 確かめ用に技を直に出す時（testMove・testSequence）は、登場をしない。
    const bool testing = !boss.testMove.empty() || boss.testSequence;
    const bool skip = seen && boss.intro.oncePerPlay;
    state.phase = boss.intro.enabled && !testing && !skip ? BossIntroPhase::Scattered : BossIntroPhase::Done;
    state.seenPhase = state.phase;
}

void BossIntro::Cancel(BossComponent& boss) {
    boss.introState.phase = boss.introState.seenPhase = BossIntroPhase::Done;
}

void BossIntro::ReadCues(HIKARI::WorldView& world, HIKARI::GameProjectSequencePlaybackHandle handle,
                         BossIntroState& state) {
    std::array<HIKARI::GameProjectSequenceEventClip, HIKARI::kGameProjectSequenceMaximumEventClips> clips{};
    const uint32_t total = world.GetSequence().GetEventClips(handle, clips.data(), static_cast<uint32_t>(clips.size()));
    if (state.cues.size() < clips.size())
        state.cues.assign(clips.size(), {});
    const size_t count = std::min({static_cast<size_t>(total), clips.size(), state.cues.size()});
    for (size_t k = 0; k < count; ++k)
        state.cues[k] = {clips[k]};
    state.cueCount = static_cast<int>(count);
    state.cueRolesRead = false;
}

void BossIntro::Face(BossComponent& boss, HIKARI::GameProjectFloat3 playerPosition) {
    BossIntroState& state = boss.introState;
    if (state.phase != BossIntroPhase::Scattered || state.faced)
        return;
    state.faced = true;
    const float x = playerPosition.x - state.origin.x, z = playerPosition.z - state.origin.z;
    if (x * x + z * z < kFaceMinDistance * kFaceMinDistance)
        return;
    state.yaw = std::atan2(x, z);
    boss.yaw = state.yaw;
}

void BossIntro::Step(HIKARI::WorldView& world, BossComponent& boss) {
    BossIntroState& state = boss.introState;
    if (state.phase == BossIntroPhase::Done)
        return;
    // 飛ばす（演出の間だけ。押されたのは描画のフレームなので、次の tick に進める）。
    if (state.skipRequested && Cinematic(boss)) {
        state.skipRequested = false;
        Skip(world, boss);
        return;
    }
    switch (state.phase) {
    case BossIntroPhase::Scattered:
        if (boss.intro.testStartFrames >= 0) {
            if (boss.tick >= static_cast<uint64_t>(boss.intro.testStartFrames))
                StartShow(world, boss);
            return;
        }
        if (boss.held)
            state.heldSeen = true;
        else if (state.heldSeen || boss.tick > kHoldGraceTicks)
            StartShow(world, boss);
        return;
    case BossIntroPhase::Show: {
        // 演出の資産が終わりに着く少し手前で（見落とした時は、長さの tick が尽きたら）、戦いの体に替えて一息の段へ。
        HIKARI::GameProjectSequencePlaybackSnapshot snapshot{};
        const bool ended = !world.GetSequence().GetSnapshot(state.sequence, snapshot) ||
                           snapshot.state == HIKARI::GameProjectSequencePlaybackState::Stopped ||
                           snapshot.timeSeconds >= snapshot.durationSeconds - kShowEndLead;
        if (++state.frame >= PhaseFrames(boss, BossIntroPhase::Show) || ended) {
            // 一息の段へ入る時に、演出の資産を止めてカメラを放す：資産は、流れ終わって待っている間も、最後のカットの
            // カメラを握ったままでいる。放すと、ゲームのカメラが出している「演出の姿勢からゲームのカメラへ混ぜた姿勢」
            // （BossIntroCamera）が画に出る。演出の灯は資産が元へ戻すので、役者が出ている間は BossIntroActor が保つ。
            StopSequence(world, state);
            Enter(boss, BossIntroPhase::Settle);
        }
        return;
    }
    default: break;
    }
    if (++state.frame < PhaseFrames(boss, state.phase))
        return;
    // 構えに収まった：演出の資産がまだ流れていれば止める（戦いの間にカメラを取らない）。
    StopSequence(world, state);
    Enter(boss, BossIntroPhase::Done);
}

} // namespace Project_TD2_2
