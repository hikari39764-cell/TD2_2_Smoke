#include "BossIntroEffects.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string_view>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/CameraViewSnapshot.h"
#include "HIKARI/Project/SequenceView.h"
#include "BossBodyMath.h"
#include "BossIntroActor.h"
#include "BossIntroSmoke.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossIntro.h"
#include "../Boss/BossVfx.h"
#include "../Combat/Feedback.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;

// 手応えの出し元の番号（ほかの手応えと重ねない）と、優先度（崩しの手応えより下）。
constexpr uint64_t kAccentCameraSource = 0x5444324A11ull;
constexpr uint64_t kAccentHitStopSource = 0x5444324A12ull;
constexpr uint64_t kAccentSlowSource = 0x5444324A14ull;
constexpr uint64_t kRumbleCameraSource = 0x5444324A15ull;
// 演出の動きのぼかしの出し元の番号（c-bc5908）。
constexpr uint64_t kMotionBlurSource = 0x5444324A16ull;
// 続けて出す揺れの 1 周（ラジアン）。全画面の効果の張りつめ・暗さを、頼まれた値へ寄せる速さ (1/秒。実時間) と、これより
// 弱ければ出さない下限。落ちている兜の隙間の瞬き：拍のこの割合までで灯り切り、残りで消える。
constexpr float kRumbleTurn = 6.2831853f;
constexpr float kPostEaseRate = 6.0f;
constexpr float kPostFloor = 0.004f;
constexpr float kGlintRise = 0.22f;
// 白黒の閃き（Action Post）の送り手。口は 1 つなので、ほかの全画面の効果（速度感）より高い優先度で出す。
constexpr uint64_t kMonoSource = 0x5444324A13ull;
constexpr int32_t kMonoPriority = 10;
constexpr int kAccentPriority = Project_TD2_2::Feedback::kBreakPriority - 10;
// 衝撃の輪の 2 つ目：1 つ目に対する大きさの倍率と、遅れて出す秒。刀が抜けた後、刀光を引く長さ (秒)。
constexpr float kSecondShockScale = 1.4f;
constexpr float kSecondShockDelay = 0.12f;
constexpr float kDrawSwingSeconds = 0.6f;
// 体を包む気配の強さの上限（兜が収まった時・刀を抜く時）。1 にすると体が光に埋もれる。
constexpr float kSeatAura = 0.4f;
// 目の光へ足す明るさの倍率：演技の間、灯る拍の始め（そこから演技の間の値へ落ち着く）。
constexpr float kShowEyeGlow = 1.6f;
constexpr float kEyesIgnite = 4.0f;
// 衝撃の輪（全画面の効果）が広がり切った時の半径（画面の高さを 1。画面の対角を越える）。
constexpr float kShockPostRadius = 1.9f;
// 剣先の星：抜けてから出るまでの秒（振り上げの頂点）、出ている長さ (秒。実時間)、大きさ（画面の高さを 1）。抜けた後に
// 刃口に残す光の割合。
constexpr float kDrawStarAt = 0.2f;
constexpr float kDrawStarSeconds = 0.45f;
constexpr float kDrawStarSize = 0.5f;
constexpr float kBladeEdgeRest = 0.3f;
constexpr float kDrawAura = 0.45f;

