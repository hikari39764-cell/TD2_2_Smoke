#include "BossPhaseShiftVisuals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/FluidView.h"
#include "BossBodyMath.h"
#include "BossPoseKit.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossPhaseShift.h"
#include "../Boss/BossVfx.h"
#include "../Combat/Feedback.h"
#include "../Common/FluidChannels.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;
using namespace BossPoseKit;

// 固定 tick の数（1 秒あたり）。
constexpr float kFramesPerSecond = 60.0f;
constexpr float kTau = 2.0f * kPi;
// 流体への提出 ID（持ち主はボスの物体。登場 1400〜1580・火の舌 1600〜 と重ねない）：継ぎ目の煙、胸の火の粉、刀身の火、
// 火の輪が場の霧を押す力。
constexpr uint64_t kLeakBase = 1700;
constexpr uint64_t kChestId = 1720;
constexpr uint64_t kBladeFireBase = 1730;
constexpr uint64_t kFogPushBase = 1740;
// 飛ばした時に、演出で入れた火を吸い消す所（1750〜1759）。
constexpr uint64_t kQuenchBase = 1750;
constexpr int kBladeFirePoints = 5;
constexpr int kFogPushCount = 8;
// 最後の当たりで時間を止める出し元（ほかの手応えと重ねない）。
constexpr uint64_t kHitStopSource = 0x5444324A21ull;
// 傾きの軸の高さ（1 倍の体での値。腰）。兜・草摺が体の動きに付いて回る割合。
constexpr float kLeanPivot = 1.9f;
constexpr float kHeadTwistShare = 0.45f;
constexpr float kTassetTwistShare = 0.5f;
constexpr float kTassetLeanShare = 0.3f;
// 刀を突き立てる道の中ほど（1 倍の体での局所 m・剣先の向き）：振りかぶらずに、前へ倒れ込みながら突き下ろす。
constexpr Vec3 kPlantMidGrip{-0.55f, 2.35f, 0.85f};
constexpr Vec3 kPlantMidTip{-0.08f, -0.55f, 0.83f};
// よろめき：握りが後ろ・上へ弾かれる量（1 倍の体での m）。
constexpr Vec3 kStaggerKick{0.05f, 0.18f, -0.25f};
// 膝をついた後の、肩で息をする揺れ（傾き rad・1 秒あたりの回数）。
constexpr float kHeave = 0.035f;
constexpr float kHeaveRate = 0.8f;
// 燃の終わりに、火が回って胸を起こす量（膝の傾きに対する割合）。
constexpr float kIgniteRaise = 0.45f;
// 刀身の根元（握りから剣先へ m。右手の先）と、拭い終わる所（地面からの残り m）。
constexpr float kRootAlong = 0.62f;
constexpr float kWipeStopAbove = 0.35f;
// 握って力を込める間の、刀の震え (m) と速さ（1 秒あたりの回数）。
constexpr float kStrain = 0.03f;
constexpr float kStrainRate = 19.0f;
// 力を込める体：捻り・傾き（度）・腰を落とす量。八相の体は登場の highTwistDegrees・highLeanDegrees。
constexpr float kStrainTwist = -14.0f;
constexpr float kStrainLean = 12.0f;
constexpr float kStrainSink = 0.06f;
// 斬り上げを八相が行き過ぎる量（道のりに対する割合）。斬り上げの棟の向きを測る幅。
constexpr float kRipOvershoot = 0.06f;
constexpr float kPathStep = 0.03f;
// 構え：八相から待機の構えへ移り終える所・左手を離し終える所（構えの拍の中の割合）。
constexpr float kSettleBlend = 0.7f;
constexpr float kSettleRelease = 0.45f;
// 刺さった瞬間の揺れ（0〜1）。最後の当たりの揺れ。閃光の 2 つ目の輪の遅れ (秒)。
constexpr float kPlantAccent = 0.45f;
constexpr float kHitAccent = 0.7f;
constexpr float kSecondShockDelay = 0.14f;
// 力を込める間の、刃口の光と体を包む気配（ゆがみ）の強さ。
constexpr float kStrainEdge = 2.2f;
constexpr float kStrainAura = 1.6f;
// 火の輪：出始めの大きさ（広がり切った時に対する割合）。
constexpr float kRingStart = 0.15f;
// 霧を押す高さ (m)。
constexpr float kFogPushHeight = 0.3f;
// 胸の火の粉の点（胴の模型の局所。1 倍の体での m。胴の殻の内では見えないので、襟の合わせ目の奥）と、そのちらつきの
// 速さ (rad/秒)。
constexpr Vec3 kChest{0.0f, 0.58f, 0.06f};
constexpr float kChestFlicker = 23.0f;

