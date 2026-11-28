#include "BossComponent.h"

#include "BossBrain.h"
#include "BossHealth.h"
#include "BossMoveExecutor.h"
#include "BossSelection.h"
#include "BossSmoke.h"
#include "BossTableLoader.h"
#include "BossTransform.h"
#include "BossVisuals.h"
#include "BossWaves.h"

namespace Project_TD2_2 {

void Boss::Update(float) {
    if (!HIKARI::Scene::IsPlaying())
        return;
    // HUD などが読めるよう、HP・最大 HP・条を出すか・演出の間か・フェーズ・フェーズの秒を、ゲーム全体の値（Params）へ毎フレーム書く。
    HIKARI::Params::Set(kBossHealthVariable, shownHealth);
    HIKARI::Params::Set(kBossMaxHealthVariable, shownMaxHealth);
    HIKARI::Params::Set(kBossBarShownVariable, shownBar);
    HIKARI::Params::Set(kBossCinematicVariable, shownCinematic);
    HIKARI::Params::Set(kBossPhaseVariable, shownPhase);
    HIKARI::Params::Set(kBossPhaseSecondsVariable, shownPhaseSeconds);
}

} // namespace Project_TD2_2
