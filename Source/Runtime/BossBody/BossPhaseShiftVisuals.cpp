#include "BossPhaseShiftVisuals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/CameraViewSnapshot.h"
#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/SequenceView.h"
#include "BossBodyMath.h"
#include "BossCinematicFx.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossPhaseShift.h"
#include "../Boss/BossVfx.h"
#include "../Burn/BurnField.h"
#include "../Player/Feedback.h"
#include "../Common/FluidChannels.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;
using Cue = BossShiftCue;

// 流体への提出 ID（持ち主はボスの物体。登場 1400〜1580・火の舌 1600〜 と重ねない）：継ぎ目の煙、拳の火、腕を伝う火、
// 刀身の火、一閃が場の霧を押す力、飛ばした時に吸い消す所。
constexpr uint64_t kLeakBase = 1700;
constexpr uint64_t kFistId = 1720;
constexpr uint64_t kSpreadBase = 1722;
constexpr uint64_t kBladeFireBase = 1730;
constexpr uint64_t kFogPushBase = 1740;
constexpr uint64_t kQuenchBase = 1750;
constexpr uint64_t kHitBurstId = 1760;
constexpr uint64_t kFallBurstId = 1761;
// 演出の間のカメラの動きのぼけの出し元と優先度（ほかの出し元より先に採る）。
constexpr uint64_t kMotionBlurSource = 0x5444324A23ull;
constexpr int32_t kMotionBlurPriority = 20;
constexpr int kLeakSeams = 6;
constexpr int kSpreadPoints = 5;
constexpr int kBladeFirePoints = 6;
constexpr int kFogPushCount = 8;
// 時間を止める出し元（打ち退け・一閃。ほかの手応えと重ねない）。
constexpr uint64_t kHitStopSource = 0x5444324A21ull;
constexpr uint64_t kSlashStopSource = 0x5444324A22ull;
// 刀身の根元（握りから剣先へ m）。
constexpr float kRootAlong = 0.62f;
// 拳の中心：手首から、前腕→手の向きへこれだけ先（1 倍の体での m）。
constexpr float kFistReach = 0.30f;
// 画面の星を置く所：拳から太陽の向きへこの距離（太陽を掴むカットのカメラは、拳と太陽を結ぶ線の上にいる）。
constexpr float kSunDistance = 400.0f;
// 流れているはずの資産の時刻が、ゲームの時間でこれだけ進まなければ、自分の時計で進める (秒)。
constexpr float kSequenceStallSeconds = 0.25f;
// 太陽へ伸ばす間、星が満ちるのは Reach の前半のこの割合まで。
constexpr float kReachRise = 0.55f;
// 拳の火が燃え上がる長さ (秒) と、Spread が終わるまでに弱める割合。
constexpr float kIgniteRiseSeconds = 0.18f;
constexpr float kIgniteFade = 0.65f;
// 火が回る道（拳 → 袖口 → 肩 → 胸）の、点と点の間の割合と、後ろの点との間隔（道の割合）。
constexpr float kSpreadGap = 0.08f;
// 燃え上がり：Spread のこの進みで（火の先が胸に届くころ）。提出 ID（+0 胸、+1 腰、+2 外へ膨らませる 1 回の押し）。
constexpr float kFlareAt = 0.62f;
constexpr uint64_t kFlareBase = 1796;
// 刀に火を移す：刀身の火（流体）を、移し終えた後もこの秒だけ弱めながら出す（刀の炎のシェーダーの層へ引き継ぐ）。
// 刀の炎の層（BossBlade の flame）を灯し始める所（Fuse の中の割合）。
constexpr float kBladeHandoffSeconds = 1.2f;
constexpr float kBladeShaderShare = 0.4f;
// 一閃：画面の衝撃の輪が広がる長さ (秒) と、広がり切る大きさ（画面の高さを 1）。2 つ目の遅れ (秒)。
constexpr float kSlashWaveSeconds = 0.5f;
constexpr float kSlashWaveRadius = 1.5f;
constexpr float kSecondShockDelay = 0.14f;
// 刃口の光が一閃の後に引く長さ (秒)。刀光を出す長さ（振りかぶりの終わりから一閃の後まで、秒）。
constexpr float kEdgeFallSeconds = 0.6f;
constexpr float kSwingAfterSeconds = 0.15f;
// 火の輪：出始めの大きさ（広がり切った時に対する割合）と、霧を押す高さ (m)。
constexpr float kRingStart = 0.15f;
constexpr float kFogPushHeight = 0.3f;
// 胴の煙を薄くし始めるまでの秒（打ち退けの拍から）と、薄くなり切るまでの秒。
constexpr float kSlumpSmokeDelay = 0.3f;
constexpr float kSlumpSmokeSeconds = 1.6f;
// 柱を降りる火：胸から足元までを分ける点の数と、足元の高さ (m)。
constexpr int kColumnPoints = 5;
constexpr float kColumnFloor = 0.6f;
constexpr uint64_t kColumnBase = 1760;
// 火の壁：燃料の提出 ID、燃える地面を塗り直す秒（毎フレーム塗り直す。演出が止まれば、この秒で消える）、炎のゆらぎの速さ。
constexpr uint64_t kWallBase = 1770;
constexpr float kWallPaintSeconds = 0.3f;
constexpr float kWallFlicker = 7.0f;
// 壁に沿って走る炎の塊の間隔（半径の倍）と、地面からの高さ（半径の倍）。
constexpr float kWallBurstStep = 1.1f;
constexpr float kWallBurstLift = 0.45f;
// 一閃の炎の塊：刀身に沿う塊・先へ続ける塊の半径（落ちた所の塊に対する割合）と、先へ続ける間隔（半径の倍）。
constexpr float kBlastAlongShare = 0.5f;
constexpr float kBlastAheadShare = 0.65f;
constexpr float kBlastAheadStep = 1.3f;
// 継ぎ目の煙：脈の速さ（rad/秒）。座り込みの段の初めと終わりで満ちる・引く割合。
constexpr float kLeakPulse = 5.3f;
constexpr float kLeakEdge = 0.15f;

