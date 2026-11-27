#include "BossPhaseShift.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>

#include "HIKARI/Project/EnvironmentView.h"
#include "HIKARI/Project/SequenceView.h"
#include "BossFrame.h"
#include "BossMath.h"
#include "BossPose.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 固定 tick の数（1 秒あたり）。資産の秒を tick に直す。
constexpr float kTicksPerSecond = 60.0f;
constexpr float kTiny = 1e-4f;
// プレイヤーがこれより近い時は、向きが決まらないので、ボスの前の向きを使う (m)。
constexpr float kFaceMinDistance = 0.5f;
// 資産を流す時の優先度（ほかの演出より勝つ。登場と同じ）。
constexpr int32_t kSequencePriority = 10;
// 資産が流れ終わったのを見落としても、長さからこの秒を過ぎたら演技を終える（時間の停止の分の余り）。
constexpr float kShowMarginSeconds = 3.0f;
// 資産の時刻が長さのこの秒手前まで来たら、渡しの段へ入る（最後のカットが切れたフレームに、カメラがゲームのカメラへ
// 一度切り替わらないように。登場と同じ）。
constexpr float kShowEndLead = 0.1f;
// 変わり目の長さの上限に足す余り (秒)。
constexpr float kTotalMarginSeconds = 3.0f;
// 太陽がこの高さ（向きの Y）より低い時は、太陽で向きを決めない（夜・地平の下）。
constexpr float kSunMinY = 0.02f;

// 事件段の名前 → 拍。Anim は「Anim.<クリップ>」、ほかは「Shift.<拍>」。
constexpr std::array<std::string_view, kBossShiftCueCount> kCueNames{
    "",           "Shift.Hit",    "Shift.Fall",   "Shift.Leak",   "Shift.Reach",
    "Shift.Grab", "Shift.Ignite", "Shift.Field",  "Shift.Spread", "Shift.Eyes",
    "Shift.Fuse", "Shift.Wall",   "Shift.Charge", "Shift.Slash",  "Shift.Settle"};

const char* BeatName(BossShiftBeat beat) {
    switch (beat) {
    case BossShiftBeat::None: return "None";
    case BossShiftBeat::Begin: return "Begin";
    case BossShiftBeat::Show: return "Show";
    case BossShiftBeat::Settle: return "Settle";
    case BossShiftBeat::Done: return "Done";
    }
    return "Unknown";
}

void Log(const BossComponent& boss, const BossPhaseShiftComponent& shift, const char* what, const char* extra) {
    char message[320]{};
    std::snprintf(message, sizeof(message), "Shift%s tick=%llu beat=%s %s", what,
                  static_cast<unsigned long long>(boss.tick), BeatName(shift.state.beat), extra);
    BossLog::Write(message);
}

void StopSequence(HIKARI::WorldView& world, BossPhaseShiftState& state) {
    if (state.sequence.IsValid() && world.GetSequence().IsActive(state.sequence))
        world.GetSequence().Stop(state.sequence);
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
    shift.state.beat = beat;
    shift.state.frame = 0;
    if (beat == BossShiftBeat::Done)
        Finish(boss, shift);
    Log(boss, shift, "Beat", "");
}

// 一閃：フェーズ 2 の見た目へ切り替え、HP の条を満たし始める（Flow.bossRefillSeconds で満ちる）。
void Slash(BossComponent& boss, BossPhaseShiftComponent& shift) {
    BossPhaseShiftState& state = shift.state;
    if (state.slashed)
        return;
    state.slashed = true;
    boss.phaseTwoLookPending = false;
    boss.phaseStartSeconds = boss.clockSeconds;
    Log(boss, shift, "Slash", "");
}

