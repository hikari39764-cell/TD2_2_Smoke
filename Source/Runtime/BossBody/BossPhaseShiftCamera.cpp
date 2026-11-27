#include "BossPhaseShiftCamera.h"

#include <algorithm>

#include "BossBodyMath.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {

using namespace BossBodyMath;

void BossPhaseShiftCamera::Update(const BossPhaseShiftComponent& shift, Player& player) {
    const BossPhaseShiftState& state = shift.state;
    // 渡しの段：演出の資産が最後に見せていたカメラの姿勢から、ゲームのカメラへ滑らかに渡す（切り替えない）。
    // 演技の間も、同じ頼みを重み 0 で出しておく（登場と同じ：資産を止めた tick に初めて頼むと、1 フレームだけゲームの
    // カメラの素の画が出る。BossIntroCamera）。
    const bool show = state.beat == BossShiftBeat::Show;
    if ((!show && state.beat != BossShiftBeat::Settle) || !state.filmValid || state.skipped)
        return;
    const float u =
        show ? 0.0f : std::clamp(state.settleClock / std::max(0.01f, shift.settings.settleSeconds), 0.0f, 1.0f);
    PlayerCinematicRequest request;
    request.eye = state.filmEye;
    request.target = state.filmTarget;
    request.fovRadians = state.filmFov;
    request.gameWeight = Smootherstep(u);
    // ゲームのカメラはボスへ向けておく（戻り切った時にボスを見ている）。
    request.focus = Add(state.origin, Vec3{0.0f, shift.settings.cameraFocusHeight, 0.0f});
    PlayerRules::RequestCinematic(player, request);
}

} // namespace Project_TD2_2
