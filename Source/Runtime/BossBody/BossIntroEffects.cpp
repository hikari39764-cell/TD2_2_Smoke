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
#include "BossCinematicFx.h"
#include "BossIntroActor.h"
#include "BossIntroSmoke.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossIntro.h"
#include "../Player/Feedback.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;

// 演出の動きのぼかしの出し元の番号（c-bc5908）と優先度（演出の効果の手応えと同じ。崩しの手応えより下）。
constexpr uint64_t kMotionBlurSource = 0x5444324A16ull;
constexpr int kMotionBlurPriority = Project_TD2_2::Feedback::kBreakPriority - 10;
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

// 効果の拍を、演技の時計 t (秒) まで進める。時計が段より前へ戻った時（プレビューで巻き戻した時）は、黙って入る前に戻す。
void StepCues(BossComponent& boss, BossIntroComponent& intro, float t) {
    BossIntroState& state = intro.state;
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
}

namespace {

// 演出の動きのぼかし（c-bc5908）：演技の間は設定のまま、一息の段ではゲームのカメラへ渡すのに合わせて弱める
// （BossIntroCamera の渡しと同じ進み）。飛ばした時は掛けない。頼みは 1 フレームだけ効くので、描画のフレームごとに出す。
void SubmitMotionBlur(HIKARI::WorldView& world, BossIntroComponent& intro, uint64_t owner) {
    const BossIntroSettings& s = intro.settings;
    BossIntroState& state = intro.state;
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
        blur.priority = kMotionBlurPriority;
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
    SubmitMotionBlur(world, intro, object.GetHandle().value);
}

} // namespace Project_TD2_2