// いまの拍の進み（0〜1。描画のフレームの秒から。なめらかに進む）。
float Progress(const BossComponent& boss) {
    const BossPhaseShiftState& state = boss.shiftState;
    const int frames = BossPhaseShift::BeatFrames(boss, state.beat);
    return frames > 0 ? Unit(state.beatSeconds * kFramesPerSecond / static_cast<float>(frames)) : 1.0f;
}

// 演出の基準（変わり目に入った時の足元・プレイヤーの側の向き）の局所を世界へ。
Vec3 Point(const BossPhaseShiftState& state, Vec3 local) { return Add(state.origin, RotateY(local, state.yaw)); }
Vec3 Direction(const BossPhaseShiftState& state, Vec3 local) { return RotateY(local, state.yaw); }

// 地面に突き立てた刀。
Held Planted(const BossComponent& boss) {
    const BossPhaseShiftSettings& s = boss.shift;
    const BossPhaseShiftState& state = boss.shiftState;
    const float lean = s.plantLeanDegrees * kRadiansPerDegree;
    const Vec3 enter = Point(state, Mul(s.plantGround, boss.body.bodyScale));
    // 柄は体の側（後ろ）へ傾く。
    const Vec3 toward = Direction(state, {0.0f, 0.0f, -1.0f});
    const Vec3 up = Normalize(Add(Vec3{0.0f, std::cos(lean), 0.0f}, Mul(toward, std::sin(lean))));
    Held held;
    held.grip = Add(enter, Mul(up, VisibleBladeReach(boss) - s.plantBuried));
    held.tip = Mul(up, -1.0f);
    held.spine = Mul(Direction(state, Forward(s.plantEdgeYawDegrees * kRadiansPerDegree)), -1.0f);
    return held;
}

Held Local(const BossComponent& boss, Vec3 grip, Vec3 tip) {
    Held held;
    held.grip = Point(boss.shiftState, Mul(grip, boss.body.bodyScale));
    held.tip = Normalize(Direction(boss.shiftState, tip));
    return held;
}

// 膝をついた体。
Motion Kneel(const BossComponent& boss) {
    const BossPhaseShiftSettings& s = boss.shift;
    return {s.kneelTwistDegrees * kRadiansPerDegree, s.kneelLeanDegrees * kRadiansPerDegree, s.kneelSink, 0.0f};
}

// 拍ごとの体の動き。
Motion MotionOf(const BossComponent& boss) {
    const BossPhaseShiftSettings& s = boss.shift;
    const BossPhaseShiftState& state = boss.shiftState;
    const float u = Progress(boss);
    const float deg = kRadiansPerDegree;
    const Motion kneel = Kneel(boss);
    const float heave = kHeave * std::sin(kTau * kHeaveRate * state.beatSeconds);
    const Motion high{boss.intro.highTwistDegrees * deg, boss.intro.highLeanDegrees * deg, 0.0f, 1.0f};
    switch (state.beat) {
    case BossShiftBeat::Collapse: {
        // よろめいて反り、前へ崩れ落ちて膝をつく（刀が刺さる所で膝が着く）。
        const float stagger = Unit(s.staggerShare);
        const float plant = std::max(stagger + kTiny, s.plantShare);
        if (u < stagger) {
            Motion m;
            m.lean = s.staggerDegrees * deg * std::sin(kPi * 0.5f * Smooth(u / std::max(kTiny, stagger)));
            m.twist = 0.3f * s.kneelTwistDegrees * deg * Smooth(u / std::max(kTiny, stagger));
            return m;
        }
        const float t = (u - stagger) / (plant - stagger);
        Motion start;
        start.lean = s.staggerDegrees * deg;
        start.twist = 0.3f * s.kneelTwistDegrees * deg;
        // 崩れ落ちる：出だしは遅く、膝が着く所で速い（重さ）。
        Motion m = MixMotion(start, kneel, Unit(t * t * (1.6f - 0.6f * t)));
        if (u >= plant)
            m.lean += heave * Smooth((u - plant) / 0.2f);
        return m;
    }
    case BossShiftBeat::Ignite: {
        // 膝をついたまま肩で息をし、火が回るにつれて胸を起こす。
        Motion m = kneel;
        m.lean = kneel.lean * (1.0f - kIgniteRaise * Smooth((u - 0.5f) / 0.5f)) + heave;
        m.twoHand = Smooth((u - s.wipeEndShare) / std::max(kTiny, 1.0f - s.wipeEndShare));
        return m;
    }
    case BossShiftBeat::Burst: {
        // 立ち上がりながら柄を両手で握って力を込め、刀が地面を離れると斬り上げて八相に収まる。
        Motion raised = kneel;
        raised.lean = kneel.lean * (1.0f - kIgniteRaise);
        raised.twoHand = 1.0f;
        const Motion strain{kStrainTwist * deg, kStrainLean * deg, kStrainSink, 1.0f};
        const float draw = Unit(s.drawShare);
        const float rip = std::max(draw + kTiny, s.ripShare);
        if (u < draw)
            return MixMotion(raised, strain, Smooth(u / std::max(kTiny, draw)));
        return MixMotion(strain, high, Smooth((u - draw) / (rip - draw)));
    }
    case BossShiftBeat::Settle: {
        Motion m = MixMotion(high, {}, Smooth(u / kSettleBlend));
        m.twoHand = 1.0f - Smooth(u / kSettleRelease);
        return m;
    }
    default: return {};
    }
}

