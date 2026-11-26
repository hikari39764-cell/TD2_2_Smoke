#include "BossCinematicFx.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/CameraViewSnapshot.h"
#include "BossBodyMath.h"
#include "../Boss/BossVfx.h"
#include "../Player/Feedback.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;

// 手応えの出し元の番号（ほかの手応えと重ねない）と、優先度（崩しの手応えより下）。
constexpr uint64_t kAccentCameraSource = 0x5444324A11ull;
constexpr uint64_t kAccentHitStopSource = 0x5444324A12ull;
constexpr uint64_t kAccentSlowSource = 0x5444324A14ull;
constexpr uint64_t kRumbleCameraSource = 0x5444324A15ull;
// 続けて出す揺れの 1 周（ラジアン）。全画面の効果の張りつめ・暗さを、頼まれた値へ寄せる速さ (1/秒。実時間) と、これより
// 弱ければ出さない下限。
constexpr float kRumbleTurn = 6.2831853f;
constexpr float kPostEaseRate = 6.0f;
constexpr float kPostFloor = 0.004f;
// 白黒の閃き（Action Post）の送り手。口は 1 つなので、ほかの全画面の効果（速度感）より高い優先度で出す。
constexpr uint64_t kMonoSource = 0x5444324A13ull;
constexpr int32_t kMonoPriority = 10;
constexpr int kAccentPriority = Project_TD2_2::Feedback::kBreakPriority - 10;
// 剣先の星の大きさ（画面の高さを 1）。
constexpr float kStarSize = 0.5f;

// 白黒の閃きの、いまの強さと反転（出していなければ 0）。反転した白黒 → 白黒 → 色へ戻る（実時間で進む）。
void StepMono(BossComponent& boss, float unscaledSeconds, float& strength, float& invert) {
    const BossCinematicFxSettings& s = boss.cinematicFx;
    BossCinematicFxState& state = boss.cinematicFxState;
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

// 全画面の効果（BossImpactMono：白黒の閃き・衝撃の輪・張りつめ・暗さ・剣先の星）。出す間は毎フレーム送る。Action Post は
// 優先度の高い 1 つしか掛からないので、まとめて同じ効果の項目として送る。衝撃の輪と星は、登場の効果
// （BossIntroEffects::Feedback）がこのフレームの強さを書いておく。
void SubmitImpactPost(HIKARI::WorldView& world, BossComponent& boss, uint64_t owner, float unscaledSeconds) {
    const BossCinematicFxSettings& s = boss.cinematicFx;
    BossCinematicFxState& state = boss.cinematicFxState;
    float mono = 0.0f;
    float invert = 0.0f;
    StepMono(boss, unscaledSeconds, mono, invert);
    // 衝撃の輪と星（このフレームの分。読んだら 0 に戻す）。
    const float shock = state.postShock;
    const float radius = state.postShockRadius;
    const float star = state.postStar;
    const bool starPointValid = state.postStarPointValid;
    state.postShock = state.postShockRadius = state.postStar = 0.0f;
    state.postStarPointValid = false;
    // 張りつめ：握っている間・前触れの間（拍が毎フレーム入れる）と、抜刀の後の見得の間。画を暗くする頼み（前触れ）も
    // 同じ。どちらも急に切り替えず、実時間で寄せる（拍の終わりや演技の終わりで、画の明るさが跳ねない）。
    const float wanted = state.tensionPending;
    state.tensionPending = 0.0f;
    const float ease = std::clamp(std::max(0.0f, unscaledSeconds) * kPostEaseRate, 0.0f, 1.0f);
    state.tensionNow += (wanted - state.tensionNow) * ease;
    state.dimNow += (state.dimPending - state.dimNow) * ease;
    state.dimPending = 0.0f;
    const float tension = state.tensionNow > kPostFloor ? state.tensionNow : 0.0f;
    const float dim = state.dimNow > kPostFloor ? state.dimNow : 0.0f;
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
        ScreenUv(world, state.postShockAt, centerU, centerV);
    request.values[2] = {shock, radius, centerU, centerV};
    request.channelMasks[2] = 15u;
    request.values[3] = {tension, dim, 0.0f, 0.0f};
    request.channelMasks[3] = 3u;
    if (star > 0.0f) {
        // 星：剣先を画面へ写して、そこに出す（剣先が読めなければ画面の上の真ん中）。
        float starU = 0.5f;
        float starV = 0.3f;
        if (starPointValid)
            ScreenUv(world, state.postStarPoint, starU, starV);
        request.values[5] = {star, starU, starV, kStarSize};
        request.channelMasks[5] = 15u;
    }
    world.GetPresentationVfx().SubmitActionPostProfile(s.slashMonoProfile, request);
}

} // namespace