// 事件段の名前から、効果の拍の種類へ（演出の資産の側の約束）。
BossIntroCueKind KindOf(const char (&name)[HIKARI::kGameProjectSequenceEventNameCapacity]) {
    // 終端が無くても、入れ物の外は読まない。
    const std::string_view text{name, static_cast<size_t>(std::find(name, std::end(name), '\0') - name)};
    if (text == "Helm.Seat")
        return BossIntroCueKind::HelmSeat;
    if (text == "Sword.Out")
        return BossIntroCueKind::SwordOut;
    if (text == "Eyes")
        return BossIntroCueKind::Eyes;
    if (text == "Smoke.Streak")
        return BossIntroCueKind::SmokeStreak;
    if (text == "Smoke.Rise")
        return BossIntroCueKind::SmokeRise;
    if (text == "Smoke.Tendril")
        return BossIntroCueKind::SmokeTendril;
    if (text == "Smoke.TendrilLeft")
        return BossIntroCueKind::SmokeTendrilLeft;
    if (text == "Hand.Right")
        return BossIntroCueKind::HandRight;
    if (text == "Hand.Left")
        return BossIntroCueKind::HandLeft;
    if (text == "Smoke.HelmLift")
        return BossIntroCueKind::HelmLift;
    if (text == "Sword.Grab")
        return BossIntroCueKind::SwordGrab;
    if (text == "Smoke.Clear")
        return BossIntroCueKind::SmokeClear;
    if (text == "Step")
        return BossIntroCueKind::Step;
    if (text == "Smoke.Disperse")
        return BossIntroCueKind::SmokeDisperse;
    if (text == "Smoke.Ambient")
        return BossIntroCueKind::SmokeAmbient;
    if (text == "Omen")
        return BossIntroCueKind::Omen;
    if (text == "Helm.Glint")
        return BossIntroCueKind::HelmGlint;
    return BossIntroCueKind::Other;
}

// 続けて出す画面の揺れを頼む（このフレームだけ。拍が毎フレーム呼ぶ）。Shake は 1 回ごとに揺れの位相が 0 へ戻るので、
// 毎フレーム頼むと揺れずに片側へ寄るだけになる。こちらは位相を自分で進める（BossIntroEffects::Feedback）。
void KeepRumble(BossComponent& boss, float strength) {
    boss.introState.rumblePending = std::max(boss.introState.rumblePending, strength);
}

// 効果の拍を、演技の時計 t (秒) まで進める。時計が段より前へ戻った時（プレビューで巻き戻した時）は、黙って入る前に戻す。
void StepCues(BossComponent& boss, float t) {
    BossIntroState& state = boss.introState;
    for (int k = 0; k < state.cueCount; ++k) {
        BossIntroCue& cue = state.cues[static_cast<size_t>(k)];
        if (cue.kind == BossIntroCueKind::Unread)
            cue.kind = KindOf(cue.clip.name);
        const float start = cue.clip.startSeconds;
        const float duration = std::max(0.0f, cue.clip.durationSeconds);
        if (t < start) {
            if (cue.entered && cue.kind == BossIntroCueKind::Eyes)
                state.eyesLit = false;
            cue.entered = cue.left = false;
            continue;
        }
        if (!cue.entered) {
            cue.entered = true;
            BossIntroEffects::OnCueStart(boss, cue);
        }
        if (cue.left)
            continue;
        if (t >= start + duration) {
            cue.left = true;
            BossIntroEffects::OnCueEnd(boss, cue);
        } else {
            BossIntroEffects::OnCue(boss, cue, t - start, duration);
        }
    }
}

// 白黒の閃きの、いまの強さと反転（出していなければ 0）。反転した白黒 → 白黒 → 色へ戻る（実時間で進む）。
void StepMono(BossComponent& boss, float unscaledSeconds, float& strength, float& invert) {
    const BossIntroSettings& s = boss.intro;
    BossIntroState& state = boss.introState;
    strength = invert = 0.0f;
    if (state.monoPending) {
        state.monoPending = false;
        state.monoSeconds = 0.0f;
        state.monoActiveStrength = state.monoStrength > 0.0f ? state.monoStrength : s.slashMonoStrength;
        state.monoActiveLength = state.monoLength;
        state.monoStrength = state.monoLength = 0.0f;
    }
    if (state.monoSeconds < 0.0f)
        return;
    // 長さを頼まれた時は、反転・白黒・戻りを項目の割合のまま伸び縮みさせる。
    const float full = std::max(0.0f, s.slashMonoInvertSeconds) + std::max(0.0f, s.slashMonoHoldSeconds) +
                       std::max(0.0f, s.slashMonoFadeSeconds);
    const float stretch = state.monoActiveLength > 0.0f && full > kTiny ? state.monoActiveLength / full : 1.0f;
    const float invertEnd = std::max(0.0f, s.slashMonoInvertSeconds) * stretch;
    const float hold = invertEnd + std::max(0.0f, s.slashMonoHoldSeconds) * stretch;
    const float end = hold + std::max(0.0f, s.slashMonoFadeSeconds) * stretch;
    const float t = state.monoSeconds;
    if (t >= end || state.monoActiveStrength <= 0.0f) {
        state.monoSeconds = -1.0f;
        return;
    }
    const float fade = t < hold ? 1.0f : 1.0f - Smooth((t - hold) / std::max(kTiny, end - hold));
    strength = state.monoActiveStrength * fade;
    invert = t < invertEnd ? 1.0f : 0.0f;
    state.monoSeconds += std::max(0.0f, unscaledSeconds);
}