// 刀と右手（世界）。
Held SwordOf(const BossComponent& boss, const BossBodyPose& solved) {
    const BossPhaseShiftSettings& s = boss.shift;
    const BossPhaseShiftState& state = boss.shiftState;
    const float u = Progress(boss);
    const Held idle = HeldOf(solved.sword);
    const Held planted = Planted(boss);
    switch (state.beat) {
    case BossShiftBeat::Collapse: {
        const float stagger = Unit(s.staggerShare);
        const float plant = std::max(stagger + kTiny, s.plantShare);
        if (u < stagger) {
            // よろめき：握りが後ろ・上へ弾かれる。
            Held held = idle;
            const float kick = std::sin(kPi * Smooth(u / std::max(kTiny, stagger)) * 0.5f);
            held.grip = Add(held.grip, Mul(Direction(state, Mul(kStaggerKick, boss.body.bodyScale)), kick));
            return held;
        }
        if (u >= plant)
            return planted;
        // 倒れ込みながら突き下ろす（終わりで速い）。
        const float t = (u - stagger) / (plant - stagger);
        Held start = idle;
        start.grip = Add(start.grip, Mul(Direction(state, Mul(kStaggerKick, boss.body.bodyScale)), 1.0f));
        const Held mid = Local(boss, kPlantMidGrip, kPlantMidTip);
        Held held = Sweep(start, mid, planted, t * t);
        held.spine = Normalize(Lerp(start.spine, planted.spine, Smooth(t)), planted.spine);
        return held;
    }
    case BossShiftBeat::Ignite: return planted;
    case BossShiftBeat::Burst: {
        const float draw = Unit(s.drawShare);
        const float rip = std::max(draw + kTiny, s.ripShare);
        if (u < draw) {
            // 両手で握って力を込める：刀の震えが強まる。
            Held held = planted;
            const float strain = kStrain * Smooth(u / std::max(kTiny, draw));
            const float t = state.beatSeconds * kTau * kStrainRate;
            held.grip.x += strain * std::sin(t);
            held.grip.z += strain * std::sin(t * 1.31f + 1.0f);
            return held;
        }
        // 斬り上げ：地面を裂いて一気に抜き、体の右の外を通って八相へ（刃が先。登場の抜刀と同じ道）。
        const BossIntroSettings& intro = boss.intro;
        const Held ripMid = Local(boss, intro.drawRipGrip, intro.drawRipTip);
        const Held high = Local(boss, intro.highGrip, intro.highTip);
        const float t = (u - draw) / (rip - draw);
        Held held =
            SweepEdged(planted, ripMid, high, RipAlong(t, kRipOvershoot), -1.0f, kPathStep, VisibleBladeReach(boss));
        held.spine = Normalize(Lerp(planted.spine, held.spine, Smooth(t / 0.3f)), held.spine);
        return held;
    }
    case BossShiftBeat::Settle: {
        const BossIntroSettings& intro = boss.intro;
        const Held ripMid = Local(boss, intro.drawRipGrip, intro.drawRipTip);
        const Held high = Local(boss, intro.highGrip, intro.highTip);
        const Held top = SweepEdged(planted, ripMid, high, 1.0f, -1.0f, kPathStep, VisibleBladeReach(boss));
        return Mix(top, idle, Smooth(u / kSettleBlend));
    }
    default: return idle;
    }
}

// 左手が刀身を拭う所（握りから剣先へ m）。拭っていない間は負。
float WipeAlong(const BossComponent& boss) {
    const BossPhaseShiftSettings& s = boss.shift;
    const BossPhaseShiftState& state = boss.shiftState;
    if (state.beat != BossShiftBeat::Ignite)
        return -1.0f;
    const float u = Progress(boss);
    const float end = std::max(kRootAlong, VisibleBladeReach(boss) - s.plantBuried - kWipeStopAbove);
    const float t = (u - s.wipeStartShare) / std::max(kTiny, s.wipeEndShare - s.wipeStartShare);
    return Lerp(kRootAlong, end, Smooth(Unit(t)));
}