// 舞台を決める：足元を場の縁から内側へ寄せ、太陽（無ければプレイヤーの側）で向きを決める。ボスをそこへ置き直す。
void ChooseStage(HIKARI::WorldView& world, BossComponent& boss, BossPhaseShiftComponent& shift) {
    const BossPhaseShiftSettings& s = shift.settings;
    BossPhaseShiftState& state = shift.state;
    Vec3 origin = boss.ground;
    const Vec3 off{origin.x - boss.arenaCenter.x, 0.0f, origin.z - boss.arenaCenter.z};
    const float radius = FlatLength(off);
    const float limit = std::max(0.0f, boss.arenaRadius - s.stageEdgeMargin);
    if (radius > limit && radius > kTiny) {
        origin.x = boss.arenaCenter.x + off.x * (limit / radius);
        origin.z = boss.arenaCenter.z + off.z * (limit / radius);
    }
    float yaw = state.yaw;
    const char* by = "player";
    if (s.faceSun) {
        HIKARI::GameProjectEnvironmentState environment{};
        if (world.GetEnvironment().GetState(environment).Succeeded() && environment.sunDirection.y > kSunMinY &&
            std::abs(environment.sunDirection.x) + std::abs(environment.sunDirection.z) > kTiny) {
            const float sunYaw = std::atan2(environment.sunDirection.x, environment.sunDirection.z);
            yaw = WrapPi(sunYaw - s.sunStageYawDegrees * kRadiansPerDegree);
            by = "sun";
            state.sunDirection = Normalize(environment.sunDirection);
            state.sunValid = true;
        }
    }
    const float moved = FlatLength(Sub(origin, boss.ground));
    state.origin = origin;
    state.yaw = yaw;
    boss.ground = origin;
    boss.yaw = yaw;
    // 体の補間（前の tick との間）をやめる：舞台へ置き直したのは切り替えなので、間を滑らせない。
    boss.ticksSeen = 0;
    if (s.placePlayer) {
        // プレイヤー：ボスの正面の playerDistance m。場の縁の内側（arenaRadius - playerArenaMargin）に収める。
        const Vec3 front = Forward(yaw);
        float distance = s.playerDistance;
        const float edge = std::max(0.0f, boss.arenaRadius - s.playerArenaMargin);
        const Vec3 from{origin.x - boss.arenaCenter.x, 0.0f, origin.z - boss.arenaCenter.z};
        const float b = Dot(from, front);
        const float c = Dot(from, from) - edge * edge;
        const float disc = b * b - c;
        if (disc > 0.0f)
            distance = std::min(distance, std::max(2.0f, -b + std::sqrt(disc)));
        state.placeAt = Add(origin, Add(Mul(front, distance), Vec3{0.0f, s.playerLift, 0.0f}));
        state.placePending = true;
    }
    char extra[200]{};
    std::snprintf(extra, sizeof(extra), "by=%s yaw=%.3f origin=(%.2f,%.2f,%.2f) moved=%.2f player=(%.2f,%.2f,%.2f)", by,
                  static_cast<double>(yaw), static_cast<double>(origin.x), static_cast<double>(origin.y),
                  static_cast<double>(origin.z), static_cast<double>(moved), static_cast<double>(state.placeAt.x),
                  static_cast<double>(state.placeAt.y), static_cast<double>(state.placeAt.z));
    Log(boss, shift, "Stage", extra);
}

// 演技を始める：資産を流し（カメラの舞台の印は骨格の体の物体）、事件段を読む。断られたら演出をやめて済みへ。
void StartShow(HIKARI::WorldView& world, BossComponent& boss, BossPhaseShiftComponent& shift) {
    const BossPhaseShiftSettings& s = shift.settings;
    BossPhaseShiftState& state = shift.state;
    state.sequence = {};
    HIKARI::GameProjectSequencePlayOptions options{};
    options.playbackRate = std::clamp(s.playbackRate, 0.01f, 8.0f);
    options.priority = kSequencePriority;
    options.holdAtEnd = 1u;
    HIKARI::GameProjectSequenceBinding binding{};
    const HIKARI::ObjectView body = world.FindSceneObject(boss.animBody.object);
    if (boss.animBody.object != 0u && body.IsValid() && !s.stageSlot.empty()) {
        std::strncpy(binding.slotName, s.stageSlot.c_str(), sizeof(binding.slotName) - 1);
        binding.object = body.GetHandle();
        options.bindings = &binding;
        options.bindingCount = 1u;
    }
    if (!s.sequence.empty())
        state.sequence = world.GetSequence().Play(s.sequence, options);
    HIKARI::GameProjectSequencePlaybackSnapshot snapshot{};
    if (!state.sequence.IsValid() || !world.GetSequence().GetSnapshot(state.sequence, snapshot)) {
        char message[200]{};
        std::snprintf(message, sizeof(message), "[BossPhaseShift] 演出の資産を流せません（%s）。演出をしません",
                      s.sequence.empty() ? "空" : s.sequence.c_str());
        BossLog::Warn(message);
        Slash(boss, shift);
        Enter(boss, shift, BossShiftBeat::Done);
        return;
    }
    state.showTicks = std::max(1, static_cast<int>(std::ceil((snapshot.durationSeconds + kShowMarginSeconds) *
                                                             kTicksPerSecond / options.playbackRate)));
    BossPhaseShift::ReadCues(world, state.sequence, shift);
    Enter(boss, shift, BossShiftBeat::Show);
    char extra[160]{};
    std::snprintf(extra, sizeof(extra), "handle=%llu seconds=%.2f bound=%d anim=%.2f+%.2f",
                  static_cast<unsigned long long>(state.sequence.value), static_cast<double>(snapshot.durationSeconds),
                  static_cast<int>(options.bindingCount), static_cast<double>(state.cues[0].start),
                  static_cast<double>(state.cues[0].length));
    Log(boss, shift, "Sequence", extra);
}

} // namespace