const BossShiftCueTime& CueOf(const BossPhaseShiftComponent& shift, Cue cue) { return BossPhaseShift::Cue(shift, cue); }

float Progress(const BossPhaseShiftComponent& shift, Cue cue) {
    return BossPhaseShift::CueProgress(shift, cue, shift.state.showSeconds);
}

bool Started(const BossPhaseShiftComponent& shift, Cue cue) {
    return BossPhaseShift::CueStarted(shift, cue, shift.state.showSeconds);
}

bool Ended(const BossPhaseShiftComponent& shift, Cue cue) {
    const BossShiftCueTime& time = CueOf(shift, cue);
    return time.start >= 0.0f && shift.state.showSeconds >= time.start + time.length;
}

// 拍ごとに 1 度だけ（when が真になった最初のフレーム）。
bool Once(BossPhaseShiftComponent& shift, Cue cue, bool when) {
    bool& fired = shift.state.fired[static_cast<size_t>(cue)];
    if (fired || !when)
        return false;
    fired = true;
    return true;
}

// 段の中の重み：始めの edge の割合で満ち、終わりの edge の割合で引く。
float Window(float p, float edge) { return Smooth(p / edge) * (1.0f - Smooth((p - (1.0f - edge)) / edge)); }

void Inject(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, uint64_t id,
            HIKARI::GameProjectFluidInteractionKind kind, Vec3 at, Vec3 velocity, float strength, float radius,
            float heat, uint32_t channels = FluidChannels::kBodyFire) {
    if (!(strength > 0.0f))
        return;
    HIKARI::GameProjectFluidInteraction emit{};
    emit.kind = kind;
    emit.position = at;
    // extents.x は半径、extents.y は温度。
    emit.extents = {std::max(0.02f, radius), heat, 0.0f};
    emit.direction = velocity;
    emit.strength = std::min(10000.0f, strength);
    emit.channels = channels;
    boss.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), id, emit).Accepted() && boss.fluidAccepted;
}

// 拳の中心（手首から、前腕→手の向きへ少し先）。
Vec3 FistPoint(const BossBodyPose& pose, const BossBodyComponent& body) {
    const Vec3 along = Normalize(Sub(pose.leftHand.position, pose.leftCuff.position), Vec3{0.0f, 1.0f, 0.0f});
    return Add(pose.leftHand.position, Mul(along, kFistReach * body.settings.bodyScale));
}

// 道（点の並び）の上の、割合 u の所。
Vec3 AlongPath(const Vec3* points, int count, float u) {
    const float f = Clamp01(u) * static_cast<float>(count - 1);
    const int i = std::min(count - 2, static_cast<int>(f));
    return Lerp(points[i], points[i + 1], f - static_cast<float>(i));
}

// 火の輪：一閃で出し、広がり切るまで燃料を出し、その後は光を下げて消す（流体の炎は自分で燃え尽きる）。
void UpdateRing(HIKARI::WorldView& world, BossComponent& boss, BossPhaseShiftComponent& shift, float seconds) {
    BossPhaseShiftState& state = shift.state;
    const BossPhaseShiftSettings& s = shift.settings;
    if (state.ringVfx.IsValid() && !world.GetVfx().IsActive(state.ringVfx))
        state.ringVfx = {};
    if (!state.ringVfx.IsValid())
        return;
    state.ringAge += std::max(0.0f, seconds);
    const float fade = state.ringAge - s.ringSeconds;
    if (fade >= s.ringFadeSeconds) {
        BossVfx::StopHandle(world, state.ringVfx);
        return;
    }
    const float grow = Clamp01(state.ringAge / std::max(0.001f, s.ringSeconds));
    const float glow = (fade <= 0.0f ? 1.0f : 1.0f - fade / std::max(0.001f, s.ringFadeSeconds)) *
                       std::clamp(s.ringThinGlow, 0.0f, 1.0f);
    const float spread = 1.0f - (1.0f - grow) * (1.0f - grow);
    const float scale = Lerp(kRingStart, 1.0f, spread) * s.ringRadius / std::max(0.01f, boss.ringEffectRadius);
    const Vec3 center{state.origin.x, state.origin.y + boss.ringHeight, state.origin.z};
    BossVfx::Send(world, boss, state.ringVfx, BossVfx::Placed(center, scale),
                  BossVfx::RingParameters(fade <= 0.0f, glow));
}

// 一閃（変わり目でいちばん強い拍）：白黒の閃き・時間の停止・強い揺れ・刀身の光・目の閃き・火の輪・霧を外へ押す。
// 画面の衝撃の輪は Render が毎フレーム出す（地面の細い輪は予告に見えるので出さない）。
void Slash(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, BossBodyComponent& body,
           BossPhaseShiftComponent& shift, Vec3 impact) {
    const BossPhaseShiftSettings& s = shift.settings;
    BossPhaseShiftState& state = shift.state;
    BossCinematicFxState& look = boss.cinematicFxState;
    if (s.slashMono)
        BossCinematicFx::MonoFlash(boss);
    look.accentPending = std::max(look.accentPending, s.slashShake);
    look.glint = std::max(look.glint, s.slashGlint);
    BossCinematicFx::EyeFlare(boss);
    // 炎の塊の列（Render が何回かに分けて出す）。
    state.blastLeft = s.slashBlastRadius > 0.0f ? std::max(1, s.slashBlastWaves) : 0;
    state.blastWait = 0.0f;
    state.blastAt = impact;
    if (s.slashHitStopFrames > 0)
        Feedback::RequestHitStop(world, kSlashStopSource ^ object.GetHandle().value, Feedback::kBreakPriority,
                                 s.slashHitStopFrames, s.slashHitStopScale);
    body.state.fieldWarm = std::max(body.state.fieldWarm, std::clamp(s.fieldWarm, 0.0f, 1.0f));
    if (s.ringRadius > 0.0f && !boss.ringEffect.IsEmpty()) {
        BossVfx::StopHandle(world, state.ringVfx);
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = boss.ringEffect;
        const Vec3 center{state.origin.x, state.origin.y + boss.ringHeight, state.origin.z};
        play.transform = BossVfx::Placed(center, kRingStart * s.ringRadius / std::max(0.01f, boss.ringEffectRadius));
        play.parameters = BossVfx::RingParameters(true, std::clamp(s.ringThinGlow, 0.0f, 1.0f));
        state.ringVfx = BossVfx::Start(world, boss, play);
        state.ringAge = 0.0f;
    }
    for (int k = 0; k < kFogPushCount && s.fogPush > 0.0f; ++k) {
        const Vec3 out = Forward(kTau * static_cast<float>(k) / static_cast<float>(kFogPushCount));
        HIKARI::GameProjectFluidInteraction push{};
        push.kind = HIKARI::GameProjectFluidInteractionKind::Impulse;
        push.position = Add(Add(state.origin, Mul(out, s.fogPushRadius * 0.5f)), Vec3{0.0f, kFogPushHeight, 0.0f});
        push.extents = {s.fogPushRadius, 0.0f, 0.0f};
        push.direction = out;
        push.strength = s.fogPush;
        push.channels = FluidChannels::kFog;
        boss.fluidAccepted =
            world.GetFluid().Submit(object.GetHandle(), kFogPushBase + static_cast<uint64_t>(k), push).Accepted() &&
            boss.fluidAccepted;
    }
    char message[160]{};
    std::snprintf(message, sizeof(message), "ShiftSlashFx tick=%llu at=(%.2f,%.2f,%.2f) t=%.2f",
                  static_cast<unsigned long long>(boss.tick), static_cast<double>(impact.x),
                  static_cast<double>(impact.y), static_cast<double>(impact.z), static_cast<double>(state.showSeconds));
    BossLog::Write(message);
}