// 刀身に灯った火の、根元から届いた所（握りから剣先へ m。灯っていなければ負）。
float LitAlong(const BossComponent& boss) {
    const BossPhaseShiftSettings& s = boss.shift;
    const BossPhaseShiftState& state = boss.shiftState;
    if (state.beat == BossShiftBeat::Ignite) {
        const float u = Progress(boss);
        return u < s.wipeStartShare ? -1.0f : WipeAlong(boss);
    }
    if (state.beat == BossShiftBeat::Burst && !state.flashed)
        return std::max(kRootAlong, VisibleBladeReach(boss) - s.plantBuried - kWipeStopAbove);
    return -1.0f;
}

// 刀身を拭う左手：刀身のその所を、両手持ちと同じ向きの拳で握って滑らせる。
BossBodyPlacement WipeHand(const BossComponent& boss, const BossBodyPlacement& sword, float along, Vec3 shoulder) {
    BossBodyPlacement at = sword;
    const Vec3 z = Rotate(sword.rotation, {0.0f, 0.0f, 1.0f});
    // TwoHandLeft は握りから twoHandBehind だけ柄頭の側へ置くので、その分を前へずらして、刀身の along の所に置く。
    at.position = Add(sword.position, Mul(z, along + boss.body.leftHand.twoHandBehind));
    return TwoHandLeft(boss, at, shoulder);
}

void Inject(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, uint64_t id,
            HIKARI::GameProjectFluidInteractionKind kind, Vec3 at, Vec3 velocity, float strength, float radius,
            float heat) {
    if (!(strength > 0.0f))
        return;
    HIKARI::GameProjectFluidInteraction emit{};
    emit.kind = kind;
    emit.position = at;
    // extents.x は半径、extents.y は温度。
    emit.extents = {std::max(0.02f, radius), heat, 0.0f};
    emit.direction = velocity;
    emit.strength = std::min(10000.0f, strength);
    // 胴の煙の域だけに届く（場の霧・場の火には入れない）。
    emit.channels = FluidChannels::kBodyFire;
    boss.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), id, emit).Accepted() && boss.fluidAccepted;
}

// 継ぎ目から漏れる黒い煙の重み（0〜1）：崩で満ち、燃の間に火に替わって引き、刀を抜く前に消える。
float LeakWeight(const BossComponent& boss) {
    const BossPhaseShiftState& state = boss.shiftState;
    const float u = Progress(boss);
    switch (state.beat) {
    case BossShiftBeat::Collapse: return Smooth((u - 0.05f) / 0.3f);
    case BossShiftBeat::Ignite: return 1.0f - 0.75f * Smooth(u);
    case BossShiftBeat::Burst: return 0.25f * (1.0f - Smooth(u / std::max(kTiny, boss.shift.drawShare)));
    default: return 0.0f;
    }
}

// 火の輪：閃光で出し、広がり切るまで燃料を出し、その後は光を下げて消す（流体の炎は自分で燃え尽きる）。
void UpdateRing(HIKARI::WorldView& world, BossComponent& boss, float seconds) {
    BossPhaseShiftState& state = boss.shiftState;
    const BossPhaseShiftSettings& s = boss.shift;
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
    const float grow = Unit(state.ringAge / std::max(0.001f, s.ringSeconds));
    // 資産の細い輪（シェーダーの層）は ringThinGlow を掛ける（既定 0：出さない。流体の炎の帯は入力 fly で出る）。
    const float glow = (fade <= 0.0f ? 1.0f : 1.0f - fade / std::max(0.001f, s.ringFadeSeconds)) *
                       std::clamp(s.ringThinGlow, 0.0f, 1.0f);
    // 広がりは出だしが速い（場を払う）。
    const float spread = 1.0f - (1.0f - grow) * (1.0f - grow);
    const float scale = Lerp(kRingStart, 1.0f, spread) * s.ringRadius / std::max(0.01f, boss.ringEffectRadius);
    const Vec3 center{state.origin.x, state.origin.y + boss.ringHeight, state.origin.z};
    BossVfx::Send(world, boss, state.ringVfx, BossVfx::Placed(center, scale),
                  BossVfx::RingParameters(fade <= 0.0f, glow));
}

