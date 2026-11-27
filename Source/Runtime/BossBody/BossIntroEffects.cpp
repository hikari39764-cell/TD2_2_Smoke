#include "BossIntroEffects.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string_view>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/CameraViewSnapshot.h"
#include "HIKARI/Project/SequenceView.h"
#include "HIKARI/Project/UiView.h"
#include "BossBodyMath.h"
#include "BossCinematicFx.h"
#include "BossIntroActor.h"
#include "BossIntroSmoke.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossIntro.h"
#include "../Boss/BossVfx.h"
#include "../Common/VfxParameters.h"
#include "../Player/Feedback.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;

// 演出の動きのぼかしの出し元の番号（c-bc5908）と優先度（演出の効果の手応えと同じ。崩しの手応えより下）。
constexpr uint64_t kMotionBlurSource = 0x5444324A16ull;
constexpr int kMotionBlurPriority = Project_TD2_2::Feedback::kBreakPriority - 10;
// 演出の被写界深度の出し元の番号（c-280b71。優先度は動きのぼかしと同じ）。
constexpr uint64_t kFocusSource = 0x5444324A17ull;
// 上下の黒帯（UI の矩形 2 枚）の出し元の番号と重ね順（HUD の条より上、勝ち負けの画面の色より下）。
constexpr uint64_t kBarTopId = 0x5444324A18ull;
constexpr uint64_t kBarBottomId = 0x5444324A19ull;
constexpr int kBarOrder = 90;
constexpr const char* kSolidTexture = "generated://solid/ffffffff";
// 「続く」ピントの段を、前の段の終わりと見なす始めのずれ (秒)。
constexpr float kFocusLinkSlack = 0.05f;
// 漂う塵の入力（資産側の約束）。
constexpr const char* kMotesAmountInput = "input.amount";
// 落ちている兜の隙間の瞬き：拍のこの割合までで灯り切り、残りで消える。
constexpr float kGlintRise = 0.22f;
// 衝撃の輪の 2 つ目：1 つ目に対する大きさの倍率と、遅れて出す秒。刀が抜けた後、刀光を引く長さ (秒)。
constexpr float kSecondShockScale = 1.4f;
constexpr float kSecondShockDelay = 0.12f;
// 体を包む気配の強さの上限（兜が収まった時・刀を抜く時）。1 にすると体が光に埋もれる。
constexpr float kSeatAura = 0.4f;
// 目の光へ足す明るさの倍率：演技の間、灯る拍の始め（そこから演技の間の値へ落ち着く）。
constexpr float kShowEyeGlow = 1.6f;
constexpr float kEyesIgnite = 4.0f;
// 衝撃の輪（全画面の効果）が広がり切った時の半径（画面の高さを 1。画面の対角を越える）。
constexpr float kShockPostRadius = 1.9f;
// 剣先の星：抜けてから出るまでの秒（振り上げの頂点）、出ている長さ (秒。実時間)。抜けた後に刃口に残す光の割合。
constexpr float kDrawStarAt = 0.2f;
constexpr float kDrawStarSeconds = 0.45f;
constexpr float kBladeEdgeRest = 0.3f;
constexpr float kDrawAura = 0.45f;

// 「:」で区切った数を 2 つまで読む（「6.40:1.5」。読めない所は、渡された値のまま）。
void ReadValues(std::string_view text, float& a, float& b) {
    const size_t split = text.find(':');
    const std::string_view first = text.substr(0, split);
    (void)std::from_chars(first.data(), first.data() + first.size(), a);
    if (split == std::string_view::npos)
        return;
    const std::string_view second = text.substr(split + 1);
    (void)std::from_chars(second.data(), second.data() + second.size(), b);
}

