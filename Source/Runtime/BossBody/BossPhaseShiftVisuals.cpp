#include "BossPhaseShiftVisuals.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/FluidView.h"
#include "BossBodyMath.h"
#include "BossCinematicFx.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossPhaseShift.h"
#include "../Boss/BossVfx.h"
#include "../Player/Feedback.h"
#include "../Common/FluidChannels.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;

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
// 刀身の根元（握りから剣先へ m。右手の先）と、拭い終わる所（地面からの残り m）。
constexpr float kRootAlong = 0.62f;
constexpr float kWipeStopAbove = 0.35f;
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
float Progress(const BossPhaseShiftComponent& shift) {
    const BossPhaseShiftState& state = shift.state;
    const int frames = BossPhaseShift::BeatFrames(shift, state.beat);
    return frames > 0 ? Clamp01(state.beatSeconds * kFramesPerSecond / static_cast<float>(frames)) : 1.0f;
}

// 刀身の火が根元から届いた所（握りから剣先へ m）。燃の外は負。
float WipeAlong(const BossComponent& boss, const BossBodySettings& body, const BossPhaseShiftComponent& shift) {
    const BossPhaseShiftSettings& s = shift.settings;
    const BossPhaseShiftState& state = shift.state;
    if (state.beat != BossShiftBeat::Ignite)
        return -1.0f;
    const float u = Progress(shift);
    const float end = std::max(kRootAlong, VisibleBladeReach(boss, body) - s.plantBuried - kWipeStopAbove);
    const float t = (u - s.wipeStartShare) / std::max(kTiny, s.wipeEndShare - s.wipeStartShare);
    return Lerp(kRootAlong, end, Smooth(Clamp01(t)));
}