// 閃光（爆の、刀が地面を離れた瞬間。変わり目でいちばん強い拍）：白黒の閃き・短いスロー・強い揺れ・衝撃の輪・刀身の光・
// 目の閃き・火の輪・霧を外へ押す・場の色を一気に進める（いずれも登場の頼みの口を使う）。
void Flash(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss) {
    const BossPhaseShiftSettings& s = boss.shift;
    BossPhaseShiftState& state = boss.shiftState;
    BossIntroState& look = boss.introState;
    look.monoPending = true;
    if (s.flashSlowFrames > 0)
        look.slowPending = s.flashSlowFrames;
    look.accentPending = 1.0f;
    look.shocks[0] = {state.origin, s.flashShock, 0.0f};
    look.shocks[1] = {state.origin, s.flashShockSecond, kSecondShockDelay};
    look.glint = s.flashGlint;
    boss.bodyState.accents.flare = std::max(boss.bodyState.accents.flare, boss.intro.eyesFlare);
    boss.bodyState.fieldWarm = std::max(boss.bodyState.fieldWarm, std::clamp(s.flashWarm, 0.0f, 1.0f));
    if (s.ringRadius > 0.0f && !boss.ringEffect.IsEmpty()) {
        BossVfx::StopHandle(world, state.ringVfx);
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = boss.ringEffect;
        const Vec3 center{state.origin.x, state.origin.y + boss.ringHeight, state.origin.z};
        play.transform = BossVfx::Placed(center, kRingStart * s.ringRadius / std::max(0.01f, boss.ringEffectRadius));
        // 出始めのフレームから細い輪を出さない（ringThinGlow。地面の予告に見えないように）。
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
}

// 飛ばした時に吸い消す所を覚える：刀を突き立てている間（崩・燃・閃光の前の爆）の、刀身（根元から地面の上まで）・柄と
// その上・両の籠手・胸。構え（立ち上がった後）では覚えない（構え直した体の火と重なる）。
void RememberQuench(BossComponent& boss, const BossBodyPose& pose) {
    const BossPhaseShiftSettings& s = boss.shift;
    BossPhaseShiftState& state = boss.shiftState;
    const bool planted = state.beat == BossShiftBeat::Collapse || state.beat == BossShiftBeat::Ignite ||
                         (state.beat == BossShiftBeat::Burst && !state.flashed);
    if (!planted) {
        state.quenchCount = 0;
        return;
    }
    const Vec3 grip = pose.sword.position;
    const Vec3 tip = Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
    const float end = std::max(kRootAlong, VisibleBladeReach(boss) - s.plantBuried - kWipeStopAbove);
    // 刀身の火は昇るので、柄の上にも球の半径ずつ 2 つ重ねる（左手は拭う間は刀身の上なので、刀身の球で足りる）。
    const float rise = std::max(0.05f, s.skipQuenchRadius * boss.body.bodyScale);
    int k = 0;
    for (float up : {0.0f, 1.0f, 2.0f})
        state.quenchAt[static_cast<size_t>(k++)] = Add(grip, Vec3{0.0f, rise * up, 0.0f});
    for (float share : {0.0f, 0.5f, 1.0f})
        state.quenchAt[static_cast<size_t>(k++)] = Add(grip, Mul(tip, Lerp(kRootAlong, end, share)));
    state.quenchAt[static_cast<size_t>(k++)] = pose.rightCuff.position;
    state.quenchAt[static_cast<size_t>(k++)] = pose.leftCuff.position;
    // 膝をついた胸の火の粉（燃の間に入れる）と、その上。立ち上がった胴の前に残って見えないように。
    const Vec3 chest = Add(pose.torso.position, Rotate(pose.torso.rotation, Mul(kChest, pose.torso.scale)));
    state.quenchAt[static_cast<size_t>(k++)] = chest;
    state.quenchAt[static_cast<size_t>(k++)] = Add(chest, Vec3{0.0f, rise, 0.0f});
    state.quenchCount = k;
}

// 飛ばした時：覚えた所の胴の煙の域を、quench の秒数だけ吸い消す（煙・熱・燃料が消える。押しはしない）。飛ばしたのを
// 見たフレームは skipFrame（火の舌を入れない）。
void Quench(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds) {
    const BossPhaseShiftSettings& s = boss.shift;
    BossPhaseShiftState& state = boss.shiftState;
    state.skipFrame = false;
    if (state.skipped && !state.skipSeen) {
        state.skipSeen = true;
        state.skipFrame = true;
        state.quenchLeft = std::max(0.0f, s.skipQuenchSeconds);
        char message[128]{};
        std::snprintf(message, sizeof(message), "ShiftQuench tick=%llu points=%d seconds=%.2f",
                      static_cast<unsigned long long>(boss.tick), state.quenchCount,
                      static_cast<double>(state.quenchLeft));
        BossLog::Write(message);
    }
    if (!(state.quenchLeft > 0.0f) || state.quenchCount <= 0 || !(s.skipQuenchRate > 0.0f))
        return;
    state.quenchLeft -= std::max(0.0f, seconds);
    for (int k = 0; k < state.quenchCount; ++k) {
        HIKARI::GameProjectFluidInteraction sink{};
        sink.kind = HIKARI::GameProjectFluidInteractionKind::Force;
        sink.position = state.quenchAt[static_cast<size_t>(k)];
        sink.extents = {std::max(0.05f, s.skipQuenchRadius * boss.body.bodyScale), 0.0f, 0.0f};
        sink.strength = 0.0f;
        sink.absorption = std::min(1000.0f, s.skipQuenchRate);
        sink.channels = FluidChannels::kBodyFire;
        boss.fluidAccepted =
            world.GetFluid().Submit(object.GetHandle(), kQuenchBase + static_cast<uint64_t>(k), sink).Accepted() &&
            boss.fluidAccepted;
    }
}

} // namespace

