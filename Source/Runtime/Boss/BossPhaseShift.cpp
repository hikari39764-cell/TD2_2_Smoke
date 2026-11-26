#include "BossPhaseShift.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "BossFrame.h"
#include "BossMath.h"
#include "BossPose.h"
#include "../Player/Player.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// プレイヤーがこれより近い時は、向きが決まらないので、ボスの前の向きを使う (m)。
constexpr float kFaceMinDistance = 0.5f;
// 崩の始めに、よろめきながらプレイヤーの側へ向き直り終える所（崩の中の割合）。
constexpr float kTurnShare = 0.5f;
// これより短い押し出しはしない (m)。
constexpr float kMinPush = 0.05f;

const char* BeatName(BossShiftBeat beat) {
    switch (beat) {
    case BossShiftBeat::None: return "None";
    case BossShiftBeat::Collapse: return "Collapse";
    case BossShiftBeat::Ignite: return "Ignite";
    case BossShiftBeat::Burst: return "Burst";
    case BossShiftBeat::Settle: return "Settle";
    case BossShiftBeat::Done: return "Done";
    }
    return "Unknown";
}

void Log(const BossComponent& boss, const BossPhaseShiftComponent& shift, const char* what, const char* extra) {
    char message[256]{};
    std::snprintf(message, sizeof(message), "Shift%s tick=%llu beat=%s %s", what,
                  static_cast<unsigned long long>(boss.tick), BeatName(shift.state.beat), extra);
    BossLog::Write(message);
}

// 演出の後：フェーズ 2 の見た目を出し、変わり目を次の tick で終える（BossHealthBars::Step）。技を選ばない間を数え始める。
void Finish(BossComponent& boss, BossPhaseShiftComponent& shift) {
    BossPhaseShiftState& state = shift.state;
    state.beat = BossShiftBeat::Done;
    state.frame = 0;
    boss.phaseTwoLookPending = false;
    state.calmLeft = std::max(0, shift.settings.calmFrames);
    boss.phaseShiftLeft = std::min(boss.phaseShiftLeft, 1);
}

void Enter(BossComponent& boss, BossPhaseShiftComponent& shift, BossShiftBeat beat) {
    BossPhaseShiftState& state = shift.state;
    state.beat = beat;
    state.frame = 0;
    // 構えに収まり始めた時から、HP の条が満ちる（Flow.bossRefillSeconds。Boss.PhaseSeconds で測る）。
    if (beat == BossShiftBeat::Settle)
        boss.phaseStartSeconds = boss.clockSeconds;
    if (beat == BossShiftBeat::Done)
        Finish(boss, shift);
    char extra[64]{};
    std::snprintf(extra, sizeof(extra), "frames=%d", BossPhaseShift::BeatFrames(shift, beat));
    Log(boss, shift, "Beat", extra);
}

} // namespace

int BossPhaseShift::BeatFrames(const BossPhaseShiftComponent& shift, BossShiftBeat beat) {
    const BossPhaseShiftSettings& s = shift.settings;
    switch (beat) {
    case BossShiftBeat::Collapse: return std::max(1, s.collapseFrames);
    case BossShiftBeat::Ignite: return std::max(1, s.igniteFrames);
    case BossShiftBeat::Burst: return std::max(1, s.burstFrames);
    case BossShiftBeat::Settle: return std::max(1, s.settleFrames);
    default: return 0;
    }
}

int BossPhaseShift::FramesBefore(const BossPhaseShiftComponent& shift, BossShiftBeat beat) {
    int frames = 0;
    for (int b = static_cast<int>(BossShiftBeat::Collapse); b < static_cast<int>(beat); ++b)
        frames += BeatFrames(shift, static_cast<BossShiftBeat>(b));
    return frames;
}

int BossPhaseShift::TotalFrames(const BossComponent& boss, const BossPhaseShiftComponent* shift) {
    return shift != nullptr && shift->settings.enabled ? FramesBefore(*shift, BossShiftBeat::Done)
                                                       : std::max(1, boss.phaseShiftFrames);
}

