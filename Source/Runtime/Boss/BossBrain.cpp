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

void BossTestBrain::OnActFinished(BossBrainContext& context) {
    // 確かめ用は、ふだんはコンボを出さない（どの技も後隙まで見せる）。コンボは行動ツリーの brain が表のとおりにつなぐ。
    // testChain の時だけ、確率・距離を見ずにコンボ先の最初の行へつなぐ（コンボの起こりの姿勢を毎回同じ手順で見る）。
    BossComponent& boss = context.boss;
    const BossMove* move = BossRules::GetMove(boss, boss.move);
    if (!boss.testChain || move == nullptr || move->comboTargets.empty() || boss.chain + 1 >= boss.comboMaxChain)
        return;
    BossRules::RequestMove(boss, move->comboTargets.front(), true);
}

void BossTestBrain::OnRecovery(BossBrainContext&) {
    // 確かめ用は後隙を切り上げない。
}

void BossTestBrain::OnWalking(BossBrainContext&) {
    // 確かめ用は歩きを切り上げない。
}

} // namespace Project_TD2_2
