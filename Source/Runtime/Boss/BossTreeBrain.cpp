#include "BossTreeBrain.h"

#include "BossFrame.h"

namespace Project_TD2_2 {

void BossTreeBrain::Attach() {
    if (nodes_ != nullptr)
        return;
    nodes_ = BossBehaviorNodes::Acquire();
    if (nodes_ == nullptr)
        BossLog::Warn("行動ツリーの節を登録できません（Project.hikari に HIKARI.BehaviorKit が入っているか確かめる）。"
                      "ボスは技を選びません");
}

void BossTreeBrain::OnIdle(BossBrainContext& context) { Think(context, BossBrainMoment::Idle); }

void BossTreeBrain::OnActFinished(BossBrainContext& context) { Think(context, BossBrainMoment::ActFinished); }

void BossTreeBrain::OnRecovery(BossBrainContext& context) { Think(context, BossBrainMoment::Recovery); }

void BossTreeBrain::Think(BossBrainContext& context, BossBrainMoment moment) {
    if (nodes_ == nullptr)
        return;
    BossComponent& boss = context.boss;
    BossThought thought{
        boss, moment, {context.phaseNumber, context.healthPercent, context.playerDistance}, context.recoveryDone};
    // 木の中の時間（Wait・Cooldown など）は、前に進めてからの時間で進める（前隙・攻撃中は木を進めないため）。
    const float delta = boss.clockSeconds - boss.brain.thoughtSeconds;
    boss.brain.thoughtSeconds = boss.clockSeconds;
    const auto outcome = nodes_->Think(thought, context.object, delta);
    if (outcome == HIKARI::BEHAVIOR::TickOutcome::Ticked || boss.brain.treeProblemLogged)
        return;
    boss.brain.treeProblemLogged = true;
    if (outcome == HIKARI::BEHAVIOR::TickOutcome::NoTree) {
        BossLog::Write("BrainNoTree ボスの物体に Behavior Tree（Update＝GameCode）が無いので、技を選びません");
        return;
    }
    char reason[256]{};
    nodes_->CopyLastError(reason, sizeof(reason));
    char message[512]{};
    std::snprintf(
        message, sizeof(message),
        "BrainNotReady tick=%llu 行動ツリーを進められません（木が未設定・部品が止めてある・木に誤りがある）%s%s",
        static_cast<unsigned long long>(boss.tick), reason[0] != '\0' ? "：" : "", reason);
    BossLog::Write(message);
}

} // namespace Project_TD2_2
