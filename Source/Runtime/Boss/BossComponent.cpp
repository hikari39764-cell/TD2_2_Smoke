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
#include "../Common/ObjectVariables.h"

namespace Project_TD2_2 {

void BossComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    // 検査器の項目は、それを使う機能のファイルに書く（c-855f05）。並び・題・項目は前と同じ。
    BossTableLoader::Describe(fields, *this);
    BossMoveExecutor::DescribeSwing(fields, *this);
    BossHealth::Describe(fields, *this);
    BossSelection::Describe(fields, *this);
    BossMoveExecutor::DescribeMovement(fields, *this);
    BossTransform::Describe(fields, *this);
    BossMoveExecutor::DescribeMoveTypes(fields, *this);
    BossWaves::DescribeAbsorb(fields, *this);
    cinematicFx.Describe(fields);
    animBody.Describe(fields);
    kasumiStep.Describe(fields);
    gourdRead.Describe(fields);
    BossWaves::Describe(fields, *this);
    BossSmoke::Describe(fields, *this);
    BossVisuals::Describe(fields, *this);
    BossTestBrain::Describe(fields, *this);
}

void BossComponent::Update(HIKARI::GameComponentContext& context, float) {
    // プレイ中の世界だけで書く（編集中は書き手を作らない）。
    if (context.GetWorld().GetGameSession() == nullptr) {
        return;
    }
    constexpr std::string_view category = "Boss";
    ObjectVariables::PublishFloat(context, category, kBossHealthVariable, shownHealth, variableWriteWarned);
    ObjectVariables::PublishFloat(context, category, kBossMaxHealthVariable, shownMaxHealth, variableWriteWarned);
    ObjectVariables::PublishFloat(context, category, kBossBarShownVariable, shownBar, variableWriteWarned);
    ObjectVariables::PublishFloat(context, category, kBossCinematicVariable, shownCinematic, variableWriteWarned);
    ObjectVariables::PublishInt(context, category, kBossPhaseVariable, shownPhase, variableWriteWarned);
    ObjectVariables::PublishFloat(context, category, kBossPhaseSecondsVariable, shownPhaseSeconds, variableWriteWarned);
}

} // namespace Project_TD2_2
