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

void Enter(const BossComponent& boss, BossIntroComponent& intro, BossIntroPhase phase) {
    intro.state.phase = phase;
    intro.state.frame = 0;
    intro.state.started = true;
    char message[128]{};
    std::snprintf(message, sizeof(message), "Intro tick=%llu phase=%s frames=%d",
                  static_cast<unsigned long long>(boss.tick), PhaseName(phase), BossIntro::PhaseFrames(intro, phase));
    BossLog::Write(message);
}

// 流している演出の資産を止める（流れ終わった・流していない時は何もしない）。
void StopSequence(HIKARI::WorldView& world, BossIntroState& state) {
    if (state.sequence.IsValid() && world.GetSequence().IsActive(state.sequence))
        world.GetSequence().Stop(state.sequence);
}

// 飛ばす：組み上がった待機の構えへ一度に進む（途中の状態で止まらない）。演出の資産も止める。
void Skip(HIKARI::WorldView& world, const BossComponent& boss, BossIntroComponent& intro) {
    BossIntroState& state = intro.state;
    char message[128]{};
    std::snprintf(message, sizeof(message), "Intro skip tick=%llu from=%s", static_cast<unsigned long long>(boss.tick),
                  PhaseName(state.phase));
    BossLog::Write(message);
    state.skipped = true;
    StopSequence(world, state);
    Enter(boss, intro, BossIntroPhase::Done);
}

// 演技を始める：演出の資産を流し（時計はその資産の時刻）、事件段を読む。断られたら、登場をやめて構えて立つ。
void StartShow(HIKARI::WorldView& world, const BossComponent& boss, BossIntroComponent& intro) {
    const BossIntroSettings& s = intro.settings;
    BossIntroState& state = intro.state;
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
        Enter(boss, intro, BossIntroPhase::Done);
        return;
    }
    state.showTicks = std::max(1, static_cast<int>(std::ceil((snapshot.durationSeconds + kShowMarginSeconds) *
                                                             kTicksPerSecond / options.playbackRate)));
    Enter(boss, intro, BossIntroPhase::Show);
    BossIntro::ReadCues(world, state.sequence, state);
    char message[128]{};
    std::snprintf(message, sizeof(message), "IntroSequence handle=%llu cues=%d seconds=%.2f",
                  static_cast<unsigned long long>(state.sequence.value), state.cueCount,
                  static_cast<double>(snapshot.durationSeconds));
    BossLog::Write(message);
}

} // namespace

bool BossIntro::BarShown(const BossIntroComponent* intro) {
    // HP の条は、演技が済んでから（戦いの体に替わった時から）出す。
    return !Cinematic(intro) || intro->state.phase == BossIntroPhase::Settle;
}

int BossIntro::PhaseFrames(const BossIntroComponent& intro, BossIntroPhase phase) {
    switch (phase) {
    case BossIntroPhase::Show: return std::max(1, intro.state.showTicks);
    case BossIntroPhase::Settle:
        return std::max(0, static_cast<int>(std::lround(intro.settings.settleSeconds * kTicksPerSecond)));
    default: return 0;
    }
}

HIKARI::GameProjectTransform BossIntro::StageTransform(const BossComponent& boss, const BossIntroComponent& intro) {
    const BossIntroState& state = intro.state;
    HIKARI::GameProjectTransform transform{};
    transform.position = Add(state.origin, Vec3{0.0f, boss.bodyHalfHeight, 0.0f});
    transform.rotation = YawRotation(state.yaw);
    transform.scale = {1.0f, 1.0f, 1.0f};
    return transform;
}

HIKARI::GameProjectTransform BossIntro::GroundTransform(const BossIntroComponent& intro) {
    HIKARI::GameProjectTransform transform{};
    transform.position = intro.state.origin;
    transform.rotation = YawRotation(intro.state.yaw);
    transform.scale = {1.0f, 1.0f, 1.0f};
    return transform;
}