// 飛ばした時に吸い消す所を覚える：拳・袖口・肩・胸と、刀身（根元から火が届いた所まで）。
void RememberQuench(const BossBodyComponent& body, BossPhaseShiftComponent& shift, const BossBodyPose& pose) {
    BossPhaseShiftState& state = shift.state;
    int k = 0;
    const auto put = [&](Vec3 at) {
        if (k < static_cast<int>(state.quenchAt.size()))
            state.quenchAt[static_cast<size_t>(k++)] = at;
    };
    put(FistPoint(pose, body));
    put(pose.leftHand.position);
    put(pose.leftCuff.position);
    put(pose.sode[1].position);
    put(pose.torso.position);
    put(Add(pose.torso.position, Vec3{0.0f, 0.8f, 0.0f}));
    if (state.bladeLit > 0.0f) {
        const Vec3 tip = Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
        for (float share : {0.0f, 0.5f, 1.0f})
            put(Add(pose.sword.position, Mul(tip, Lerp(kRootAlong, state.bladeLit, share))));
    }
    state.quenchCount = k;
}

// 飛ばした時：覚えた所の胴の煙の域と場の火の域を、quench の秒数だけ吸い消す。飛ばしたのを見たフレームは skipFrame。
void Quench(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, const BossBodyComponent& body,
            BossPhaseShiftComponent& shift, float seconds) {
    const BossPhaseShiftSettings& s = shift.settings;
    BossPhaseShiftState& state = shift.state;
    state.skipFrame = false;
    if (state.skipped && !state.skipSeen) {
        state.skipSeen = true;
        state.skipFrame = true;
        state.quenchLeft = std::max(0.0f, s.skipQuenchSeconds);
    }
    if (!(state.quenchLeft > 0.0f) || state.quenchCount <= 0 || !(s.skipQuenchRate > 0.0f))
        return;
    state.quenchLeft -= std::max(0.0f, seconds);
    for (int k = 0; k < state.quenchCount; ++k) {
        HIKARI::GameProjectFluidInteraction sink{};
        sink.kind = HIKARI::GameProjectFluidInteractionKind::Force;
        sink.position = state.quenchAt[static_cast<size_t>(k)];
        sink.extents = {std::max(0.05f, s.skipQuenchRadius * body.settings.bodyScale), 0.0f, 0.0f};
        sink.strength = 0.0f;
        sink.absorption = std::min(1000.0f, s.skipQuenchRate);
        sink.channels = FluidChannels::kBodyFire | FluidChannels::kFire;
        boss.fluidAccepted =
            world.GetFluid().Submit(object.GetHandle(), kQuenchBase + static_cast<uint64_t>(k), sink).Accepted() &&
            boss.fluidAccepted;
    }
}

// 演出の資産の時刻に見た目の時計を合わせる（1 歩先：このフレームの systems の後に資産が進み、カメラもその時刻で決まる）。
void FollowSequence(HIKARI::WorldView& world, BossPhaseShiftComponent& shift, float dt) {
    BossPhaseShiftState& state = shift.state;
    const float rate = std::clamp(shift.settings.playbackRate, 0.01f, 8.0f);
    HIKARI::GameProjectSequencePlaybackSnapshot snapshot{};
    if (!state.sequenceStalled && state.sequence.IsValid() &&
        world.GetSequence().GetSnapshot(state.sequence, snapshot)) {
        if (snapshot.state == HIKARI::GameProjectSequencePlaybackState::Paused)
            return;
        if (snapshot.state == HIKARI::GameProjectSequencePlaybackState::Playing) {
            if (snapshot.timeSeconds != state.sequenceSeen) {
                state.sequenceSeen = snapshot.timeSeconds;
                state.sequenceStall = 0.0f;
                state.showSeconds = snapshot.timeSeconds + dt * rate;
                return;
            }
            state.sequenceStall += dt;
            if (state.sequenceStall < kSequenceStallSeconds)
                return;
            state.sequenceStalled = true;
            BossLog::Warn(
                "[BossPhaseShift] 演出の資産の時刻が進みません（演出の機能が切ってある？）。ゲームの時間で流します");
        }
    }
    state.showSeconds += dt * rate;
}

} // namespace