// 世界の点の、場面の描画範囲の中の UV。写せなければ画面の真ん中の少し下。
void ScreenUv(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 point, float& u, float& v) {
    u = 0.5f;
    v = 0.6f;
    HIKARI::GameProjectCameraViewSnapshot snapshot{};
    HIKARI::GameProjectWorldToScreenResult result{};
    if (!world.GetCamera().TryGetViewSnapshot(snapshot) ||
        !HIKARI::ProjectWorldToScreen(snapshot, point, HIKARI::GameProjectScreenCoordinateSpace::PhysicalOutputPixels,
                                      result))
        return;
    const auto& rect = snapshot.sceneContentPhysical;
    if (rect.right - rect.left <= 0.0f || rect.bottom - rect.top <= 0.0f)
        return;
    u = (result.x - rect.left) / (rect.right - rect.left);
    v = (result.y - rect.top) / (rect.bottom - rect.top);
}

// 全画面の効果（BossImpactMono：白黒の閃き・衝撃の輪・張りつめ）。出す間は毎フレーム送る。Action Post は優先度の高い
// 1 つしか掛からないので、3 つを同じ効果の項目として送る。
void SubmitImpactPost(HIKARI::WorldView& world, BossComponent& boss, uint64_t owner, float unscaledSeconds) {
    const BossIntroSettings& s = boss.intro;
    BossIntroState& state = boss.introState;
    float mono = 0.0f;
    float invert = 0.0f;
    StepMono(boss, unscaledSeconds, mono, invert);
    // 衝撃の輪：実時間で広がる（時間が止まっている間も画の上を走る）。
    float shock = 0.0f;
    float radius = 0.0f;
    if (state.shockSeconds >= 0.0f) {
        const float u = state.shockSeconds / std::max(kTiny, s.drawShockPostSeconds);
        if (u >= 1.0f || s.drawShockPost <= 0.0f) {
            state.shockSeconds = -1.0f;
        } else {
            shock = s.drawShockPost * (1.0f - u) * (1.0f - u);
            radius = kShockPostRadius * EaseOut(u);
            state.shockSeconds += std::max(0.0f, unscaledSeconds);
        }
    }
    // 張りつめ：握っている間・前触れの間（拍が毎フレーム入れる）と、抜刀の後の見得の間。画を暗くする頼み（前触れ）も
    // 同じ。どちらも急に切り替えず、実時間で寄せる（拍の終わりや演技の終わりで、画の明るさが跳ねない）。
    float wanted = state.tensionPending;
    state.tensionPending = 0.0f;
    if (state.phase == BossIntroPhase::Show && !state.skipped)
        for (int k = 0; k < state.cueCount; ++k)
            if (state.cues[static_cast<size_t>(k)].kind == BossIntroCueKind::SwordOut &&
                state.cues[static_cast<size_t>(k)].left)
                wanted = std::max(wanted, s.finaleTension);
    const float ease = std::clamp(std::max(0.0f, unscaledSeconds) * kPostEaseRate, 0.0f, 1.0f);
    state.tensionNow += (wanted - state.tensionNow) * ease;
    state.dimNow += (state.dimPending - state.dimNow) * ease;
    state.dimPending = 0.0f;
    const float tension = state.tensionNow > kPostFloor ? state.tensionNow : 0.0f;
    const float dim = state.dimNow > kPostFloor ? state.dimNow : 0.0f;
    // 剣先の星（実時間で消える）。
    float star = 0.0f;
    if (state.starSeconds >= 0.0f) {
        const float u = state.starSeconds / kDrawStarSeconds;
        if (u >= 1.0f || s.bladeStar <= 0.0f) {
            state.starSeconds = -1.0f;
        } else {
            star = s.bladeStar * (1.0f - u) * (1.0f - u);
            state.starSeconds += std::max(0.0f, unscaledSeconds);
        }
    }
    if ((mono <= 0.0f && shock <= 0.0f && tension <= 0.0f && dim <= 0.0f && star <= 0.0f) || s.slashMonoProfile.empty())
        return;
    HIKARI::GameProjectActionPostFrameRequest request{};
    request.sourceId = kMonoSource ^ owner;
    request.priority = kMonoPriority;
    request.values[0] = {mono, invert, 0.0f, 0.0f};
    request.channelMasks[0] = 3u;
    float centerU = 0.5f;
    float centerV = 0.6f;
    if (shock > 0.0f)
        ScreenUv(world, state.shockAt, centerU, centerV);
    request.values[2] = {shock, radius, centerU, centerV};
    request.channelMasks[2] = 15u;
    request.values[3] = {tension, dim, 0.0f, 0.0f};
    request.channelMasks[3] = 3u;
    if (star > 0.0f) {
        // 星：剣先を画面へ写して、そこに出す。
        BossBodyPose pose{};
        float starU = 0.5f;
        float starV = 0.3f;
        if (BossIntroActor::Pose(boss, pose))
            ScreenUv(
                world,
                Add(pose.sword.position, Mul(Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f}), VisibleBladeReach(boss))),
                starU, starV);
        request.values[5] = {star, starU, starV, kDrawStarSize};
        request.channelMasks[5] = 15u;
    }
    world.GetPresentationVfx().SubmitActionPostProfile(s.slashMonoProfile, request);
}

} // namespace

