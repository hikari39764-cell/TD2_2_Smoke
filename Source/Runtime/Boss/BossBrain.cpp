#include "BossBrain.h"

#include "BossRules.h"

namespace Project_TD2_2 {

bool BossTestBrain::Wants(const BossComponent& boss) { return boss.testSequence || !boss.testMove.empty(); }

void BossTestBrain::OnIdle(BossBrainContext& context) {
    BossComponent& boss = context.boss;
    const bool firstMove = boss.sequenceCursor == 0;
    if (boss.idleFrames < (firstMove ? boss.testStartFrames : boss.testGapFrames))
        return;
    const int count = BossRules::GetMoveCount(boss);
    if (!boss.testMove.empty() && !boss.testSequence) {
        if ((firstMove || boss.testLoop) && BossRules::RequestMoveByName(boss, boss.testMove))
            ++boss.sequenceCursor;
        return;
    }
    if (!boss.testSequence || count <= 0)
        return;
    // 順番に全部：動かせない型の行は飛ばす。
    for (int tries = 0; tries < count; ++tries) {
        if (boss.sequenceCursor >= count) {
            if (!boss.testLoop)
                return;
            boss.sequenceCursor = 0;
        }
        const int index = boss.sequenceCursor++;
        if (BossRules::RequestMove(boss, index))
            return;
    }
}

void BossTestBrain::OnActFinished(BossBrainContext&) {
    // 確かめ用はコンボを出さない（どの技も後隙まで見せる）。コンボは行動ツリーの brain が表のとおりにつなぐ。
}

void BossTestBrain::OnRecovery(BossBrainContext&) {
    // 確かめ用は後隙を切り上げない。
}

} // namespace Project_TD2_2