void BossPhaseShiftVisuals::Advance(HIKARI::WorldView& world, BossComponent& boss, BossBodyComponent& look,
                                    BossPhaseShiftComponent* shift, float seconds) {
    (void)boss;
    if (shift == nullptr)
        return;
    BossPhaseShiftState& state = shift->state;
    const BossPhaseShiftSettings& s = shift->settings;
    const float dt = std::max(0.0f, seconds);
    const float rate = std::clamp(s.playbackRate, 0.01f, 8.0f);
    switch (state.beat) {
    case BossShiftBeat::Show: FollowSequence(world, *shift, dt); break;
    case BossShiftBeat::Settle:
        state.showSeconds += dt * rate;
        state.settleClock += dt;
        break;
    default: break;
    }
    // 骨格の体にクリップを頼む（演技と渡しの間。飛ばした後は頼まない＝待機へ混ぜて戻る）。姿の層はエンジンの動画の更新が
    // 次のフレームに読むので、もう 1 歩先の時刻を頼む（カメラと同じ時刻の姿が描かれる。登場と同じ）。
    look.anim.cinematicBlendOut = s.clipBlendOutSeconds;
    if ((state.beat == BossShiftBeat::Show || state.beat == BossShiftBeat::Settle) && !state.skipped) {
        const BossShiftCueTime& anim = CueOf(*shift, Cue::Anim);
        look.anim.cinematicClip = s.clip;
        look.anim.cinematicNormalized =
            Clamp01((state.showSeconds + dt * rate - anim.start) / std::max(0.01f, anim.length));
    } else {
        look.anim.cinematicClip.clear();
    }
    // 演出の資産が流れている間、カメラの姿勢を控えておく（渡しの段で BossPhaseShiftCamera がここからゲームのカメラへ）。
    HIKARI::GameProjectCameraPose film{};
    HIKARI::GameProjectSequencePlaybackSnapshot playback{};
    if (state.beat == BossShiftBeat::Show && !state.skipped &&
        world.GetSequence().GetSnapshot(state.sequence, playback) &&
        playback.state == HIKARI::GameProjectSequencePlaybackState::Playing &&
        world.GetCamera().GetRenderedPose(film) && film.verticalFovRadians > 0.0f) {
        state.filmValid = true;
        state.filmEye = film.eye;
        state.filmTarget = film.target;
        state.filmFov = film.verticalFovRadians;
    }
}

float BossPhaseShiftVisuals::FireFloor(const BossPhaseShiftComponent* shift) {
    return BossPhaseShift::Cinematic(shift) ? Clamp01(shift->state.fireFloor) : 0.0f;
}

float BossPhaseShiftVisuals::SmokeScale(const BossPhaseShiftComponent* shift) {
    if (!BossPhaseShift::Pinned(shift) || shift->state.skipped)
        return 1.0f;
    const BossShiftCueTime& hit = CueOf(*shift, Cue::Hit);
    const float t = shift->state.showSeconds;
    const float down = hit.start >= 0.0f ? Smooth((t - hit.start - kSlumpSmokeDelay) / kSlumpSmokeSeconds) : 1.0f;
    const float up = Smooth(Progress(*shift, Cue::Spread));
    return Lerp(1.0f, std::clamp(shift->settings.slumpSmoke, 0.0f, 1.0f), down * (1.0f - up));
}

float BossPhaseShiftVisuals::BurnBoost(const BossPhaseShiftComponent* shift) {
    if (!BossPhaseShift::Cinematic(shift) || shift->state.embersLeft <= 0.0f)
        return 1.0f;
    const BossPhaseShiftSettings& s = shift->settings;
    return Lerp(1.0f, std::max(1.0f, s.flareEmbers),
                Smooth(shift->state.embersLeft / std::max(0.01f, s.flareEmbersSeconds)));
}

float BossPhaseShiftVisuals::FieldFloor(const BossPhaseShiftComponent* shift) {
    return BossPhaseShift::Cinematic(shift) ? Clamp01(shift->state.fieldFloor) : 0.0f;
}

HIKARI::GameProjectTransform BossPhaseShiftVisuals::SmokeBody(const BossPhaseShiftComponent* shift,
                                                              const BossBodyComponent& look,
                                                              const HIKARI::GameProjectTransform& body) {
    if (!BossPhaseShift::Pinned(shift) || !look.anim.smokePivotValid)
        return body;
    // 腰をなめらかに追った所（細かい揺れは追わない。BossBodyVisuals の StepSmokeTilt）。
    HIKARI::GameProjectTransform out = body;
    out.position.x = look.anim.smokePivot.x;
    out.position.z = look.anim.smokePivot.z;
    return out;
}

void BossPhaseShiftVisuals::ApplyPose(BossComponent& boss, BossPhaseShiftComponent* shift, BossBodyPose& pose) {
    if (!BossPhaseShift::Cinematic(shift))
        return;
    const BossPhaseShiftSettings& s = shift->settings;
    const BossPhaseShiftState& state = shift->state;
    const bool phaseTwo = BossBodyMath::PhaseTwoLook(boss);
    // 刀の炎の層：刀に火を移す間の途中から（流体の火から引き継ぐ）。目：熱さで混ぜる。
    pose.bladeHot =
        phaseTwo || state.bladeDone || (Started(*shift, Cue::Fuse) && Progress(*shift, Cue::Fuse) >= kBladeShaderShare);
    pose.eyesHeat = phaseTwo ? 1.0f : state.eyesHeat;
    // 振りかぶり：刃口の光・体を包む気配・画の張りつめが高まり、振り下ろしで刀光を引く。一閃の後は引く。
    BossCinematicFxState& look = boss.cinematicFxState;
    const float t = state.showSeconds;
    const BossShiftCueTime& slash = CueOf(*shift, Cue::Slash);
    if (Started(*shift, Cue::Charge) && (slash.start < 0.0f || t < slash.start)) {
        const float c = Smooth(Progress(*shift, Cue::Charge));
        look.bladeEdge = std::max(look.bladeEdge, s.chargeEdge * c);
        look.aura = std::max(look.aura, s.chargeAura * c);
        look.tensionPending = std::max(look.tensionPending, s.chargeTension * c);
    }
    if (Ended(*shift, Cue::Charge) && slash.start >= 0.0f && t < slash.start + kSwingAfterSeconds)
        look.bladeSwing = true;
    if (slash.start >= 0.0f && t >= slash.start)
        look.bladeEdge = std::max(look.bladeEdge, s.chargeEdge * (1.0f - Smooth((t - slash.start) / kEdgeFallSeconds)));
}