void BossIntroEffects::OnCueStart(BossComponent& boss, const BossIntroCue& cue) {
    const BossIntroSettings& s = boss.intro;
    switch (cue.kind) {
    case BossIntroCueKind::HandRight:
    case BossIntroCueKind::HandLeft: Shake(boss, s.attachShake); break;
    case BossIntroCueKind::HelmSeat: {
        // 兜が頭に収まった瞬間：白黒の閃き・足元の衝撃の輪・揺れ・一瞬の停止・襟から噴く煙（BossIntroSmoke）。
        const HIKARI::GameProjectFloat3 feet = BossIntroSmoke::FeetWorld(boss);
        MonoFlash(boss);
        Shock(boss, 0, feet, s.seatShock);
        Shock(boss, 1, feet, s.seatShock * kSecondShockScale, kSecondShockDelay);
        Shake(boss, s.seatShake);
        HitStop(boss, s.seatHitStopFrames);
        boss.introState.smokeSeatPush = true;
        break;
    }
    case BossIntroCueKind::SwordOut: {
        // 刀が地から抜けた瞬間：白黒の閃き・根元の衝撃の輪・強い揺れ・停止・刀身の光・黒い煙の噴き上げ（BossIntroSmoke）。
        const HIKARI::GameProjectFloat3 entry = BossIntroSmoke::SwordEntryWorld(boss);
        MonoFlash(boss);
        Shock(boss, 0, entry, s.drawShock);
        Shock(boss, 1, entry, s.drawShock * kSecondShockScale, kSecondShockDelay);
        Shake(boss, s.drawShake);
        HitStop(boss, s.drawHitStopFrames);
        // 停止の後、噴き上がりをスローで見せる。根元で閃光を弾けさせる。
        Slow(boss, s.drawHitStopFrames + s.drawSlowFrames);
        if (s.drawFlashScale > 0.0f)
            MistBurst(boss, {entry.x, entry.y + 1.0f, entry.z}, s.drawFlashScale);
        boss.introState.smokeDrawPush = true;
        // 根元から、画の上を衝撃の輪が走る（全画面の効果）。
        boss.introState.shockSeconds = 0.0f;
        boss.introState.shockAt = {entry.x, entry.y + 0.6f, entry.z};
        break;
    }
    // 刀へ歩く一歩：重さを、短い揺れで伝える（煙は BossIntroSmoke）。
    case BossIntroCueKind::Step: Shake(boss, s.stepShake); break;
    // 見得の終わり：黒い霧が外へ流れて晴れ始める（BossIntroSmoke）。揺れ・閃き・衝撃の輪は付けない（静かに晴れて、
    // そのままゲームのカメラへ渡る）。
    case BossIntroCueKind::SmokeDisperse: boss.introState.dispersePush = true; break;
    case BossIntroCueKind::Eyes:
        boss.introState.eyesLit = true;
        EyeFlare(boss);
        break;
    default: break;
    }
}