void BossCinematicFx::BeginFrame(BossComponent& boss) {
    BossCinematicFxState& state = boss.cinematicFxState;
    // 毎フレームの頼みは、ここで 0 に戻す（このあと登場の拍と変わり目の演出が、同じフレームの分を足す）。
    state.aura = 0.0f;
    state.bladeEdge = 0.0f;
    state.bladeSwing = false;
    state.eyeGlow = 0.0f;
}

void BossCinematicFx::Feedback(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                               float unscaledSeconds) {
    const BossCinematicFxSettings& s = boss.cinematicFx;
    BossCinematicFxState& state = boss.cinematicFxState;
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
    Project_TD2_2::Feedback::SubmitCameraShake(world, state.shake, {}, unscaledSeconds);
}

void BossCinematicFx::MonoFlash(BossComponent& boss, float strength, float seconds) {
    BossCinematicFxState& state = boss.cinematicFxState;
    state.monoPending = true;
    state.monoStrength = std::clamp(strength, 0.0f, 1.0f);
    state.monoLength = std::max(0.0f, seconds);
}

void BossCinematicFx::Shake(BossComponent& boss, float strength) {
    boss.cinematicFxState.accentPending = std::max(boss.cinematicFxState.accentPending, strength);
}

void BossCinematicFx::KeepRumble(BossComponent& boss, float strength) {
    boss.cinematicFxState.rumblePending = std::max(boss.cinematicFxState.rumblePending, strength);
}

void BossCinematicFx::HitStop(BossComponent& boss, int frames) {
    boss.cinematicFxState.hitStopPending = std::max(boss.cinematicFxState.hitStopPending, frames);
}

void BossCinematicFx::Slow(BossComponent& boss, int frames) {
    boss.cinematicFxState.slowPending = std::max(boss.cinematicFxState.slowPending, frames);
}

void BossCinematicFx::MistBurst(BossComponent& boss, HIKARI::GameProjectFloat3 at, float scale) {
    BossCinematicFxState& state = boss.cinematicFxState;
    state.flashPending = true;
    state.flashAt = at;
    state.flashScale = scale;
}

void BossCinematicFx::Aura(BossComponent& boss, float strength) {
    boss.cinematicFxState.aura = std::max(boss.cinematicFxState.aura, strength);
}

void BossCinematicFx::Shock(BossComponent& boss, int slot, HIKARI::GameProjectFloat3 at, float size,
                            float delaySeconds) {
    BossCinematicFxState& state = boss.cinematicFxState;
    state.shocks[static_cast<size_t>(std::clamp(slot, 0, static_cast<int>(state.shocks.size()) - 1))] = {at, size,
                                                                                                         delaySeconds};
}

void BossCinematicFx::Glint(BossComponent& boss, float brightness) { boss.cinematicFxState.glint = brightness; }

void BossCinematicFx::BladeEdge(BossComponent& boss, float strength) {
    boss.cinematicFxState.bladeEdge = std::max(boss.cinematicFxState.bladeEdge, strength);
}

void BossCinematicFx::EyeFlare(BossComponent& boss, float strength) {
    // 目の閃きは体の見た目の部品が出す（c-bd1a7a）：ここは頼みだけを書く。
    float& flare = boss.cinematicFxState.eyeFlareRequest;
    flare = std::max(flare, strength > 0.0f ? strength : boss.cinematicFx.eyesFlare);
}

} // namespace Project_TD2_2