// 事件段の名前から、効果の拍の種類へ（演出の資産の側の約束）。数の付く名前（Focus・Blur）は、数も読む。
BossIntroCueKind KindOf(BossIntroCue& cue) {
    const char(&name)[HIKARI::kGameProjectSequenceEventNameCapacity] = cue.clip.name;
    // 終端が無くても、入れ物の外は読まない。
    const std::string_view text{name, static_cast<size_t>(std::find(name, std::end(name), '\0') - name)};
    if (text.rfind("Focus:", 0) == 0 || text.rfind("Focus>", 0) == 0) {
        cue.valueA = 0.0f;
        cue.valueB = 1.0f;
        ReadValues(text.substr(6), cue.valueA, cue.valueB);
        return text[5] == ':' ? BossIntroCueKind::Focus : BossIntroCueKind::FocusLink;
    }
    if (text.rfind("Blur:", 0) == 0) {
        cue.valueA = 180.0f;
        cue.valueB = 24.0f;
        ReadValues(text.substr(5), cue.valueA, cue.valueB);
        return BossIntroCueKind::Blur;
    }
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
    if (text == "Hand.Fist")
        return BossIntroCueKind::HandFist;
    if (text == "Smoke.HelmLift")
        return BossIntroCueKind::HelmLift;
    if (text == "Sword.Grab")
        return BossIntroCueKind::SwordGrab;
    if (text == "Sword.Tip")
        return BossIntroCueKind::SwordTip;
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

// 効果の拍を、演技の時計 t (秒) まで進める。時計が段より前へ戻った時（プレビューで巻き戻した時）は、黙って入る前に戻す。
void StepCues(BossComponent& boss, BossIntroComponent& intro, float t) {
    BossIntroState& state = intro.state;
    for (int k = 0; k < state.cueCount; ++k) {
        BossIntroCue& cue = state.cues[static_cast<size_t>(k)];
        if (cue.kind == BossIntroCueKind::Unread)
            cue.kind = KindOf(cue);
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
            BossIntroEffects::OnCueStart(boss, intro, cue);
        }
        if (cue.left)
            continue;
        if (t >= start + duration) {
            cue.left = true;
            BossIntroEffects::OnCueEnd(boss, intro, cue);
        } else {
            BossIntroEffects::OnCue(boss, intro, cue, t - start, duration);
        }
    }
}

} // namespace

void BossIntroEffects::OnCueStart(BossComponent& boss, BossIntroComponent& intro, const BossIntroCue& cue) {
    const BossIntroSettings& s = intro.settings;
    switch (cue.kind) {
    case BossIntroCueKind::HandRight:
    case BossIntroCueKind::HandLeft: BossCinematicFx::Shake(boss, s.attachShake); break;
    case BossIntroCueKind::HelmSeat: {
        // 兜が頭に収まった瞬間：白黒の閃き・足元の衝撃の輪・揺れ・一瞬の停止・襟から噴く煙（BossIntroSmoke）。
        const HIKARI::GameProjectFloat3 feet = BossIntroSmoke::FeetWorld(intro);
        BossCinematicFx::MonoFlash(boss);
        BossCinematicFx::Shock(boss, 0, feet, s.seatShock);
        BossCinematicFx::Shock(boss, 1, feet, s.seatShock * kSecondShockScale, kSecondShockDelay);
        BossCinematicFx::Shake(boss, s.seatShake);
        BossCinematicFx::HitStop(boss, s.seatHitStopFrames);
        intro.state.smokeSeatPush = true;
        break;
    }
    // 煙が鎧に届いて引き起こす瞬間：足元から砂ぼこりが立つ。
    case BossIntroCueKind::SmokeRise: intro.state.dustPending = true; break;
    case BossIntroCueKind::SwordOut: {
        // 刀が地から抜けた瞬間：白黒の閃き・根元の衝撃の輪・強い揺れ・停止・刀身の光・黒い煙の噴き上げ（BossIntroSmoke）。
        const HIKARI::GameProjectFloat3 entry = BossIntroSmoke::SwordEntryWorld(intro);
        BossCinematicFx::MonoFlash(boss);
        BossCinematicFx::Shock(boss, 0, entry, s.drawShock);
        BossCinematicFx::Shock(boss, 1, entry, s.drawShock * kSecondShockScale, kSecondShockDelay);
        BossCinematicFx::Shake(boss, s.drawShake);
        BossCinematicFx::HitStop(boss, s.drawHitStopFrames);
        // 停止の後、噴き上がりをスローで見せる。根元で閃光を弾けさせる。
        BossCinematicFx::Slow(boss, s.drawHitStopFrames + s.drawSlowFrames);
        if (s.drawFlashScale > 0.0f)
            BossCinematicFx::MistBurst(boss, {entry.x, entry.y + 1.0f, entry.z}, s.drawFlashScale);
        intro.state.smokeDrawPush = true;
        // 土くれと砂の輪（一度きりの特効）。
        intro.state.debrisPending = true;
        // 根元から、画の上を衝撃の輪が走る（全画面の効果）。
        intro.state.shockSeconds = 0.0f;
        intro.state.shockAt = {entry.x, entry.y + 0.6f, entry.z};
        break;
    }
    // 刀へ歩く一歩：重さを、短い揺れで伝える（煙は BossIntroSmoke）。
    case BossIntroCueKind::Step: BossCinematicFx::Shake(boss, s.stepShake); break;
    // 見得の終わり：黒い霧が外へ流れて晴れ始める（BossIntroSmoke）。揺れ・閃き・衝撃の輪は付けない（静かに晴れて、
    // そのままゲームのカメラへ渡る）。
    case BossIntroCueKind::SmokeDisperse: intro.state.dispersePush = true; break;
    case BossIntroCueKind::Eyes:
        intro.state.eyesLit = true;
        BossCinematicFx::EyeFlare(boss);
        intro.state.hazePending = true;
        break;
    default: break;
    }
}