void BossIntroEffects::OnCue(BossComponent& boss, const BossIntroCue& cue, float localSeconds, float durationSeconds) {
    const float u = std::clamp(localSeconds / std::max(kTiny, durationSeconds), 0.0f, 1.0f);
    switch (cue.kind) {
    // 目が灯る拍：強く灯ってから、演技の間の明るさへ落ち着く。
    case BossIntroCueKind::Eyes:
        boss.introState.eyeGlow =
            std::max(boss.introState.eyeGlow, kShowEyeGlow + kEyesIgnite * (1.0f - u) * (1.0f - u));
        // 閃きは、段の間ゆっくり引いていく（一瞬で消さない）。
        EyeFlare(boss, boss.intro.eyesFlare * (1.0f - u));
        break;
    // 兜が収まった後、体を包む気配が引いていく（体を白く飛ばさない強さに抑える）。
    case BossIntroCueKind::HelmSeat: Aura(boss, kSeatAura * (1.0f - u)); break;
    // 刀を握っている間、気配と刃口の光が高まる。
    case BossIntroCueKind::SwordGrab:
        Aura(boss, kDrawAura * (0.3f + 0.7f * u));
        BladeEdge(boss, u);
        // 握る間、揺れが高まって行く（抜ける瞬間の強い揺れへつながる）。
        KeepRumble(boss, boss.intro.grabRumble * u * u);
        // 画の縁が締まり、色がずれて行く（全画面の効果）。
        boss.introState.tensionPending = boss.intro.grabTension * u * u;
        break;
    // 前触れ：画がゆっくり暗くなり、縁が締まり、低い揺れが高まって行く（霧の流れが変わるのは BossIntroSmoke）。
    case BossIntroCueKind::Omen: {
        const float rise = Smooth(u);
        boss.introState.dimPending = std::max(boss.introState.dimPending, boss.intro.omenDim * rise);
        boss.introState.tensionPending = std::max(boss.introState.tensionPending, boss.intro.omenTension * rise);
        KeepRumble(boss, boss.intro.omenRumble * u * u);
        break;
    }
    // 落ちている兜の隙間の瞬き：すっと灯って、尾を引いて消える（ごく短い）。
    case BossIntroCueKind::HelmGlint: {
        const float fall = 1.0f - (u - kGlintRise) / (1.0f - kGlintRise);
        boss.introState.helmGlint = std::max(boss.introState.helmGlint, u < kGlintRise ? u / kGlintRise : fall * fall);
        break;
    }
    // 抜けた後、刀光を引いて上がり、気配が引いていく。
    case BossIntroCueKind::SwordOut:
        Aura(boss, kDrawAura * (1.0f - u));
        // 刃口の細い線は、抜けた後も弱く残る。振り上げの頂点で、剣先に星を 1 つ。
        BladeEdge(boss, std::max(kBladeEdgeRest, 1.0f - u));
        if (boss.introState.starSeconds < 0.0f && localSeconds >= kDrawStarAt && localSeconds < kDrawStarAt + 0.12f)
            boss.introState.starSeconds = 0.0f;
        break;
    default: break;
    }
}

