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
    if (state.phase != BossIntroPhase::Settle || !state.filmValid || state.skipped)
        return;
    const float u = static_cast<float>(state.frame) / std::max(1.0f, intro.settings.settleSeconds * 60.0f);
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