int BossPhaseShift::TotalFrames(const BossComponent& boss, const BossPhaseShiftComponent* shift) {
    if (shift == nullptr || !shift->settings.enabled)
        return std::max(1, boss.phaseShiftFrames);
    const BossPhaseShiftSettings& s = shift->settings;
    const float seconds =
        s.clipSeconds / std::clamp(s.playbackRate, 0.01f, 8.0f) + s.settleSeconds + kTotalMarginSeconds;
    return std::max(1, static_cast<int>(std::ceil(seconds * kTicksPerSecond)));
}

HIKARI::GameProjectTransform BossPhaseShift::StageTransform(const BossComponent& boss,
                                                            const BossPhaseShiftComponent& shift) {
    HIKARI::GameProjectTransform transform{};
    transform.position = Add(shift.state.origin, Vec3{0.0f, boss.bodyHalfHeight, 0.0f});
    transform.rotation = YawRotation(shift.state.yaw);
    transform.scale = {1.0f, 1.0f, 1.0f};
    return transform;
}

void BossPhaseShift::ReadCues(HIKARI::WorldView& world, HIKARI::GameProjectSequencePlaybackHandle handle,
                              BossPhaseShiftComponent& shift) {
    BossPhaseShiftState& state = shift.state;
    state.cues.fill({});
    std::array<HIKARI::GameProjectSequenceEventClip, HIKARI::kGameProjectSequenceMaximumEventClips> clips{};
    const uint32_t total = world.GetSequence().GetEventClips(handle, clips.data(), static_cast<uint32_t>(clips.size()));
    const size_t count = std::min(static_cast<size_t>(total), clips.size());
    const std::string anim = "Anim." + shift.settings.clip;
    for (size_t k = 0; k < count; ++k) {
        const std::string_view name{clips[k].name};
        for (size_t c = 0; c < kBossShiftCueCount; ++c) {
            const bool match = c == static_cast<size_t>(BossShiftCue::Anim) ? name == anim : name == kCueNames[c];
            if (match && state.cues[c].start < 0.0f)
                state.cues[c] = {clips[k].startSeconds, std::max(0.0f, clips[k].durationSeconds)};
        }
    }
    // クリップの段が無ければ、0 秒から設定の長さで流す。
    BossShiftCueTime& clip = state.cues[static_cast<size_t>(BossShiftCue::Anim)];
    if (clip.start < 0.0f || clip.length <= 0.0f)
        clip = {0.0f, std::max(0.1f, shift.settings.clipSeconds)};
}

float BossPhaseShift::CueProgress(const BossPhaseShiftComponent& shift, BossShiftCue cue, float seconds) {
    const BossShiftCueTime& time = Cue(shift, cue);
    if (time.start < 0.0f || seconds < time.start)
        return 0.0f;
    if (time.length <= kTiny)
        return 1.0f;
    return std::clamp((seconds - time.start) / time.length, 0.0f, 1.0f);
}

