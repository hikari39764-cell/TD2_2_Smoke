#include "BossPhaseTwoField.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/EnvironmentView.h"
#include "BossBodyMath.h"
#include "BossPhaseShiftVisuals.h"
#include "../Boss/BossMath.h"
#include "../Boss/BossVfx.h"
#include "../Common/VfxParameters.h"
#include "../Common/FieldValues.h"

namespace Project_TD2_2 {
namespace {

// 場のエフェクトの入力（資産側の契約）：漂う火の粉の量（Float）、焦げた輪の濃さ（Color の A）、熾火の明るさ（Color）。
constexpr const char* kEmbersInput = "input.embers";
constexpr const char* kScorchInput = "input.scorch";
constexpr const char* kScorchGlowInput = "input.scorchGlow";
// 場の色は、進みがこれだけ変わるまで送り直さない（送るたびにエンジンが色調整の表を焼き直す）。
constexpr float kWarmSendStep = 0.01f;

// value を target へ、span 秒で 0〜1 を渡る一定の速さで寄せる（span が 0 ならすぐ）。
float Approach(float value, float target, float seconds, float span) {
    const float step = span > 0.0f ? std::max(0.0f, seconds) / span : 1.0f;
    return value < target ? std::min(target, value + step) : std::max(target, value - step);
}

HIKARI::GameProjectFloat3 Tint(HIKARI::GameProjectFloat3 base, HIKARI::GameProjectFloat3 tint, float t) {
    return {base.x * BossMath::Lerp(1.0f, tint.x, t), base.y * BossMath::Lerp(1.0f, tint.y, t),
            base.z * BossMath::Lerp(1.0f, tint.z, t)};
}

// 場の色：場面の環境の色調整（置き換える前に覚えた値）に、フェーズ 2 の色味を進みの分だけ掛けて置き換える。
// 進みが 0 に戻ったら置き換えを解く。
void UpdateGrade(HIKARI::WorldView& world, BossBody& look) {
    BossBodyState& state = look.state;
    const HIKARI::EnvironmentView environment = world.GetEnvironment();
    const float warm = state.fieldWarm;
    if (warm <= 0.0f) {
        if (state.fieldWarmSent >= 0.0f)
            environment.ClearColorGradingOverride();
        state.fieldWarmSent = -1.0f;
        state.fieldBaseSeen = false;
        return;
    }
    const bool settled = warm >= 1.0f && state.fieldWarmSent >= 1.0f;
    if (settled || (state.fieldWarmSent >= 0.0f && warm < 1.0f && std::abs(warm - state.fieldWarmSent) < kWarmSendStep))
        return;
    if (!state.fieldBaseSeen) {
        HIKARI::GameProjectColorGrading base{};
        if (!environment.GetColorGrading(base).Succeeded())
            return;
        state.fieldBase = base;
        state.fieldBaseSeen = true;
    }
    const BossBody& s = look;
    HIKARI::GameProjectColorGrading grading = state.fieldBase;
    grading.shadowsTint = Tint(grading.shadowsTint, ToGP(s.bodyFieldShadowsTint), warm);
    grading.midtonesTint = Tint(grading.midtonesTint, ToGP(s.bodyFieldMidtonesTint), warm);
    grading.highlightsTint = Tint(grading.highlightsTint, ToGP(s.bodyFieldHighlightsTint), warm);
    if (environment.SetColorGradingOverride(grading).Succeeded())
        state.fieldWarmSent = warm;
}

// 漂う火の粉（ボスの足元に付けて出し続ける）と焦げた輪（変わり目にボスが立っていた所）。
void UpdateEffects(HIKARI::WorldView& world, Boss& boss, BossBody& look, float seconds) {
    const BossBody& s = look;
    BossBodyState& state = look.state;
    const bool phaseTwo = BossBodyMath::PhaseTwoLook(boss);
    if (!s.bodyFieldEffect.IsSet() || (!phaseTwo && state.fieldWarm <= 0.0f && state.fieldScorchShow <= 0.0f)) {
        BossVfx::StopHandle(world, state.fieldEmbersVfx);
        BossVfx::StopHandle(world, state.fieldScorchVfx);
        state.fieldScorchPlaced = false;
        state.fieldScorchShow = 0.0f;
        return;
    }
    HIKARI::GameProjectVfxParameterOverrides embers{};
    VfxParameters::SetFloat(embers, kEmbersInput, s.bodyFieldEmbers * state.fieldWarm);
    VfxParameters::SetColorAlpha(embers, kScorchInput, {1.0f, 1.0f, 1.0f}, 0.0f);
    VfxParameters::SetColor(embers, kScorchGlowInput, {});
    BossVfx::StartOrSend(world, boss, state.fieldEmbersVfx, VfxPlayback::Reference(s.bodyFieldEffect), BossVfx::Placed(boss.ground, 1.0f), embers);

    // 焦げた輪：フェーズ 2 に入った時の足元に置き、現れたら撃破の後も残す（フェーズ 1 へ戻ったら消す）。
    if (phaseTwo && !state.fieldScorchPlaced) {
        state.fieldScorchAt = boss.ground;
        state.fieldScorchPlaced = true;
    }
    state.fieldScorchShow = Approach(state.fieldScorchShow, phaseTwo ? 1.0f : 0.0f, seconds, s.bodyFieldScorchSeconds);
    const float show = state.fieldScorchShow;
    HIKARI::GameProjectVfxParameterOverrides scorch{};
    VfxParameters::SetFloat(scorch, kEmbersInput, 0.0f);
    VfxParameters::SetColorAlpha(scorch, kScorchInput, {1.0f, 1.0f, 1.0f},
                                 std::clamp(s.bodyFieldScorch, 0.0f, 1.0f) * show);
    const float glow = s.bodyFieldScorchGlow * show * std::max(state.fieldWarm, 0.0f);
    VfxParameters::SetColor(scorch, kScorchGlowInput, {glow, glow, glow});
    BossVfx::StartOrSend(world, boss, state.fieldScorchVfx, VfxPlayback::Reference(s.bodyFieldEffect),
                         BossVfx::Placed(state.fieldScorchAt, std::max(0.1f, s.bodyFieldScorchSize)), scorch);
}

} // namespace

void BossPhaseTwoField::Update(HIKARI::WorldView& world, Boss& boss, BossBody& look,
                               const BossPhaseShift* shift, float seconds) {
    BossBodyState& state = look.state;
    const bool warm = BossBodyMath::PhaseTwoLook(boss) && boss.state != BossState::Dead;
    state.fieldWarm = Approach(state.fieldWarm, warm ? 1.0f : 0.0f, seconds, look.bodyFieldWarmSeconds);
    // 変わり目の演出（c-271cba）：太陽を掴んでから、演出が決める下限まで場の色を進める（フェーズ 2 の見た目の前から）。
    state.fieldWarm = std::max(state.fieldWarm, BossPhaseShiftVisuals::FieldFloor(shift));
    UpdateGrade(world, look);
    UpdateEffects(world, boss, look, seconds);
}

void BossPhaseTwoField::Stop(HIKARI::WorldView& world, BossBody& look) {
    BossBodyState& state = look.state;
    BossVfx::StopHandle(world, state.fieldEmbersVfx);
    BossVfx::StopHandle(world, state.fieldScorchVfx);
    if (state.fieldWarmSent >= 0.0f)
        world.GetEnvironment().ClearColorGradingOverride();
    state.fieldWarm = 0.0f;
    state.fieldWarmSent = -1.0f;
    state.fieldBaseSeen = false;
    state.fieldScorchPlaced = false;
    state.fieldScorchShow = 0.0f;
}

} // namespace Project_TD2_2