void BossPhaseShiftVisuals::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                                   BossBodyComponent& body, BossPhaseShiftComponent* shift, const BossBodyPose& pose,
                                   float seconds) {
    if (shift == nullptr)
        return;
    BossPhaseShiftState& state = shift->state;
    const BossPhaseShiftSettings& s = shift->settings;
    const float dt = std::max(0.0f, seconds);
    UpdateRing(world, boss, *shift, dt);
    Quench(world, object, boss, body, *shift, dt);
    if (!BossPhaseShift::Pinned(shift) || state.skipped)
        return;
    RememberQuench(body, *shift, pose);
    BossCinematicFxState& fx = boss.cinematicFxState;
    for (BossCinematicFxState::Shock& shock : fx.shocks)
        shock.delay -= dt;
    const float t = state.showSeconds;
    const float scale = body.settings.bodyScale;
    const uint64_t owner = object.GetHandle().value;
    const Vec3 fist = FistPoint(pose, body);

    // 打ち退け：時間を一瞬止め、揺らす。
    if (Once(*shift, Cue::Hit, Started(*shift, Cue::Hit))) {
        if (s.hitStopFrames > 0)
            Feedback::RequestHitStop(world, kHitStopSource ^ owner, Feedback::kBreakPriority, s.hitStopFrames,
                                     s.hitStopScale);
        fx.accentPending = std::max(fx.accentPending, s.hitShake);
        // 胴の煙が、打たれた勢いで体から弾け出る（向きの無い衝撃＝外へ広がる）。
        Inject(world, object, boss, kHitBurstId, HIKARI::GameProjectFluidInteractionKind::Impulse, pose.torso.position,
               {}, s.hitBurst, s.hitBurstRadius * scale, 0.0f);
    }

    // 崩れ落ちる：刀が地面を打つ瞬間に揺らし、胴の煙が弾ける。
    if (Once(*shift, Cue::Fall, Started(*shift, Cue::Fall))) {
        fx.accentPending = std::max(fx.accentPending, s.fallShake);
        Inject(world, object, boss, kFallBurstId, HIKARI::GameProjectFluidInteractionKind::Impulse, pose.torso.position,
               {}, s.fallBurst, s.hitBurstRadius * scale, 0.0f);
    }

    // カメラの動きのぼけ：演出の間は強め、渡しの間はゲームのカメラへ戻るのに合わせて弱める。
    if (s.motionBlur && world.GetCamera().IsValid()) {
        const float weight =
            state.beat == BossShiftBeat::Settle
                ? 1.0f - Smootherstep(std::clamp(state.settleClock / std::max(0.01f, s.settleSeconds), 0.0f, 1.0f))
                : 1.0f;
        if (weight > 0.0f)
            Feedback::SubmitMotionBlur(world, kMotionBlurSource ^ owner, kMotionBlurPriority, weight,
                                       s.motionBlurShutterDegrees, s.motionBlurMaxPixels);
    }

    // 座り込み：鎧の継ぎ目（襟の左右・肩・袖口）から黒い煙が漏れる。
    if (Started(*shift, Cue::Leak) && !Ended(*shift, Cue::Leak) && s.leakDensity > 0.0f) {
        const float leak = s.leakDensity * Window(Progress(*shift, Cue::Leak), kLeakEdge);
        const Vec3 center = pose.torso.position;
        const float radius = s.leakRadius * scale;
        int k = 0;
        const auto seam = [&](Vec3 at, float share) {
            const Vec3 out = Normalize(Add(FlatNormalize(Sub(at, center), Forward(state.yaw)), Vec3{0.0f, 0.6f, 0.0f}));
            const float pulse = 0.75f + 0.25f * std::sin(t * kLeakPulse + 1.7f * static_cast<float>(k));
            if (k < kLeakSeams)
                Inject(world, object, boss, kLeakBase + static_cast<uint64_t>(k++),
                       HIKARI::GameProjectFluidInteractionKind::Emit, at, Mul(out, s.leakSpeed), leak * share * pulse,
                       radius, 0.0f);
        };
        for (float side : {1.0f, -1.0f}) {
            const Vec3 collar =
                Mul({body.settings.fireCollar.x * side, body.settings.fireCollar.y, body.settings.fireCollar.z},
                    pose.torso.scale);
            seam(Add(pose.torso.position, Rotate(pose.torso.rotation, collar)), 1.0f);
        }
        seam(pose.sode[0].position, 0.8f);
        seam(pose.sode[1].position, 0.8f);
        seam(pose.rightCuff.position, 0.6f);
        seam(pose.leftCuff.position, 0.6f);
    }

    // 太陽へ伸ばす：太陽の所の画面の星が強まり、握るにつれて拳に隠れて消える。カメラの前にある時だけ（太陽を掴むカット）。
    if (state.sunValid && Started(*shift, Cue::Reach) && !Ended(*shift, Cue::Grab)) {
        float star = s.reachStar * Smooth(Progress(*shift, Cue::Reach) / kReachRise);
        if (Started(*shift, Cue::Grab))
            star *= 1.0f - Smooth(Progress(*shift, Cue::Grab));
        // 星は本当の太陽の所（カメラから太陽の向き）に出す：指の間から見える太陽に光の筋を足し、握るにつれて消える。
        HIKARI::GameProjectCameraPose film{};
        const bool read = world.GetCamera().GetRenderedPose(film);
        const Vec3 point = Add(read ? film.eye : fist, Mul(state.sunDirection, kSunDistance));
        const bool ahead = read && Dot(state.sunDirection, Normalize(Sub(film.target, film.eye))) > 0.0f;
        if (ahead && star > 0.0f) {
            fx.postStar = std::max(fx.postStar, star);
            fx.postStarPoint = point;
            fx.postStarPointValid = true;
        }
    }
    // 掴む（握り終えた所）：光を拳へ吸い込む輪・小さな揺れ・画が一瞬暗くなる。ここから場の色が変わり、拳が燃える。
    if (Once(*shift, Cue::Grab, Ended(*shift, Cue::Grab))) {
        state.sunPoint = fist;
        state.sunPointValid = true;
        state.grabLeft = s.grabShockSeconds;
        state.dimLeft = s.grabDimSeconds;
        fx.accentPending = std::max(fx.accentPending, s.grabShake);
        char message[160]{};
        std::snprintf(message, sizeof(message), "ShiftGrabFx tick=%llu fist=(%.2f,%.2f,%.2f) t=%.2f",
                      static_cast<unsigned long long>(boss.tick), static_cast<double>(fist.x),
                      static_cast<double>(fist.y), static_cast<double>(fist.z), static_cast<double>(t));
        BossLog::Write(message);
    }
    if (state.grabLeft > 0.0f) {
        state.grabLeft = std::max(0.0f, state.grabLeft - dt);
        const float k = state.grabLeft / std::max(0.01f, s.grabShockSeconds);
        fx.postShock = std::max(fx.postShock, s.grabShock * Smooth(k / 0.3f));
        fx.postShockRadius = s.grabShockRadius * k * k;
        fx.postShockAt = fist;
    }
    if (state.dimLeft > 0.0f) {
        state.dimLeft = std::max(0.0f, state.dimLeft - dt);
        fx.dimPending = std::max(fx.dimPending, s.grabDim * Smooth(state.dimLeft / std::max(0.01f, s.grabDimSeconds)));
    }

    // 拳の火：握った所で火が付き、燃え上がって、火が回るにつれて弱まる（袖口の火の舌に引き継ぐ）。
    if (Started(*shift, Cue::Ignite) && !Ended(*shift, Cue::Spread) && s.fistFuel > 0.0f) {
        const BossShiftCueTime& ignite = CueOf(*shift, Cue::Ignite);
        const float rise = Smooth((t - ignite.start) / kIgniteRiseSeconds);
        const float fade = 1.0f - kIgniteFade * Smooth(Progress(*shift, Cue::Spread));
        Inject(world, object, boss, kFistId, HIKARI::GameProjectFluidInteractionKind::Fuel, fist, {0.0f, 0.9f, 0.0f},
               s.fistFuel * rise * fade, s.fistFuelRadius * scale, s.fistFuelHeat);
    }
    // 場の色：掴んでから変わっていく。
    state.fieldFloor = std::max(state.fieldFloor, s.fieldWarm * Smooth(Progress(*shift, Cue::Field)));
    // 火が回る：拳 → 袖口 → 肩 → 胸へ火の先が進み（その後ろに 3 つの点で燃料）、胴の煙の火が上がる。
    if (Started(*shift, Cue::Spread)) {
        const float p = Progress(*shift, Cue::Spread);
        state.fireFloor = std::max(state.fireFloor, s.spreadFire * Smooth((p - 0.35f) / 0.65f));
        if (!Ended(*shift, Cue::Spread) && s.spreadFuel > 0.0f) {
            const Vec3 path[4] = {fist, pose.leftCuff.position, pose.sode[1].position, pose.torso.position};
            const float front = Smooth(p);
            for (int k = 0; k < kSpreadPoints; ++k) {
                const float u = front - kSpreadGap * static_cast<float>(k);
                if (u < 0.0f)
                    break;
                Inject(world, object, boss, kSpreadBase + static_cast<uint64_t>(k),
                       HIKARI::GameProjectFluidInteractionKind::Fuel, AlongPath(path, 4, u), {0.0f, 0.7f, 0.0f},
                       s.spreadFuel / kSpreadPoints * Window(p, 0.1f), s.fistFuelRadius * scale, s.fistFuelHeat);
            }
        }
    }
    // 胴の煙の柱へ火が降りる：胸から足元へ、火の先が Spread の進みで降り、その後ろに燃料を出す（黒い煙が燃えて炎になる）。
    // 回り切った後も、演出の終わりまで、弱めて燃やし続ける（立ち上がる時・刀を拭う時も、体が燃えて見える）。
    const Vec3 columnTop = pose.torso.position;
    const Vec3 columnFoot{body.anim.hipsSeen ? body.anim.hipsWorld.x : columnTop.x, state.origin.y + kColumnFloor,
                          body.anim.hipsSeen ? body.anim.hipsWorld.z : columnTop.z};
    if (Started(*shift, Cue::Spread) && s.columnFuel > 0.0f) {
        const bool spreading = !Ended(*shift, Cue::Spread);
        const float p = Progress(*shift, Cue::Spread);
        const float front = spreading ? Smooth((p - 0.2f) / 0.7f) : 1.0f;
        // 回っている間は、段の初めと終わりで満ち引きする。終わりは、燃やし続ける割合までで止める。
        const float sustain = std::clamp(s.columnSustain, 0.0f, 1.0f);
        const float level = spreading ? std::max(Window(p, 0.12f), sustain * Smooth((p - 0.5f) / 0.5f)) : sustain;
        for (int k = 0; k < kColumnPoints; ++k) {
            const float u = (static_cast<float>(k) + 0.5f) / kColumnPoints;
            if (u > front)
                break;
            Inject(world, object, boss, kColumnBase + static_cast<uint64_t>(k),
                   HIKARI::GameProjectFluidInteractionKind::Fuel, Lerp(columnTop, columnFoot, u), {0.0f, 1.4f, 0.0f},
                   s.columnFuel / kColumnPoints * level, s.columnFuelRadius * scale, s.fistFuelHeat);
        }
    }
    // 燃え上がり：火の先が胸に届いた所で、胴の煙の柱が一気に燃え上がる（この拍の見せ場）。胸と腰へ燃料を入れ、炎を外へ
    // 少し膨らませ、小さく揺らし、火の粉をしばらく増やす。
    if (!state.flared && Started(*shift, Cue::Spread) && Progress(*shift, Cue::Spread) >= kFlareAt) {
        state.flared = true;
        state.flareLeft = s.flareSeconds;
        state.embersLeft = s.flareEmbersSeconds;
        fx.accentPending = std::max(fx.accentPending, s.flareShake);
        if (s.flarePush > 0.0f)
            Inject(world, object, boss, kFlareBase + 2, HIKARI::GameProjectFluidInteractionKind::Impulse, columnTop, {},
                   s.flarePush, s.flareRadius * scale * 1.6f, 0.0f);
    }
    if (state.flareLeft > 0.0f && s.flareFuel > 0.0f) {
        // 出始めが最大で、引いて行く。
        const float k = Smooth(state.flareLeft / std::max(0.01f, s.flareSeconds) / 0.7f);
        Inject(world, object, boss, kFlareBase, HIKARI::GameProjectFluidInteractionKind::Fuel, columnTop,
               {0.0f, 2.2f, 0.0f}, s.flareFuel * k, s.flareRadius * scale, s.fistFuelHeat);
        Inject(world, object, boss, kFlareBase + 1, HIKARI::GameProjectFluidInteractionKind::Fuel,
               Lerp(columnTop, columnFoot, 0.45f), {0.0f, 2.6f, 0.0f}, s.flareFuel * 0.7f * k, s.flareRadius * scale,
               s.fistFuelHeat);
    }
    state.flareLeft = std::max(0.0f, state.flareLeft - dt);
    state.embersLeft = std::max(0.0f, state.embersLeft - dt);
    // 目：拳の火が兜に届くころ、ふだんの色から橙赤へ。
    state.eyesHeat = std::max(state.eyesHeat, Smooth(Progress(*shift, Cue::Eyes)));

    // 刀に火を移す：左手が刀身を拭う所より先へ（fuseLead の割合で）火が進み、届いた所まで燃料を出す。拭い終えたら刀の炎の
    // 層へ引き継ぎ、流体の火は少しの間だけ弱めながら出す。
    const Vec3 grip = pose.sword.position;
    const Vec3 blade = Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
    const float reach = VisibleBladeReach(boss, body.settings);
    if (Started(*shift, Cue::Fuse) && !state.bladeDone) {
        const float p = Progress(*shift, Cue::Fuse);
        const float hand = Dot(Sub(pose.leftHand.position, grip), blade);
        float lit = kRootAlong + std::max(0.0f, hand - kRootAlong) * s.fuseLead;
        lit = std::max(lit, Lerp(kRootAlong, reach, Smooth(p)));
        state.bladeLit = std::clamp(std::max(state.bladeLit, lit), kRootAlong, reach);
        if (p >= 1.0f)
            state.bladeDone = true;
    }
    if (state.bladeLit > 0.0f && s.bladeFuel > 0.0f) {
        const BossShiftCueTime& fuse = CueOf(*shift, Cue::Fuse);
        const float after = state.bladeDone ? (t - (fuse.start + fuse.length)) / kBladeHandoffSeconds : 0.0f;
        const float weight = 1.0f - Smooth(after);
        if (weight > 0.0f) {
            for (int k = 0; k < kBladeFirePoints; ++k) {
                const float along = Lerp(kRootAlong, state.bladeLit, (static_cast<float>(k) + 0.5f) / kBladeFirePoints);
                Inject(world, object, boss, kBladeFireBase + static_cast<uint64_t>(k),
                       HIKARI::GameProjectFluidInteractionKind::Fuel, Add(grip, Mul(blade, along)), {0.0f, 1.0f, 0.0f},
                       s.bladeFuel / kBladeFirePoints * weight, s.bladeFuelRadius, s.bladeFuelHeat);
            }
        }
    }

    // 火の壁：左手を前へ払った後、ボスの正面（舞台の正面へ wallDistance）に、真ん中から左右へ炎が立ち上がる。地面は燃える
    // 地面として塗り（毎フレーム短い秒で塗り直す）、その上へ熱い燃料を吹き上げて壁の高さにする。一閃までで、一閃で爆ぜる。
    const Vec3 front = Forward(state.yaw);
    const Vec3 across{front.z, 0.0f, -front.x};
    const Vec3 wallCenter = Add(state.origin, Mul(front, s.wallDistance));
    if (Started(*shift, Cue::Wall) && !Started(*shift, Cue::Slash)) {
        const float grow = Smooth((t - CueOf(*shift, Cue::Wall).start) / std::max(0.05f, s.wallRiseSeconds));
        const float half = s.wallHalfWidth * grow;
        if (s.wallPaintWidth > 0.0f && half > 0.2f)
            BurnField::PaintSegment(world, Sub(wallCenter, Mul(across, half)), Add(wallCenter, Mul(across, half)),
                                    s.wallPaintWidth, kWallPaintSeconds);
        const int points = std::clamp(s.wallPoints, 0, 16);
        for (int k = 0; k < points; ++k) {
            const float u = (static_cast<float>(k) + 0.5f) / static_cast<float>(points) * 2.0f - 1.0f;
            const float flicker = 0.8f + 0.2f * std::sin(t * kWallFlicker + 1.9f * static_cast<float>(k));
            const Vec3 at = Add(Add(wallCenter, Mul(across, u * half)), Vec3{0.0f, s.wallFuelRadius * 0.6f, 0.0f});
            Inject(world, object, boss, kWallBase + static_cast<uint64_t>(k),
                   HIKARI::GameProjectFluidInteractionKind::Fuel, at, {0.0f, s.wallFuelRise, 0.0f},
                   s.wallFuel * grow * flicker, s.wallFuelRadius, s.wallFuelHeat, FluidChannels::kFire);
        }
    }
    // 一閃：剣先が地面に当たった所で、閃光・停止・揺れ・火の輪・霧を押す。画面の衝撃の輪は、そこから広がる。
    const BossShiftCueTime& slash = CueOf(*shift, Cue::Slash);
    const Vec3 tip = Add(grip, Mul(blade, reach));
    if (Once(*shift, Cue::Slash, Started(*shift, Cue::Slash))) {
        Slash(world, object, boss, body, *shift, Vec3{tip.x, state.origin.y, tip.z});
        state.blastAlong = FlatNormalize(blade, Forward(state.yaw));
        state.blastReach = FlatLength(Sub(tip, grip));
        state.wallBurst = Started(*shift, Cue::Wall) && s.wallBurstRadius > 0.0f ? 0.0f : -1.0f;
    }
    // 火の壁が爆ぜる：剣先の落ちた所（を壁の線へ写した所）から、壁に沿って左右へ炎の塊が走る。
    if (state.wallBurst >= 0.0f) {
        const float step = std::max(0.3f, s.wallBurstRadius * kWallBurstStep);
        const float from = state.wallBurst;
        state.wallBurst += dt * s.wallBurstSpeed;
        const float hit = std::clamp(Dot(Sub(state.blastAt, wallCenter), across), -s.wallHalfWidth, s.wallHalfWidth);
        const Vec3 lift{0.0f, s.wallBurstRadius * kWallBurstLift, 0.0f};
        for (float d = std::ceil(from / step) * step; d < state.wallBurst; d += step) {
            if (d <= 0.0f)
                continue;
            for (float side : {1.0f, -1.0f}) {
                const float x = hit + side * d;
                if (std::abs(x) <= s.wallHalfWidth)
                    BurnField::AddBlast(world, Add(Add(wallCenter, Mul(across, x)), lift), s.wallBurstRadius,
                                        BurnBlastKind::Hit);
            }
        }
        if (state.wallBurst > 2.0f * s.wallHalfWidth)
            state.wallBurst = -1.0f;
    }
    // 一閃の炎の塊：落ちた所に大きい塊、刀身に沿って地面に小さい塊、剣先の先へ続く塊を、何回かに分けて出す。
    if (state.blastLeft > 0) {
        state.blastWait -= dt;
        if (state.blastWait <= 0.0f) {
            state.blastWait = std::max(0.01f, s.slashBlastInterval);
            --state.blastLeft;
            const float radius = s.slashBlastRadius;
            const Vec3 at = Add(state.blastAt, Vec3{0.0f, radius * 0.4f, 0.0f});
            BurnField::AddBlast(world, at, radius, BurnBlastKind::Hit);
            for (int k = 0; k < s.slashBlastAlong; ++k) {
                const float back = state.blastReach * (static_cast<float>(k) + 0.5f) / std::max(1, s.slashBlastAlong);
                BurnField::AddBlast(world, Sub(at, Mul(state.blastAlong, back)), radius * kBlastAlongShare,
                                    BurnBlastKind::Hit);
            }
            for (int k = 0; k < s.slashBlastAhead; ++k)
                BurnField::AddBlast(
                    world, Add(at, Mul(state.blastAlong, radius * kBlastAheadStep * static_cast<float>(k + 1))),
                    radius * kBlastAheadShare, BurnBlastKind::Hit);
        }
    }
    if (slash.start >= 0.0f && t >= slash.start && s.slashShock > 0.0f) {
        const float age = t - slash.start;
        const auto wave = [&](float delay, float strength) {
            const float u = (age - delay) / kSlashWaveSeconds;
            if (u < 0.0f || u >= 1.0f || strength <= fx.postShock)
                return;
            fx.postShock = strength * (1.0f - Smooth(u));
            fx.postShockRadius = kSlashWaveRadius * (1.0f - (1.0f - u) * (1.0f - u));
            fx.postShockAt = Vec3{tip.x, state.origin.y, tip.z};
        };
        wave(0.0f, s.slashShock);
        wave(kSecondShockDelay, s.slashShockSecond);
    }
}