bool BossPhaseShift::CueStarted(const BossPhaseShiftComponent& shift, BossShiftCue cue, float seconds) {
    const BossShiftCueTime& time = Cue(shift, cue);
    return time.start >= 0.0f && seconds >= time.start;
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
    // 狙いのずれのねじり（c-884095）は演出へ持ち込まない：骨格の体は BodyYaw で置くので、残っていると舞台へ置き直した
    // 体が最初の 0.2 秒ほどで正面へねじり戻って見える。
    boss.aimTwist = 0.0f;
    boss.aimTwistTarget = 0.0f;
    state.startYaw = boss.yaw;
    state.yaw = boss.yaw;
    const Vec3 toPlayer = Sub(playerCenter, boss.ground);
    if (hasPlayer && FlatLength(toPlayer) >= kFaceMinDistance)
        state.yaw = YawOf(toPlayer);
    state.fallHeight = std::max(0.0f, rootBefore - BossPose::RootHeight(boss));
    boss.phaseTwoLookPending = true;
    // HP の条は、一閃まで空のまま（Slash で満ち始める）。
    boss.phaseStartSeconds = boss.clockSeconds + static_cast<float>(TotalFrames(boss, shift)) / kTicksPerSecond;
    Enter(boss, *shift, BossShiftBeat::Begin);
    char extra[128]{};
    std::snprintf(extra, sizeof(extra), "yaw=%.3f startYaw=%.3f fall=%.3f total=%d", static_cast<double>(state.yaw),
                  static_cast<double>(state.startYaw), static_cast<double>(state.fallHeight), TotalFrames(boss, shift));
    Log(boss, *shift, "Begin", extra);
}

void BossPhaseShift::Step(HIKARI::WorldView& world, BossComponent& boss, BossPhaseShiftComponent* shift) {
    if (shift == nullptr)
        return;
    BossPhaseShiftState& state = shift->state;
    if (!Cinematic(shift)) {
        state.skipRequested = false;
        if (state.calmLeft > 0)
            --state.calmLeft;
        return;
    }
    // 飛ばす（押されたのは描画のフレームなので、次の tick に進める）：フェーズ 2 の構えへ一度に進み、条はいまから満ちる。
    if (state.skipRequested) {
        state.skipRequested = false;
        state.skipped = true;
        StopSequence(world, state);
        if (state.beat == BossShiftBeat::Begin)
            ChooseStage(world, boss, *shift);
        Slash(boss, *shift);
        boss.yaw = state.yaw;
        Log(boss, *shift, "Skip", "");
        Enter(boss, *shift, BossShiftBeat::Done);
        return;
    }
    ++state.frame;
    switch (state.beat) {
    case BossShiftBeat::Begin:
        ChooseStage(world, boss, *shift);
        StartShow(world, boss, *shift);
        return;
    case BossShiftBeat::Show: {
        HIKARI::GameProjectSequencePlaybackSnapshot snapshot{};
        const bool read = world.GetSequence().GetSnapshot(state.sequence, snapshot);
        if (read)
            state.ruleSeconds = snapshot.timeSeconds;
        // 一閃の事件段が来たら（段が無ければ渡しの時に）、フェーズ 2 の見た目と条。
        if (CueStarted(*shift, BossShiftCue::Slash, state.ruleSeconds))
            Slash(boss, *shift);
        const BossShiftCueTime& settle = Cue(*shift, BossShiftCue::Settle);
        const bool ended = !read || snapshot.state == HIKARI::GameProjectSequencePlaybackState::Stopped ||
                           snapshot.timeSeconds >= snapshot.durationSeconds - kShowEndLead ||
                           (settle.start >= 0.0f && state.ruleSeconds >= settle.start);
        if (state.frame >= state.showTicks || ended) {
            Slash(boss, *shift);
            // 渡しの段へ入る時に資産を止めてカメラを放す（ゲームのカメラが、演出の姿勢から混ぜた姿勢を出す。登場と同じ）。
            StopSequence(world, state);
            Enter(boss, *shift, BossShiftBeat::Settle);
        }
        return;
    }
    case BossShiftBeat::Settle:
        if (state.frame >= std::max(0, static_cast<int>(std::lround(shift->settings.settleSeconds * kTicksPerSecond))))
            Enter(boss, *shift, BossShiftBeat::Done);
        return;
    default: return;
    }
}

bool BossPhaseShift::TakePlacement(BossPhaseShiftComponent* shift, HIKARI::GameProjectFloat3& at,
                                   HIKARI::GameProjectFloat3& facing) {
    if (shift == nullptr || !shift->state.placePending)
        return false;
    BossPhaseShiftState& state = shift->state;
    state.placePending = false;
    at = state.placeAt;
    facing = FlatNormalize(Sub(state.origin, state.placeAt), Forward(state.yaw + kPi));
    return true;
}

} // namespace Project_TD2_2