void BossIntroEffects::OnCueEnd(BossComponent& /*boss*/, const BossIntroCue& /*cue*/) {
    // 段の終わりで止める効果は、ここに足す（いまは無い）。
}

void BossIntroEffects::OnSkip(BossComponent& boss) {
    // 残りの拍は鳴らさない（飛ばした後の構えに、演技の途中の効果を出さない）。
    BossIntroState& state = boss.introState;
    for (int k = 0; k < state.cueCount; ++k)
        state.cues[static_cast<size_t>(k)].entered = state.cues[static_cast<size_t>(k)].left = true;
}

void BossIntroEffects::Update(BossComponent& boss, float showSeconds, float seconds) {
    BossIntroState& state = boss.introState;
    // 毎フレームの頼みは、ここで 0 に戻す（このあと変わり目の演出が、同じフレームの分を足す）。
    state.aura = 0.0f;
    state.bladeEdge = 0.0f;
    state.bladeSwing = false;
    state.helmGlint = 0.0f;
    // 目が灯った後、演技の間は普段より明るく灯す（昼の空を背にしても、兜の隙間の光が読める）。
    state.eyeGlow =
        state.eyesLit && (state.phase == BossIntroPhase::Show || BossIntro::ActorLingers(boss)) ? kShowEyeGlow : 0.0f;
    // 遅れて出る衝撃の輪の時計（登場の後は、変わり目の演出が進める）。
    if (state.phase != BossIntroPhase::Done)
        for (BossIntroState::Shock& shock : state.shocks)
            shock.delay -= std::max(0.0f, seconds);
    if (state.skipped && !state.skipSeen) {
        state.skipSeen = true;
        OnSkip(boss);
    }
    if (state.phase == BossIntroPhase::Show || state.phase == BossIntroPhase::Settle || state.previewing)
        StepCues(boss, showSeconds);
}

void BossIntroEffects::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds) {
    // 登場の黒い煙（流体への注入・押し・吸収）。胴の煙は BossBodyVisuals が出す。
    BossIntroSmoke::Render(world, object, boss, seconds);
    // 落ちている兜の隙間の瞬き（序幕）。色は、目の光と同じ色合い。
    const Vec3 tint = boss.body.accents.eyesMistColor;
    BossIntroActor::SetHelmPartGlint(world, boss, Mul(tint, 1.0f / std::max({tint.x, tint.y, tint.z, kTiny})),
                                     boss.intro.helmGlint * boss.introState.helmGlint);
}