// 刀身に灯った火の、根元から届いた所（握りから剣先へ m。灯っていなければ負）。
float LitAlong(const BossComponent& boss, const BossBodySettings& body, const BossPhaseShiftComponent& shift) {
    const BossPhaseShiftSettings& s = shift.settings;
    const BossPhaseShiftState& state = shift.state;
    if (state.beat == BossShiftBeat::Ignite) {
        const float u = Progress(shift);
        return u < s.wipeStartShare ? -1.0f : WipeAlong(boss, body, shift);
    }
    if (state.beat == BossShiftBeat::Burst && !state.flashed)
        return std::max(kRootAlong, VisibleBladeReach(boss, body) - s.plantBuried - kWipeStopAbove);
    return -1.0f;
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
float LeakWeight(const BossPhaseShiftComponent& shift) {
    const BossPhaseShiftState& state = shift.state;
    const float u = Progress(shift);
    switch (state.beat) {
    case BossShiftBeat::Collapse: return Smooth((u - 0.05f) / 0.3f);
    case BossShiftBeat::Ignite: return 1.0f - 0.75f * Smooth(u);
    case BossShiftBeat::Burst: return 0.25f * (1.0f - Smooth(u / std::max(kTiny, shift.settings.drawShare)));
    default: return 0.0f;
    }
}

// 火の輪：閃光で出し、広がり切るまで燃料を出し、その後は光を下げて消す（流体の炎は自分で燃え尽きる）。
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
// 目の閃き・火の輪・霧を外へ押す・場の色を一気に進める（いずれも演出の効果の頼みの口 BossCinematicFx を使う）。
void Flash(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, BossBodyComponent& body,
           BossPhaseShiftComponent& shift) {
    const BossPhaseShiftSettings& s = shift.settings;
    BossPhaseShiftState& state = shift.state;
    BossCinematicFxState& look = boss.cinematicFxState;
    look.monoPending = true;
    if (s.flashSlowFrames > 0)
        look.slowPending = s.flashSlowFrames;
    look.accentPending = 1.0f;
    look.shocks[0] = {state.origin, s.flashShock, 0.0f};
    look.shocks[1] = {state.origin, s.flashShockSecond, kSecondShockDelay};
    look.glint = s.flashGlint;
    BossCinematicFx::EyeFlare(boss);
    body.state.fieldWarm = std::max(body.state.fieldWarm, std::clamp(s.flashWarm, 0.0f, 1.0f));
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

// 飛ばした時に吸い消す所を覚える：閃光の前（崩・燃・閃光の前の爆）の、刀身（根元から火が届く所まで）・柄とその上・
// 両の籠手・胸。閃光の後は覚えない（構え直した体の火と重なる）。
void RememberQuench(const BossComponent& boss, const BossBodyComponent& body, BossPhaseShiftComponent& shift,
                    const BossBodyPose& pose) {
    const BossPhaseShiftSettings& s = shift.settings;
    BossPhaseShiftState& state = shift.state;
    const bool planted = state.beat == BossShiftBeat::Collapse || state.beat == BossShiftBeat::Ignite ||
                         (state.beat == BossShiftBeat::Burst && !state.flashed);
    if (!planted) {
        state.quenchCount = 0;
        return;
    }
    const Vec3 grip = pose.sword.position;
    const Vec3 tip = Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
    const float end = std::max(kRootAlong, VisibleBladeReach(boss, body.settings) - s.plantBuried - kWipeStopAbove);
    // 刀身の火は昇るので、柄の上にも球の半径ずつ 2 つ重ねる。
    const float rise = std::max(0.05f, s.skipQuenchRadius * body.settings.bodyScale);
    int k = 0;
    for (float up : {0.0f, 1.0f, 2.0f})
        state.quenchAt[static_cast<size_t>(k++)] = Add(grip, Vec3{0.0f, rise * up, 0.0f});
    for (float share : {0.0f, 0.5f, 1.0f})
        state.quenchAt[static_cast<size_t>(k++)] = Add(grip, Mul(tip, Lerp(kRootAlong, end, share)));
    state.quenchAt[static_cast<size_t>(k++)] = pose.rightCuff.position;
    state.quenchAt[static_cast<size_t>(k++)] = pose.leftCuff.position;
    // 胸の火の粉（燃の間に入れる）と、その上。構え直した胴の前に残って見えないように。
    const Vec3 chest = Add(pose.torso.position, Rotate(pose.torso.rotation, Mul(kChest, pose.torso.scale)));
    state.quenchAt[static_cast<size_t>(k++)] = chest;
    state.quenchAt[static_cast<size_t>(k++)] = Add(chest, Vec3{0.0f, rise, 0.0f});
    state.quenchCount = k;
}

// 飛ばした時：覚えた所の胴の煙の域を、quench の秒数だけ吸い消す（煙・熱・燃料が消える。押しはしない）。飛ばしたのを
// 見たフレームは skipFrame（火の舌を入れない）。
void Quench(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, const BossBodyComponent& body,
            BossPhaseShiftComponent& shift, float seconds) {
    const BossPhaseShiftSettings& s = shift.settings;
    BossPhaseShiftState& state = shift.state;
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
        sink.extents = {std::max(0.05f, s.skipQuenchRadius * body.settings.bodyScale), 0.0f, 0.0f};
        sink.strength = 0.0f;
        sink.absorption = std::min(1000.0f, s.skipQuenchRate);
        sink.channels = FluidChannels::kBodyFire;
        boss.fluidAccepted =
            world.GetFluid().Submit(object.GetHandle(), kQuenchBase + static_cast<uint64_t>(k), sink).Accepted() &&
            boss.fluidAccepted;
    }
}

} // namespace

void BossPhaseShiftVisuals::Advance(BossPhaseShiftComponent* shift, float seconds) {
    if (shift == nullptr)
        return;
    BossPhaseShiftState& state = shift->state;
    if (state.beat != state.seenBeat) {
        state.seenBeat = state.beat;
        state.beatSeconds = 0.0f;
    }
    state.beatSeconds += std::max(0.0f, seconds);
}

float BossPhaseShiftVisuals::FireFloor(const BossPhaseShiftComponent* shift) {
    if (shift == nullptr)
        return 0.0f;
    const BossPhaseShiftState& state = shift->state;
    const float fire = std::clamp(shift->settings.igniteFire, 0.0f, 1.0f);
    if (state.beat == BossShiftBeat::Ignite)
        return fire * Smooth((Progress(*shift) - 0.1f) / 0.9f);
    if (state.beat == BossShiftBeat::Burst)
        return fire;
    return 0.0f;
}