void BossPhaseShiftVisuals::Advance(BossComponent& boss, float seconds) {
    BossPhaseShiftState& state = boss.shiftState;
    if (state.beat != state.seenBeat) {
        state.seenBeat = state.beat;
        state.beatSeconds = 0.0f;
    }
    state.beatSeconds += std::max(0.0f, seconds);
}

float BossPhaseShiftVisuals::FireFloor(const BossComponent& boss) {
    const BossPhaseShiftState& state = boss.shiftState;
    const float fire = std::clamp(boss.shift.igniteFire, 0.0f, 1.0f);
    if (state.beat == BossShiftBeat::Ignite)
        return fire * Smooth((Progress(boss) - 0.1f) / 0.9f);
    if (state.beat == BossShiftBeat::Burst)
        return fire;
    return 0.0f;
}

void BossPhaseShiftVisuals::ApplyPose(BossComponent& boss, const HIKARI::GameProjectTransform& body,
                                      BossBodyPose& pose) {
    if (!BossPhaseShift::Cinematic(boss))
        return;
    const BossPhaseShiftSettings& s = boss.shift;
    const BossPhaseShiftState& state = boss.shiftState;
    const BossBodySettings& b = boss.body;
    const BossBodyPose solved = pose;
    const Motion motion = MotionOf(boss);
    const float u = Progress(boss);

    // 刀と右手。
    const Held sword = SwordOf(boss, solved);
    pose.sword = Place(sword, SwordScale(boss));
    pose.rightHand = Place(sword, HandScale(boss));
    pose.showHands = true;
    // 刀身は、拭い終わった所から灼けた刀身（その前は、拭った所まで流体の火が灯る）。
    pose.bladeHot = BossBodyMath::PhaseTwoLook(boss) || (state.beat == BossShiftBeat::Ignite && u >= s.wipeEndShare) ||
                    state.beat == BossShiftBeat::Burst;
    const float pivot = BodyPoint(b, {0.0f, kLeanPivot, 0.0f}).y;
    const Vec3 rightShoulder =
        BendPoint(state, motion, pivot, BodyToWorld(boss, body, BodyPoint(b, b.shoulder)), 1.0f, 1.0f);
    const float handScale = HandScale(boss);
    const Vec3 rightWrist = Add(pose.rightHand.position, Rotate(pose.rightHand.rotation, Mul(b.gripWrist, handScale)));
    const Vec3 gripElbow = Normalize(Rotate(pose.rightHand.rotation, b.gripElbow));
    const Vec3 rightElbow = Lerp(Normalize(Sub(rightShoulder, rightWrist), gripElbow), gripElbow, b.cuffGripBlend);
    pose.rightCuff =
        Cuff(rightWrist, rightElbow, Rotate(pose.rightHand.rotation, b.gripBack), handScale, b, rightShoulder);

    // 鎧：膝をつく・起き上がる体の動き（胴・大袖・左手は丸ごと、兜と草摺は少なめに付く）。
    const auto bend = [&](BossBodyPlacement& placement, float twistShare, float leanShare) {
        placement.position = BendPoint(state, motion, pivot, placement.position, twistShare, leanShare);
        placement.rotation = Multiply(BendTurn(state, motion, twistShare, leanShare), placement.rotation);
    };
    bend(pose.torso, 1.0f, 1.0f);
    bend(pose.sode[0], 1.0f, 1.0f);
    bend(pose.sode[1], 1.0f, 1.0f);
    bend(pose.leftCuff, 1.0f, 1.0f);
    bend(pose.leftHand, 1.0f, 1.0f);
    bend(pose.head, kHeadTwistShare, 1.0f);
    for (int k = 0; k < pose.tassetCount; ++k)
        bend(pose.tassets[static_cast<size_t>(k)], kTassetTwistShare, kTassetLeanShare);

    // 左手：燃の間は刀身の根元へ伸ばして先へ拭い、柄へ戻って両手で握る。爆・構えは両手持ち（構えで離す）。
    const Vec3 right = BodyPoint(b, b.shoulder);
    const Vec3 leftShoulder =
        BendPoint(state, motion, pivot, BodyToWorld(boss, body, {-right.x, right.y, right.z}), 1.0f, 1.0f);
    BossBodyPlacement left = pose.leftHand;
    float leftWeight = 0.0f;
    if (state.beat == BossShiftBeat::Ignite) {
        const float wipe = WipeAlong(boss);
        const BossBodyPlacement onBlade = WipeHand(boss, pose.sword, wipe, leftShoulder);
        const BossBodyPlacement onHilt = TwoHandLeft(boss, pose.sword, leftShoulder);
        if (u < s.wipeEndShare) {
            left = onBlade;
            leftWeight = Smooth(u / std::max(kTiny, s.wipeReachShare));
        } else {
            const float back = Smooth((u - s.wipeEndShare) / std::max(kTiny, 1.0f - s.wipeEndShare));
            left = onBlade;
            left.position = Lerp(onBlade.position, onHilt.position, back);
            left.rotation = Blend(onBlade.rotation, onHilt.rotation, back);
            leftWeight = 1.0f;
        }
    } else if (motion.twoHand > 0.0f) {
        left = TwoHandLeft(boss, pose.sword, leftShoulder);
        leftWeight = Smooth(motion.twoHand);
    }
    if (leftWeight > 0.0f) {
        pose.leftHand.position = Lerp(pose.leftHand.position, left.position, leftWeight);
        pose.leftHand.rotation = Blend(pose.leftHand.rotation, left.rotation, leftWeight);
        if (leftWeight > 0.5f)
            pose.leftShape = BossBodyHandShape::Fist;
        const Vec3 back = Rotate(pose.leftHand.rotation, {0.0f, 1.0f, 0.0f});
        pose.leftCuff =
            Cuff(pose.leftHand.position, Sub(leftShoulder, pose.leftHand.position), back, handScale, b, leftShoulder);
    }

    // 空中で 1 本目が空になった時：崩の始めに、元の高さから落ちて来る（体の物体は地面にあるので、外形だけを上げる）。
    if (state.beat == BossShiftBeat::Collapse && state.fallHeight > 0.0f) {
        const float t = Unit(u / std::max(kTiny, s.fallShare));
        const Vec3 lift{0.0f, state.fallHeight * (1.0f - t * t), 0.0f};
        for (BossBodyPlacement* placement : {&pose.sword, &pose.rightHand, &pose.rightCuff, &pose.leftHand,
                                             &pose.leftCuff, &pose.torso, &pose.head, &pose.sode[0], &pose.sode[1]})
            placement->position = Add(placement->position, lift);
        for (int k = 0; k < pose.tassetCount; ++k)
            pose.tassets[static_cast<size_t>(k)].position = Add(pose.tassets[static_cast<size_t>(k)].position, lift);
    }

    // 刃口の光・体を包む気配：力を込める間に強まり、斬り上げで刀光を引く。
    BossIntroState& look = boss.introState;
    if (state.beat == BossShiftBeat::Burst) {
        const float draw = Unit(s.drawShare);
        const float charge = Smooth(u / std::max(kTiny, draw));
        look.bladeEdge =
            std::max(look.bladeEdge, kStrainEdge * (state.flashed ? 1.0f - Smooth((u - draw) / 0.3f) : charge));
        look.aura = std::max(look.aura, kStrainAura * (state.flashed ? 0.0f : charge));
        look.bladeSwing = u >= draw && u < std::max(draw + kTiny, s.ripShare);
    }
}