void BossIntroEffects::OnCue(BossComponent& boss, BossIntroComponent& intro, const BossIntroCue& cue,
                             float localSeconds, float durationSeconds) {
    const BossIntroSettings& s = intro.settings;
    BossCinematicFxState& fx = boss.cinematicFxState;
    const float u = std::clamp(localSeconds / std::max(kTiny, durationSeconds), 0.0f, 1.0f);
    switch (cue.kind) {
    // 目が灯る拍：強く灯ってから、演技の間の明るさへ落ち着く。
    case BossIntroCueKind::Eyes:
        fx.eyeGlow = std::max(fx.eyeGlow, kShowEyeGlow + kEyesIgnite * (1.0f - u) * (1.0f - u));
        // 閃きは、段の間ゆっくり引いていく（一瞬で消さない）。
        BossCinematicFx::EyeFlare(boss, boss.cinematicFx.eyesFlare * (1.0f - u));
        break;
    // 兜が収まった後、体を包む気配が引いていく（体を白く飛ばさない強さに抑える）。
    case BossIntroCueKind::HelmSeat: BossCinematicFx::Aura(boss, kSeatAura * (1.0f - u)); break;
    // 刀を握っている間、気配と刃口の光が高まる。
    case BossIntroCueKind::SwordGrab:
        BossCinematicFx::Aura(boss, kDrawAura * (0.3f + 0.7f * u));
        BossCinematicFx::BladeEdge(boss, u);
        // 握る間、揺れが高まって行く（抜ける瞬間の強い揺れへつながる）。
        BossCinematicFx::KeepRumble(boss, s.grabRumble * u * u);
        // 画の縁が締まり、色がずれて行く（全画面の効果）。
        fx.tensionPending = s.grabTension * u * u;
        break;
    // 前触れ：画がゆっくり暗くなり、縁が締まり、低い揺れが高まって行く（霧の流れが変わるのは BossIntroSmoke）。
    case BossIntroCueKind::Omen: {
        const float rise = Smooth(u);
        fx.dimPending = std::max(fx.dimPending, s.omenDim * rise);
        fx.tensionPending = std::max(fx.tensionPending, s.omenTension * rise);
        BossCinematicFx::KeepRumble(boss, s.omenRumble * u * u);
        break;
    }
    // 落ちている兜の隙間の瞬き：すっと灯って、尾を引いて消える（ごく短い）。
    case BossIntroCueKind::HelmGlint: {
        const float fall = 1.0f - (u - kGlintRise) / (1.0f - kGlintRise);
        intro.state.helmGlint = std::max(intro.state.helmGlint, u < kGlintRise ? u / kGlintRise : fall * fall);
        break;
    }
    // 抜けた後、刀光を引いて上がり、気配が引いていく。
    // ピント：この段の値から、すぐ後に続く段（Focus>）の値へ、段の間に線形に寄る（続く段が無ければ、この段の値のまま）。
    case BossIntroCueKind::Focus:
    case BossIntroCueKind::FocusLink: {
        float distance = cue.valueA;
        float strength = cue.valueB;
        const float end = cue.clip.startSeconds + std::max(0.0f, cue.clip.durationSeconds);
        for (int k = 0; k < intro.state.cueCount; ++k) {
            const BossIntroCue& next = intro.state.cues[static_cast<size_t>(k)];
            if (next.kind == BossIntroCueKind::FocusLink && std::abs(next.clip.startSeconds - end) <= kFocusLinkSlack) {
                distance = Lerp(cue.valueA, next.valueA, u);
                strength = Lerp(cue.valueB, next.valueB, u);
                break;
            }
        }
        intro.state.focusSeen = true;
        intro.state.focusWantDistance = distance;
        intro.state.focusWantStrength = strength;
        break;
    }
    // 動きのぼかしの強い所。
    case BossIntroCueKind::Blur:
        intro.state.blurWantShutter = cue.valueA;
        intro.state.blurWantPixels = cue.valueB;
        break;
    case BossIntroCueKind::SwordOut:
        BossCinematicFx::Aura(boss, kDrawAura * (1.0f - u));
        // 刃口の細い線は、抜けた後も弱く残る。振り上げの頂点で、剣先に星を 1 つ。
        BossCinematicFx::BladeEdge(boss, std::max(kBladeEdgeRest, 1.0f - u));
        if (intro.state.starSeconds < 0.0f && localSeconds >= kDrawStarAt && localSeconds < kDrawStarAt + 0.12f)
            intro.state.starSeconds = 0.0f;
        break;
    default: break;
    }
}