bool BossPhaseShiftVisuals::HoldTongues(const BossPhaseShiftComponent* shift) {
    return shift != nullptr && shift->state.skipFrame;
}

void BossPhaseShiftVisuals::Stop(HIKARI::WorldView& world, BossPhaseShiftComponent* shift) {
    if (shift != nullptr)
        BossVfx::StopHandle(world, shift->state.ringVfx);
}

void BossPhaseShiftVisuals::Preview(HIKARI::WorldView& world, const BossComponent& boss,
                                    BossPhaseShiftComponent& shift) {
    BossPhaseShiftState& state = shift.state;
    HIKARI::GameProjectSequencePlaybackSnapshot snapshot{};
    const bool shown = !shift.settings.sequence.empty() && world.GetSequence().GetEditorPreview(snapshot) &&
                       shift.settings.sequence == snapshot.assetGuid;
    const HIKARI::ObjectView body =
        boss.animBody.object != 0u ? world.FindSceneObject(boss.animBody.object) : HIKARI::ObjectView{};
    const HIKARI::AnimationView animation = world.GetAnimation();
    if (!shown || !body.IsValid() || !animation.IsValid()) {
        // プレビューをやめた：重ねた姿の層を外し、動画の評価を止める（ふだんの編集の見た目へ戻す）。
        if (state.previewing) {
            state.previewing = false;
            state.previewHandle = {};
            if (body.IsValid() && animation.IsValid()) {
                (void)animation.ClearMaskedPoseLayer(body.GetHandle().value, 0.0f);
                (void)animation.Stop(body.GetHandle().value);
                (void)animation.SetEvaluationEnabled(body.GetHandle().value, 0u);
            }
        }
        return;
    }
    const uint64_t handle = body.GetHandle().value;
    if (!state.previewing || snapshot.handle != state.previewHandle) {
        state.previewing = true;
        state.previewHandle = snapshot.handle;
        BossPhaseShift::ReadCues(world, snapshot.handle, shift);
        (void)animation.SetEvaluationEnabled(handle, 1u);
        // 下の層に流すクリップ（無いと姿の層が効かない）：同じクリップを止めて置く。
        if (animation.Play(handle, shift.settings.clip, 0u, 1u))
            (void)animation.Pause(handle);
    }
    const BossShiftCueTime& anim = CueOf(shift, Cue::Anim);
    const float u = Clamp01((snapshot.timeSeconds - anim.start) / std::max(0.01f, anim.length));
    (void)animation.SetMaskedPoseLayer(handle, shift.settings.clip, "Hips", u, 1.0f, 0.0f);
}

} // namespace Project_TD2_2