namespace {

// 演出の動きのぼかし（c-bc5908）：演技の間は設定のまま、一息の段ではゲームのカメラへ渡すのに合わせて弱める
// （BossIntroCamera の渡しと同じ進み）。飛ばした時は掛けない。頼みは 1 フレームだけ効くので、描画のフレームごとに出す。
void SubmitMotionBlur(HIKARI::WorldView& world, BossComponent& boss, uint64_t owner) {
    const BossIntroSettings& s = boss.intro;
    BossIntroState& state = boss.introState;
    float weight = 0.0f;
    if (!state.skipped && s.motionBlurShutterDegrees > 0.0f && s.motionBlurMaxPixels > 0.0f) {
        if (state.phase == BossIntroPhase::Show) {
            weight = 1.0f;
        } else if (state.phase == BossIntroPhase::Settle) {
            const float u =
                std::clamp(static_cast<float>(state.frame) / std::max(1.0f, s.settleSeconds * 60.0f), 0.0f, 1.0f);
            weight = 1.0f - u * u * (3.0f - 2.0f * u);
        }
    }
    bool accepted = false;
    if (weight > 0.0f) {
        HIKARI::GameProjectCameraMotionBlurRequest blur{};
        blur.cameraObjectHandle = 0; // いま画を出しているカメラ（演技の間は演出の資産のカメラ）
        blur.sourceId = kMotionBlurSource ^ owner;
        blur.priority = kAccentPriority;
        // 重みはカメラの元の設定（無効なら 0°）からのシャッター角と長さの補間。
        blur.weight = weight;
        blur.settings.enabled = 1;
        blur.settings.shutterAngleDegrees = std::clamp(s.motionBlurShutterDegrees, 0.0f, 360.0f);
        blur.settings.maxBlur1080pPixels = std::clamp(s.motionBlurMaxPixels, 0.0f, 64.0f);
        accepted = world.GetCamera().SubmitMotionBlur(blur);
    }
    if (accepted != state.motionBlurAccepted) {
        state.motionBlurAccepted = accepted;
        char message[128]{};
        std::snprintf(message, sizeof(message), "IntroMotionBlur accepted=%d weight=%.3f shutter=%.1f pixels=%.1f",
                      static_cast<int>(accepted), static_cast<double>(weight),
                      static_cast<double>(s.motionBlurShutterDegrees), static_cast<double>(s.motionBlurMaxPixels));
        BossLog::Write(message);
    }
}

} // namespace

void BossIntroEffects::Feedback(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                                float unscaledSeconds) {
    const BossIntroSettings& s = boss.intro;
    BossIntroState& state = boss.introState;
    const uint64_t owner = object.GetHandle().value;
    if (state.accentPending > 0.0f) {
        Project_TD2_2::Feedback::CameraShakeParams shake{};
        shake.sourceId = kAccentCameraSource ^ owner;
        shake.priority = kAccentPriority;
        shake.frames = s.accentShakeFrames;
        shake.amplitude = s.accentShake * state.accentPending;
        shake.frequency = s.accentShakeFrequency;
        // 強い拍だけ、画角を一瞬広げる。
        shake.fovDegrees = state.accentPending >= 1.0f ? s.accentFovDegrees : 0.0f;
        Project_TD2_2::Feedback::StartCameraShake(state.shake, shake);
        if (state.accentPending >= 1.0f && s.accentRumble > 0.0f)
            Project_TD2_2::Feedback::Rumble(world, s.accentRumble, s.accentRumble * 0.6f, s.accentRumbleFrames);
        state.accentPending = 0.0f;
    }
    if (state.hitStopPending > 0) {
        Project_TD2_2::Feedback::RequestHitStop(world, kAccentHitStopSource ^ owner, kAccentPriority,
                                                state.hitStopPending, s.hitStopScale);
        state.hitStopPending = 0;
    }
    if (state.slowPending > 0) {
        Project_TD2_2::Feedback::RequestHitStop(world, kAccentSlowSource ^ owner, kAccentPriority, state.slowPending,
                                                s.drawSlowScale);
        state.slowPending = 0;
    }
    if (state.flashPending) {
        state.flashPending = false;
        const float scale = state.flashScale > 0.0f ? state.flashScale : s.slashFlashScale;
        if (scale > 0.0f && !s.slashFlashEffect.IsEmpty()) {
            HIKARI::GameProjectVfxPlayRequest play{};
            play.asset = s.slashFlashEffect;
            play.transform = BossVfx::Placed(state.flashAt, scale);
            play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
            BossVfx::Start(world, boss, play);
        }
    }
    SubmitImpactPost(world, boss, owner, unscaledSeconds);
    // 続けて出す揺れ（前触れ・刀を握る間）：位相を実時間で進め、速さの違う波を重ねる（円を描くだけの機械的な揺れに
    // しない）。
    if (state.rumblePending > 0.0f) {
        state.rumblePhase += kRumbleTurn * s.rumbleFrequency * std::max(0.0f, unscaledSeconds);
        const float amplitude = s.accentShake * state.rumblePending;
        const float p = state.rumblePhase;
        (void)Project_TD2_2::Feedback::SubmitCameraOffset(
            world, kRumbleCameraSource ^ owner, kAccentPriority - 1,
            {amplitude * (0.7f * std::sin(p) + 0.3f * std::sin(p * 2.3f + 1.1f)),
             amplitude * (0.7f * std::sin(p * 1.31f + 0.7f) + 0.3f * std::sin(p * 2.9f)), 0.0f});
        state.rumblePending = 0.0f;
    }
    // 演出の資産が流れている間、カメラの姿勢を控えておく（演技の後、BossIntroCamera がここからゲームのカメラへ渡す）。
    // 控えるのは、資産が進んでいる間だけ：流れ終わった後の画は、もう資産のカメラとは限らない（そこを控えると、渡す元が
    // ゲームのカメラ自身になって、渡しが効かずに切り替わって見える）。姿勢は、エンジンが直近に解決した描画のカメラから
    // 取る（画面に出した画の写し TryGetViewSnapshot は、画を出さない実行では取れない）。
    HIKARI::GameProjectCameraPose film{};
    HIKARI::GameProjectSequencePlaybackSnapshot playback{};
    if (state.phase == BossIntroPhase::Show && !state.skipped &&
        world.GetSequence().GetSnapshot(state.sequence, playback) &&
        playback.state == HIKARI::GameProjectSequencePlaybackState::Playing &&
        world.GetCamera().GetRenderedPose(film) && film.verticalFovRadians > 0.0f) {
        state.filmValid = true;
        state.filmEye = film.eye;
        state.filmTarget = film.target;
        state.filmFov = film.verticalFovRadians;
    }
    SubmitMotionBlur(world, boss, owner);
    Project_TD2_2::Feedback::SubmitCameraShake(world, state.shake, 0, unscaledSeconds);
}