void BossIntroEffects::OnCueEnd(BossComponent& /*boss*/, BossIntroComponent& /*intro*/, const BossIntroCue& /*cue*/) {
    // 段の終わりで止める効果は、ここに足す（いまは無い）。
}

void BossIntroEffects::OnSkip(BossIntroComponent& intro) {
    // 残りの拍は鳴らさない（飛ばした後の構えに、演技の途中の効果を出さない）。
    BossIntroState& state = intro.state;
    for (int k = 0; k < state.cueCount; ++k)
        state.cues[static_cast<size_t>(k)].entered = state.cues[static_cast<size_t>(k)].left = true;
}

void BossIntroEffects::Update(BossComponent& boss, BossIntroComponent& intro, float showSeconds, float seconds) {
    BossIntroState& state = intro.state;
    BossCinematicFxState& fx = boss.cinematicFxState;
    // 毎フレームの頼み（兜の瞬き）は、ここで 0 に戻す（気配・刃口・刀光・目は BossCinematicFx::BeginFrame が戻した）。
    state.helmGlint = 0.0f;
    // カメラの効果の頼み（ピント・動きのぼかしの強い所）も、拍が毎フレーム入れ直す。
    state.focusSeen = false;
    state.blurWantShutter = state.blurWantPixels = -1.0f;
    // 目が灯った後、演技の間は普段より明るく灯す（昼の空を背にしても、兜の隙間の光が読める）。
    fx.eyeGlow =
        state.eyesLit && (state.phase == BossIntroPhase::Show || BossIntro::ActorLingers(&intro)) ? kShowEyeGlow : 0.0f;
    // 遅れて出る衝撃の輪の時計（登場の後は、変わり目の演出が進める）。
    if (state.phase != BossIntroPhase::Done)
        for (BossCinematicFxState::Shock& shock : fx.shocks)
            shock.delay -= std::max(0.0f, seconds);
    if (state.skipped && !state.skipSeen) {
        state.skipSeen = true;
        OnSkip(intro);
    }
    if (state.phase == BossIntroPhase::Show || state.phase == BossIntroPhase::Settle || state.previewing)
        StepCues(boss, intro, showSeconds);
}

