#include "BossIntroCamera.h"

#include <algorithm>

#include "BossBodyMath.h"
#include "../Boss/BossIntro.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {

using namespace BossBodyMath;

void BossIntroCamera::Update(const BossIntroComponent& intro, Player& player) {
    const BossIntroState& state = intro.state;
    // 演技の後の一息の段：演出の資産が最後に見せていたカメラの姿勢から、ゲームのカメラへ滑らかに渡す（切り替えない）。
    // 演技の間も、同じ頼みを重み 0（演出の姿勢そのまま）で出しておく：プレイヤーのカメラ（PlayerSystem）はボスより先に
    // 進むので、資産を止めた tick に初めて頼むと、その tick の画には間に合わず、1 フレームだけゲームのカメラの素の画
    // （プレイヤーの後ろから）が出る。資産が流れている間は、資産のカメラが画を握っているので、頼んでも画は変わらない。
    const bool show = state.phase == BossIntroPhase::Show;
    if ((!show && state.phase != BossIntroPhase::Settle) || !state.filmValid || state.skipped)
        return;
    const float u =
        show ? 0.0f : static_cast<float>(state.frame) / std::max(1.0f, intro.settings.settleSeconds * 60.0f);
    PlayerCinematicRequest request;
    request.eye = state.filmEye;
    request.target = state.filmTarget;
    request.fovRadians = state.filmFov;
    request.gameWeight = Smootherstep(u);
    // ゲームのカメラはボスへ向けておく（戻り切った時にボスを見ている）。
    request.focus = Add(state.origin, Vec3{0.0f, intro.settings.cameraFocusHeight, 0.0f});
    PlayerRules::RequestCinematic(player, request);
}

} // namespace Project_TD2_2