void BossIntroEffects::MonoFlash(BossComponent& boss, float strength, float seconds) {
    BossIntroState& state = boss.introState;
    state.monoPending = true;
    state.monoStrength = std::clamp(strength, 0.0f, 1.0f);
    state.monoLength = std::max(0.0f, seconds);
}

void BossIntroEffects::Shake(BossComponent& boss, float strength) {
    boss.introState.accentPending = std::max(boss.introState.accentPending, strength);
}

void BossIntroEffects::HitStop(BossComponent& boss, int frames) {
    boss.introState.hitStopPending = std::max(boss.introState.hitStopPending, frames);
}

void BossIntroEffects::Slow(BossComponent& boss, int frames) {
    boss.introState.slowPending = std::max(boss.introState.slowPending, frames);
}

void BossIntroEffects::MistBurst(BossComponent& boss, HIKARI::GameProjectFloat3 at, float scale) {
    BossIntroState& state = boss.introState;
    state.flashPending = true;
    state.flashAt = at;
    state.flashScale = scale;
}

void BossIntroEffects::Aura(BossComponent& boss, float strength) {
    boss.introState.aura = std::max(boss.introState.aura, strength);
}

void BossIntroEffects::Shock(BossComponent& boss, int slot, HIKARI::GameProjectFloat3 at, float size,
                             float delaySeconds) {
    BossIntroState& state = boss.introState;
    state.shocks[static_cast<size_t>(std::clamp(slot, 0, static_cast<int>(state.shocks.size()) - 1))] = {at, size,
                                                                                                         delaySeconds};
}

void BossIntroEffects::Glint(BossComponent& boss, float brightness) { boss.introState.glint = brightness; }

void BossIntroEffects::BladeEdge(BossComponent& boss, float strength) {
    boss.introState.bladeEdge = std::max(boss.introState.bladeEdge, strength);
}

void BossIntroEffects::BladeSwing(BossComponent& boss) { boss.introState.bladeSwing = true; }

void BossIntroEffects::EyeFlare(BossComponent& boss, float strength) {
    float& flare = boss.bodyState.accents.flare;
    flare = std::max(flare, strength > 0.0f ? strength : boss.intro.eyesFlare);
}

} // namespace Project_TD2_2