void BossPhaseShiftVisuals::ApplyPose(BossComponent& boss, BossPhaseShiftComponent* shift, BossBodyPose& pose) {
    if (!BossPhaseShift::Cinematic(shift))
        return;
    const BossPhaseShiftSettings& s = shift->settings;
    const BossPhaseShiftState& state = shift->state;
    const float u = Progress(*shift);
    // 体は骨格の体が待機の姿で演じる（c-db1a15）。ここは刀身の灼けと刃口の光・気配だけ：拭い終わった所から灼けた刀身。
    pose.bladeHot = BossBodyMath::PhaseTwoLook(boss) || (state.beat == BossShiftBeat::Ignite && u >= s.wipeEndShare) ||
                    state.beat == BossShiftBeat::Burst;

    // 刃口の光・体を包む気配：力を込める間に強まり、斬り上げで刀光を引く。
    BossCinematicFxState& look = boss.cinematicFxState;
    if (state.beat == BossShiftBeat::Burst) {
        const float draw = Clamp01(s.drawShare);
        const float charge = Smooth(u / std::max(kTiny, draw));
        look.bladeEdge =
            std::max(look.bladeEdge, kStrainEdge * (state.flashed ? 1.0f - Smooth((u - draw) / 0.3f) : charge));
        look.aura = std::max(look.aura, kStrainAura * (state.flashed ? 0.0f : charge));
        look.bladeSwing = u >= draw && u < std::max(draw + kTiny, s.ripShare);
    }
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
    if (!BossPhaseShift::Cinematic(shift))
        return;
    RememberQuench(boss, body, *shift, pose);
    // 遅れて出る衝撃の輪の時計（登場の後は、登場の見た目が進めない）。
    for (BossCinematicFxState::Shock& shock : boss.cinematicFxState.shocks)
        shock.delay -= dt;
    const float u = Progress(*shift);
    const uint64_t owner = object.GetHandle().value;
    // 最後の当たり：時間を一瞬止め、揺らす。
    if (state.beat == BossShiftBeat::Collapse && !state.hitSeen) {
        state.hitSeen = true;
        if (s.hitStopFrames > 0)
            Feedback::RequestHitStop(world, kHitStopSource ^ owner, Feedback::kBreakPriority, s.hitStopFrames,
                                     s.hitStopScale);
        boss.cinematicFxState.accentPending = std::max(boss.cinematicFxState.accentPending, kHitAccent);
    }
    // 刀が地面に刺さった瞬間：揺れ（地面の輪は出さない。細い輪は予告に見える）。
    if (state.beat == BossShiftBeat::Collapse && u >= s.plantShare && !state.plantSeen) {
        state.plantSeen = true;
        boss.cinematicFxState.accentPending = std::max(boss.cinematicFxState.accentPending, kPlantAccent);
    }
    if (state.flashed && !state.flashSeen) {
        state.flashSeen = true;
        if (!state.skipped)
            Flash(world, object, boss, body, *shift);
    }

    const float scale = body.settings.bodyScale;
    // 継ぎ目から漏れる黒い煙：襟の左右、大袖の付け根、両の袖口、草摺の上の縁から、外へ吹き出す。
    const float leak = s.leakDensity * LeakWeight(*shift);
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
                Mul({body.settings.fireCollar.x * side, body.settings.fireCollar.y, body.settings.fireCollar.z},
                    pose.torso.scale);
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
               s.chestFuel * glow * flicker, s.chestRadius * scale, body.settings.fireTongueHeat);
    }
    // 刀身に灯る火：拭った所まで、根元から燃料を出す（刀を抜く閃光で灼けた刀身と刀の火へ引き継ぐ）。
    const float lit = LitAlong(boss, body.settings, *shift);
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

bool BossPhaseShiftVisuals::HoldTongues(const BossPhaseShiftComponent* shift) {
    return shift != nullptr && shift->state.skipFrame;
}

void BossPhaseShiftVisuals::Stop(HIKARI::WorldView& world, BossPhaseShiftComponent* shift) {
    if (shift != nullptr)
        BossVfx::StopHandle(world, shift->state.ringVfx);
}

} // namespace Project_TD2_2