void BossIntroEffects::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                              const BossBodySettings& body, BossIntroComponent& intro, float seconds) {
    // 登場の黒い煙（流体への注入・押し・吸収）。胴の煙は BossBodyVisuals が出す。
    BossIntroSmoke::Render(world, object, boss, body, intro, seconds);
    // 落ちている兜の隙間の瞬き（序幕）。色は、目の光と同じ色合い。
    const Vec3 tint = body.accents.eyesMistColor;
    BossIntroActor::SetHelmPartGlint(world, intro, Mul(tint, 1.0f / std::max({tint.x, tint.y, tint.z, kTiny})),
                                     intro.settings.helmGlint * intro.state.helmGlint);
    // 一度きりの特効（c-280b71）：拍が立てた頼みを、ここで出す。飛ばした後は出さない。
    const BossIntroSettings& s = intro.settings;
    BossIntroState& state = intro.state;
    const auto burst = [&](bool& pending, const HIKARI::GameProjectVfxAssetReference& asset, Vec3 at, float scale) {
        if (!pending)
            return;
        pending = false;
        if (asset.IsEmpty() || state.skipped)
            return;
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = asset;
        play.transform.position = at;
        play.transform.scale = {scale, scale, scale};
        play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
        BossVfx::Start(world, boss, play);
    };
    burst(state.dustPending, s.dustEffect, BossIntroSmoke::FeetWorld(intro), s.dustScale);
    burst(state.debrisPending, s.debrisEffect, BossIntroSmoke::SwordEntryWorld(intro), s.debrisScale);
    BossBodyPose pose{};
    const bool posed = BossIntroActor::Pose(intro, pose);
    burst(state.hazePending, s.hazeEffect,
          posed ? pose.head.position : Add(BossIntroSmoke::FeetWorld(intro), {0.0f, 5.5f, 0.0f}), s.hazeScale);
    // 演技の間じゅう舞台に漂う塵と灰（演技が済んだ・飛ばしたら止める）。
    const bool showing = state.phase == BossIntroPhase::Show && !state.skipped;
    if (showing && !s.motesEffect.IsEmpty() && s.motesAmount > 0.0f) {
        HIKARI::GameProjectTransform stage{};
        stage.position = BossIntroSmoke::FeetWorld(intro);
        HIKARI::GameProjectVfxParameterOverrides parameters{};
        VfxParameters::SetFloat(parameters, kMotesAmountInput, s.motesAmount);
        BossVfx::StartOrSend(world, boss, state.motesHandle, s.motesEffect, stage, parameters);
    } else {
        BossVfx::StopHandle(world, state.motesHandle);
    }
}