void BossPhaseShift::Begin(BossComponent& boss, BossPhaseShiftComponent* shift, HIKARI::GameProjectFloat3 playerCenter,
                           bool hasPlayer, float rootBefore) {
    // 演出が無い・切った時は、フェーズ 2 の見た目をすぐ出す（待たない）。
    boss.phaseTwoLookPending = false;
    if (shift == nullptr)
        return;
    BossPhaseShiftState& state = shift->state;
    state = {};
    if (!shift->settings.enabled)
        return;
    state.origin = boss.ground;
    state.startYaw = boss.yaw;
    state.yaw = boss.yaw;
    const Vec3 toPlayer = Sub(playerCenter, boss.ground);
    if (hasPlayer && FlatLength(toPlayer) >= kFaceMinDistance)
        state.yaw = YawOf(toPlayer);
    state.fallHeight = std::max(0.0f, rootBefore - BossPose::RootHeight(boss));
    boss.phaseTwoLookPending = true;
    // HP の条は、構えに収まり始めるまで空のまま（Enter(Settle) で満ち始める）。
    boss.phaseStartSeconds =
        boss.clockSeconds + static_cast<float>(FramesBefore(*shift, BossShiftBeat::Settle)) / kFramesPerSecond;
    Enter(boss, *shift, BossShiftBeat::Collapse);
    char extra[128]{};
    std::snprintf(extra, sizeof(extra), "yaw=%.3f startYaw=%.3f fall=%.3f total=%d", static_cast<double>(state.yaw),
                  static_cast<double>(state.startYaw), static_cast<double>(state.fallHeight), TotalFrames(boss, shift));
    Log(boss, *shift, "Begin", extra);
}

void BossPhaseShift::Step(BossComponent& boss, BossPhaseShiftComponent* shift) {
    if (shift == nullptr)
        return;
    BossPhaseShiftState& state = shift->state;
    if (!Cinematic(shift)) {
        state.skipRequested = false;
        if (state.calmLeft > 0)
            --state.calmLeft;
        return;
    }
    const BossPhaseShiftSettings& s = shift->settings;
    // 飛ばす（押されたのは描画のフレームなので、次の tick に進める）：フェーズ 2 の構えへ一度に進み、条はいまから満ちる。
    if (state.skipRequested) {
        state.skipRequested = false;
        state.skipped = true;
        state.flashed = true;
        boss.phaseStartSeconds = boss.clockSeconds;
        boss.yaw = state.yaw;
        Log(boss, *shift, "Skip", "");
        Finish(boss, *shift);
        return;
    }
    ++state.frame;
    const int frames = BeatFrames(*shift, state.beat);
    // 崩：よろめきながら、プレイヤーの側へ向き直る。
    if (state.beat == BossShiftBeat::Collapse) {
        const float turn = Smooth(static_cast<float>(state.frame) / (static_cast<float>(frames) * kTurnShare));
        boss.yaw = state.startYaw + WrapPi(state.yaw - state.startYaw) * std::clamp(turn, 0.0f, 1.0f);
    }
    // 爆：刀が地面を離れる tick に閃光（フェーズ 2 の見た目へ切り替え、プレイヤーを押し出す）。
    if (state.beat == BossShiftBeat::Burst && !state.flashed &&
        state.frame >=
            static_cast<int>(std::lround(std::clamp(s.drawShare, 0.0f, 1.0f) * static_cast<float>(frames)))) {
        state.flashed = true;
        boss.phaseTwoLookPending = false;
        state.pushPending = true;
        Log(boss, *shift, "Flash", "");
    }
    if (state.frame >= frames)
        Enter(boss, *shift, static_cast<BossShiftBeat>(static_cast<int>(state.beat) + 1));
}

void BossPhaseShift::Push(BossComponent& boss, BossPhaseShiftComponent* shift, Player& player,
                          HIKARI::GameProjectFloat3 playerCenter) {
    if (shift == nullptr)
        return;
    BossPhaseShiftState& state = shift->state;
    if (!state.pushPending)
        return;
    state.pushPending = false;
    const BossPhaseShiftSettings& s = shift->settings;
    Vec3 away = Sub(playerCenter, boss.ground);
    const float distance = FlatLength(away);
    away = FlatNormalize(away, Forward(state.yaw));
    float push = std::clamp(s.pushToDistance - distance, 0.0f, s.pushMaxDistance);
    // 場の縁の内側で止める：押した先が arenaRadius - pushArenaMargin の円を出るなら、円に届く所までにする。
    const float limit = std::max(0.0f, boss.arenaRadius - s.pushArenaMargin);
    const Vec3 from{playerCenter.x - boss.arenaCenter.x, 0.0f, playerCenter.z - boss.arenaCenter.z};
    // |from + away·d| = limit を d について解く（大きい方の根）。
    const float b = Dot(from, away);
    const float c = Dot(from, from) - limit * limit;
    const float disc = b * b - c;
    const float room = disc > 0.0f ? std::max(0.0f, -b + std::sqrt(disc)) : 0.0f;
    push = std::min(push, room);
    char extra[160]{};
    std::snprintf(extra, sizeof(extra), "distance=%.3f push=%.3f room=%.3f dir=(%.3f,%.3f)",
                  static_cast<double>(distance), static_cast<double>(push), static_cast<double>(room),
                  static_cast<double>(away.x), static_cast<double>(away.z));
    Log(boss, *shift, "Push", extra);
    if (push < kMinPush)
        return;
    PlayerRules::AddRecoil(player, away, push, std::max(1, s.pushFrames), s.pushEasePower);
}

} // namespace Project_TD2_2