void BossIntro::Begin(const BossComponent& boss, BossIntroComponent& intro, bool seen) {
    BossIntroState& state = intro.state;
    state = {};
    state.origin = boss.ground;
    state.yaw = boss.yaw;
    state.cues.assign(HIKARI::kGameProjectSequenceMaximumEventClips, {});
    // 確かめ用に技を直に出す時（testMove・testSequence）は、登場をしない。
    const bool testing = !boss.testMove.empty() || boss.testSequence;
    const bool skip = seen && intro.settings.oncePerPlay;
    state.phase = intro.settings.enabled && !testing && !skip ? BossIntroPhase::Scattered : BossIntroPhase::Done;
    state.seenPhase = state.phase;
}

void BossIntro::Cancel(BossIntroComponent& intro) { intro.state.phase = intro.state.seenPhase = BossIntroPhase::Done; }

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

void BossIntro::Face(BossComponent& boss, BossIntroComponent& intro, HIKARI::GameProjectFloat3 playerPosition) {
    BossIntroState& state = intro.state;
    if (state.phase != BossIntroPhase::Scattered || state.faced)
        return;
    state.faced = true;
    const float x = playerPosition.x - state.origin.x, z = playerPosition.z - state.origin.z;
    if (x * x + z * z < kFaceMinDistance * kFaceMinDistance)
        return;
    state.yaw = std::atan2(x, z);
    boss.yaw = state.yaw;
}

void BossIntro::Step(HIKARI::WorldView& world, const BossComponent& boss, BossIntroComponent& intro) {
    BossIntroState& state = intro.state;
    if (state.phase == BossIntroPhase::Done)
        return;
    // 飛ばす（演出の間だけ。押されたのは描画のフレームなので、次の tick に進める）。
    if (state.skipRequested && Cinematic(&intro)) {
        state.skipRequested = false;
        Skip(world, boss, intro);
        return;
    }
    switch (state.phase) {
    case BossIntroPhase::Scattered:
        if (intro.settings.testStartFrames >= 0) {
            if (boss.tick >= static_cast<uint64_t>(intro.settings.testStartFrames))
                StartShow(world, boss, intro);
            return;
        }
        if (boss.held)
            state.heldSeen = true;
        else if (state.heldSeen || boss.tick > kHoldGraceTicks)
            StartShow(world, boss, intro);
        return;
    case BossIntroPhase::Show: {
        // 演出の資産が終わりに着く少し手前で（見落とした時は、長さの tick が尽きたら）、戦いの体に替えて一息の段へ。
        HIKARI::GameProjectSequencePlaybackSnapshot snapshot{};
        const bool ended = !world.GetSequence().GetSnapshot(state.sequence, snapshot) ||
                           snapshot.state == HIKARI::GameProjectSequencePlaybackState::Stopped ||
                           snapshot.timeSeconds >= snapshot.durationSeconds - kShowEndLead;
        if (++state.frame >= PhaseFrames(intro, BossIntroPhase::Show) || ended) {
            // 一息の段へ入る時に、演出の資産を止めてカメラを放す：資産は、流れ終わって待っている間も、最後のカットの
            // カメラを握ったままでいる。放すと、ゲームのカメラが出している「演出の姿勢からゲームのカメラへ混ぜた姿勢」
            // （BossIntroCamera）が画に出る。演出の灯は資産が元へ戻すので、役者が出ている間は BossIntroActor が保つ。
            StopSequence(world, state);
            Enter(boss, intro, BossIntroPhase::Settle);
        }
        return;
    }
    default: break;
    }
    if (++state.frame < PhaseFrames(intro, state.phase))
        return;
    // 構えに収まった：演出の資産がまだ流れていれば止める（戦いの間にカメラを取らない）。
    StopSequence(world, state);
    Enter(boss, intro, BossIntroPhase::Done);
}

} // namespace Project_TD2_2