namespace {

// 演出の動きのぼかし（c-bc5908）：演技の間は設定のまま、一息の段ではゲームのカメラへ渡すのに合わせて弱める
// （BossIntroCamera の渡しと同じ進み）。飛ばした時は掛けない。頼みは 1 フレームだけ効くので、描画のフレームごとに出す。
void SubmitMotionBlur(HIKARI::WorldView& world, BossIntroComponent& intro, uint64_t owner, float unscaledSeconds) {
    const BossIntroSettings& s = intro.settings;
    BossIntroState& state = intro.state;
    // 強い所（事件段 Blur）の値へ、実時間で寄せる。拍が無い間は設定の値へ戻る。
    const float wantShutter = state.blurWantShutter >= 0.0f ? state.blurWantShutter : s.motionBlurShutterDegrees;
    const float wantPixels = state.blurWantPixels >= 0.0f ? state.blurWantPixels : s.motionBlurMaxPixels;
    if (state.blurShutter < 0.0f || !(s.motionBlurBlendSeconds > 0.0f)) {
        state.blurShutter = wantShutter;
        state.blurPixels = wantPixels;
    } else {
        const float ease = std::clamp(std::max(0.0f, unscaledSeconds) / s.motionBlurBlendSeconds, 0.0f, 1.0f);
        state.blurShutter += (wantShutter - state.blurShutter) * ease;
        state.blurPixels += (wantPixels - state.blurPixels) * ease;
    }
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
    // いま画を出しているカメラへ（演技の間は演出の資産のカメラ）。
    if (weight > 0.0f)
        accepted = Project_TD2_2::Feedback::SubmitMotionBlur(world, kMotionBlurSource ^ owner, kMotionBlurPriority,
                                                             weight, state.blurShutter, state.blurPixels);
    if (accepted != state.motionBlurAccepted) {
        state.motionBlurAccepted = accepted;
        char message[128]{};
        std::snprintf(message, sizeof(message), "IntroMotionBlur accepted=%d weight=%.3f shutter=%.1f pixels=%.1f",
                      static_cast<int>(accepted), static_cast<double>(weight),
                      static_cast<double>(s.motionBlurShutterDegrees), static_cast<double>(s.motionBlurMaxPixels));
        BossLog::Write(message);
    }
}

// 演出の被写界深度（c-280b71）：拍（Focus）が入れた距離と強さで、描画のフレームごとに頼む。拍が無い間・一息の段・
// 飛ばした後は、focusFadeSeconds で外す（外れ切ったら頼まない＝カメラの元の設定）。
void SubmitFocus(HIKARI::WorldView& world, BossIntroComponent& intro, uint64_t owner, float unscaledSeconds) {
    const BossIntroSettings& s = intro.settings;
    BossIntroState& state = intro.state;
    const bool wanted = state.focusSeen && state.phase == BossIntroPhase::Show && !state.skipped;
    if (wanted) {
        // 距離は、拍が線形に寄せた値をそのまま（カットの始めの段は、その値へ跳ぶ）。
        state.focusDistance = state.focusWantDistance;
        state.focusStrength = state.focusWantStrength;
    }
    const float step = s.focusFadeSeconds > 0.0f ? std::max(0.0f, unscaledSeconds) / s.focusFadeSeconds : 1.0f;
    state.focusWeight = wanted ? std::min(1.0f, state.focusWeight + step) : std::max(0.0f, state.focusWeight - step);
    bool accepted = false;
    if (state.focusWeight > 0.0f && state.focusDistance > 0.0f) {
        const float distance = state.focusDistance;
        HIKARI::GameProjectCameraDepthOfFieldRequest request{};
        request.cameraObjectHandle = 0; // いま画を出しているカメラ
        request.sourceId = kFocusSource ^ owner;
        request.priority = kMotionBlurPriority;
        request.weight = state.focusWeight;
        request.settings.enabled = 1;
        request.settings.focusDistance = distance;
        request.settings.focusRange = distance * std::max(0.0f, s.focusRangeShare);
        request.settings.nearTransition = std::max(0.05f, distance * s.focusNearShare);
        request.settings.farTransition = std::max(0.05f, distance * s.focusFarShare);
        request.settings.nearBlurRadius1080pPixels =
            std::clamp(s.focusNearBlurPixels * state.focusStrength, 0.0f, 64.0f);
        request.settings.farBlurRadius1080pPixels = std::clamp(s.focusFarBlurPixels * state.focusStrength, 0.0f, 64.0f);
        accepted = world.GetCamera().SubmitDepthOfField(request);
    }
    if (accepted != state.focusAccepted) {
        state.focusAccepted = accepted;
        char message[128]{};
        std::snprintf(message, sizeof(message), "IntroFocus accepted=%d weight=%.2f distance=%.2f strength=%.2f",
                      static_cast<int>(accepted), static_cast<double>(state.focusWeight),
                      static_cast<double>(state.focusDistance), static_cast<double>(state.focusStrength));
        BossLog::Write(message);
    }
}

// 上下の黒帯（c-280b71）：演技の間、画を barsAspect の横縦比に見せる。UI の黒い矩形 2 枚（ブルームの後に載る）。
// 演技が済んだ・飛ばしたら、barsFadeSeconds で引っ込む。
void SubmitBars(HIKARI::WorldView& world, BossIntroComponent& intro, float unscaledSeconds) {
    const BossIntroSettings& s = intro.settings;
    BossIntroState& state = intro.state;
    const bool wanted = state.phase == BossIntroPhase::Show && !state.skipped && s.barsAspect > 0.0f;
    const float step = s.barsFadeSeconds > 0.0f ? std::max(0.0f, unscaledSeconds) / s.barsFadeSeconds : 1.0f;
    state.bars = wanted ? std::min(1.0f, state.bars + step) : std::max(0.0f, state.bars - step);
    HIKARI::GameProjectUiSpriteRequest request{};
    // 帯 1 本の高さ（参照の画面 1920×1080 の画素）：画面の比より横長に見せる分だけ。
    const float full = 0.5f * (request.referenceHeight - request.referenceWidth / std::max(s.barsAspect, 0.1f));
    const float height = full * Smooth(state.bars);
    if (!(height > 0.5f))
        return;
    request.order = kBarOrder;
    request.x = 0.0f;
    request.width = request.referenceWidth;
    request.height = height;
    request.tint = {0.0f, 0.0f, 0.0f, 1.0f};
    request.sourceId = kBarTopId;
    request.y = 0.0f;
    (void)world.GetUi().SubmitSprite(kSolidTexture, request);
    request.sourceId = kBarBottomId;
    request.y = request.referenceHeight - height;
    (void)world.GetUi().SubmitSprite(kSolidTexture, request);
}

} // namespace