void BossPhaseShiftVisuals::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                                   const BossBodyPose& pose, float seconds) {
    BossPhaseShiftState& state = boss.shiftState;
    const BossPhaseShiftSettings& s = boss.shift;
    const float dt = std::max(0.0f, seconds);
    UpdateRing(world, boss, dt);
    Quench(world, object, boss, dt);
    if (!BossPhaseShift::Cinematic(boss))
        return;
    RememberQuench(boss, pose);
    // 遅れて出る衝撃の輪の時計（登場の後は、登場の見た目が進めない）。
    for (BossIntroState::Shock& shock : boss.introState.shocks)
        shock.delay -= dt;
    const float u = Progress(boss);
    const uint64_t owner = object.GetHandle().value;
    // 最後の当たり：時間を一瞬止め、揺らす。
    if (state.beat == BossShiftBeat::Collapse && !state.hitSeen) {
        state.hitSeen = true;
        if (s.hitStopFrames > 0)
            Feedback::RequestHitStop(world, kHitStopSource ^ owner, Feedback::kBreakPriority, s.hitStopFrames,
                                     s.hitStopScale);
        boss.introState.accentPending = std::max(boss.introState.accentPending, kHitAccent);
    }
    // 刀が地面に刺さった瞬間：揺れ（地面の輪は出さない。細い輪は予告に見える）。
    if (state.beat == BossShiftBeat::Collapse && u >= s.plantShare && !state.plantSeen) {
        state.plantSeen = true;
        boss.introState.accentPending = std::max(boss.introState.accentPending, kPlantAccent);
    }
    if (state.flashed && !state.flashSeen) {
        state.flashSeen = true;
        if (!state.skipped)
            Flash(world, object, boss);
    }

    const float scale = boss.body.bodyScale;
    // 継ぎ目から漏れる黒い煙：襟の左右、大袖の付け根、両の袖口、草摺の上の縁から、外へ吹き出す。
    const float leak = s.leakDensity * LeakWeight(boss);
    if (leak > 0.0f) {
        const Vec3 center = pose.torso.position;
        const float radius = s.leakRadius * scale;
        int k = 0;
        const auto seam = [&](Vec3 at, float share) {
            const Vec3 out = Normalize(Add(FlatNormalize(Sub(at, center), Forward(state.yaw)), Vec3{0.0f, 0.6f, 0.0f}));
            // 濃さのむら（継ぎ目ごとに脈打つ）。
            const float pulse = 0.75f + 0.25f * std::sin(state.beatSeconds * 5.3f + 1.7f * static_cast<float>(k));
            Inject(world, object, boss, kLeakBase + static_cast<uint64_t>(k++),
                   HIKARI::GameProjectFluidInteractionKind::Emit, at, Mul(out, s.leakSpeed), leak * share * pulse,
                   radius, 0.0f);
        };
        for (float side : {1.0f, -1.0f}) {
            const Vec3 collar =
                Mul({boss.body.fireCollar.x * side, boss.body.fireCollar.y, boss.body.fireCollar.z}, pose.torso.scale);
            seam(Add(pose.torso.position, Rotate(pose.torso.rotation, collar)), 1.0f);
        }
        seam(pose.sode[0].position, 0.8f);
        seam(pose.sode[1].position, 0.8f);
        seam(pose.rightCuff.position, 0.6f);
        seam(pose.leftCuff.position, 0.6f);
        for (int t = 0; t < pose.tassetCount && k < 14; t += 2)
            seam(pose.tassets[static_cast<size_t>(t)].position, 0.7f);
    }
    // 胸の煙の中の火の粉：燃の始めに灯り、火が回るにつれて消える（胴の火に引き継ぐ）。
    if (state.beat == BossShiftBeat::Ignite && s.chestFuel > 0.0f) {
        const float glow = Smooth(u / 0.12f) * (1.0f - Smooth((u - 0.45f) / 0.4f));
        const float flicker = 0.7f + 0.3f * std::sin(state.beatSeconds * kChestFlicker);
        const Vec3 chest = Add(pose.torso.position, Rotate(pose.torso.rotation, Mul(kChest, pose.torso.scale)));
        Inject(world, object, boss, kChestId, HIKARI::GameProjectFluidInteractionKind::Fuel, chest, {0.0f, 0.8f, 0.0f},
               s.chestFuel * glow * flicker, s.chestRadius * scale, boss.body.fireTongueHeat);
    }
    // 刀身に灯る火：拭った所まで、根元から燃料を出す（刀を抜く閃光で灼けた刀身と刀の火へ引き継ぐ）。
    const float lit = LitAlong(boss);
    if (lit > 0.0f && s.bladeFuel > 0.0f) {
        const Vec3 grip = pose.sword.position;
        const Vec3 tip = Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
        for (int k = 0; k < kBladeFirePoints; ++k) {
            const float along = Lerp(kRootAlong, lit, (static_cast<float>(k) + 0.5f) / kBladeFirePoints);
            if (along > lit)
                break;
            Inject(world, object, boss, kBladeFireBase + static_cast<uint64_t>(k),
                   HIKARI::GameProjectFluidInteractionKind::Fuel, Add(grip, Mul(tip, along)), {0.0f, 1.2f, 0.0f},
                   s.bladeFuel / kBladeFirePoints, s.bladeFuelRadius, s.bladeFuelHeat);
        }
    }
}

bool BossPhaseShiftVisuals::HoldTongues(const BossComponent& boss) { return boss.shiftState.skipFrame; }

void BossPhaseShiftVisuals::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    BossVfx::StopHandle(world, boss.shiftState.ringVfx);
}

} // namespace Project_TD2_2
