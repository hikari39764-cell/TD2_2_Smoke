#include "BossIntro.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "BossFrame.h"

namespace Project_TD2_2 {
namespace {

// 戦いの流れ（Flow）がボスを止めるより先に 1 tick 目が来ても、すぐには始めない。止められないまま、この tick を
// 過ぎたら（Flow の無い場面）始める。
constexpr uint64_t kHoldGraceTicks = 3;
// プレイヤーがこれより近い時は、向きを変えない（向きが決まらない）。
constexpr float kFaceMinDistance = 0.5f;

const char* PhaseName(BossIntroPhase phase) {
    switch (phase) {
    case BossIntroPhase::Scattered: return "Scattered";
    case BossIntroPhase::Gather: return "Gather";
    case BossIntroPhase::Assemble: return "Assemble";
    case BossIntroPhase::Draw: return "Draw";
    case BossIntroPhase::Flourish: return "Flourish";
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

} // namespace

int BossIntro::PhaseFrames(const BossComponent& boss, BossIntroPhase phase) {
    const BossIntroSettings& s = boss.intro;
    switch (phase) {
    case BossIntroPhase::Gather: return std::max(1, s.gatherFrames);
    case BossIntroPhase::Assemble: return std::max(1, s.assembleFrames);
    case BossIntroPhase::Draw: return std::max(1, s.drawFrames);
    case BossIntroPhase::Flourish: return std::max(1, s.flourishFrames);
    case BossIntroPhase::Settle: return std::max(0, s.settleFrames);
    default: return 0;
    }
}

void BossIntro::Begin(BossComponent& boss, bool seen) {
    BossIntroState& state = boss.introState;
    state = {};
    state.origin = boss.ground;
    state.yaw = boss.yaw;
    // 確かめ用に技を直に出す時（testMove・testSequence）は、登場をしない。
    const bool testing = !boss.testMove.empty() || boss.testSequence;
    const bool skip = seen && boss.intro.oncePerPlay;
    state.phase = boss.intro.enabled && !testing && !skip ? BossIntroPhase::Scattered : BossIntroPhase::Done;
    state.seenPhase = state.phase;
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

void BossIntro::Step(BossComponent& boss) {
    BossIntroState& state = boss.introState;
    switch (state.phase) {
    case BossIntroPhase::Done: return;
    case BossIntroPhase::Scattered:
        if (boss.intro.testStartFrames >= 0) {
            if (boss.tick >= static_cast<uint64_t>(boss.intro.testStartFrames))
                Enter(boss, BossIntroPhase::Gather);
            return;
        }
        if (boss.held)
            state.heldSeen = true;
        else if (state.heldSeen || boss.tick > kHoldGraceTicks)
            Enter(boss, BossIntroPhase::Gather);
        return;
    default: break;
    }
    if (++state.frame < PhaseFrames(boss, state.phase))
        return;
    Enter(boss, static_cast<BossIntroPhase>(static_cast<int>(state.phase) + 1));
}

} // namespace Project_TD2_2