void BossIntroEffects::Feedback(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                                const BossBodySettings& body, BossIntroComponent& intro, float unscaledSeconds) {
    const BossIntroSettings& s = intro.settings;
    BossIntroState& state = intro.state;
    BossCinematicFxState& fx = boss.cinematicFxState;
    // 全画面の衝撃の輪（刀が抜けた瞬間から）：実時間で広がる（時間が止まっている間も画の上を走る）。このフレームの強さを
    // 効果の口へ書く（出すのは BossCinematicFx::Feedback）。
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
    fx.postShock = shock;
    fx.postShockRadius = radius;
    fx.postShockAt = state.shockAt;
    // 抜刀の後の見得の間の張りつめ（握っている間・前触れの間は、拍が毎フレーム入れる）。
    if (state.phase == BossIntroPhase::Show && !state.skipped)
        for (int k = 0; k < state.cueCount; ++k)
            if (state.cues[static_cast<size_t>(k)].kind == BossIntroCueKind::SwordOut &&
                state.cues[static_cast<size_t>(k)].left)
                fx.tensionPending = std::max(fx.tensionPending, s.finaleTension);
    // 剣先の星（実時間で消える）。置き場所は役者の刀の先（読めなければ、画面の上の真ん中）。
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
    fx.postStar = star;
    fx.postStarPointValid = false;
    BossBodyPose pose{};
    if (star > 0.0f && BossIntroActor::Pose(intro, pose)) {
        fx.postStarPointValid = true;
        fx.postStarPoint = Add(pose.sword.position,
                               Mul(Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f}), VisibleBladeReach(boss, body)));
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
    SubmitMotionBlur(world, intro, object.GetHandle().value, unscaledSeconds);
    SubmitFocus(world, intro, object.GetHandle().value, unscaledSeconds);
    SubmitBars(world, intro, unscaledSeconds);
}

} // namespace Project_TD2_2
