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

void BossTestBrain::Describe(HIKARI::PropertyFields& fields, BossComponent& boss) {
    if (fields.Section("確かめ用（正式シーンでは使わない）")) {
        fields.Text("testMove", "試す技（行の名前。空は使わない）", boss.testMove);
        fields.Bool("testSequence", "表の技を順番に全部出す", boss.testSequence);
        fields.Bool("testLoop", "最後まで出したら最初から", boss.testLoop);
        fields.Int("testStartFrames", "最初の技までの待ち (F)", boss.testStartFrames, 0, 7200);
        fields.Int("testGapFrames", "技と技の間の待ち (F)", boss.testGapFrames, 0, 7200);
        fields.Bool("testHoldStill", "待機中に横歩きしない", boss.testHoldStill);
        fields.Bool("testChain", "試す技のコンボ先（表の最初の行）へ続ける（コンボの最大の数まで）", boss.testChain);
        fields.Int("testDamageAtFrame", "ボスに確かめのダメージを入れる F（-1 は使わない）", boss.testDamageAtFrame, -1,
                   72000);
        fields.Float("testDamageAmount", "確かめのダメージの量", boss.testDamageAmount, 0.0f, 100000.0f);
        fields.Int("testDamageRepeatFrames", "確かめのダメージの間隔 (F、0 は 1 回)", boss.testDamageRepeatFrames, 0,
                   7200);
        fields.Int("testDamageRepeatCount", "確かめのダメージの回数", boss.testDamageRepeatCount, 1, 7200);
        fields.Int("traceEveryFrames", "状態をログに出す間隔 (F、0 は出さない)", boss.traceEveryFrames, 0, 7200);
        fields.Int("randomSeed", "乱数の種（移動の長さ・浮く高さ）", boss.randomSeed, 0, 1000000);
        fields.Bool("reachProbe", "剣の技が当たる間合い（距離 × 角度）を測って記録に出す（始めに 1 度）",
                    boss.reachProbe);
        fields.Float("reachProbeDistanceMin", "間合いの計測：近い端 (m)", boss.reachProbeDistanceMin, 0.0f, 50.0f);
        fields.Float("reachProbeDistanceMax", "間合いの計測：遠い端 (m)", boss.reachProbeDistanceMax, 0.0f, 50.0f);
        fields.Float("reachProbeDistanceStep", "間合いの計測：距離の刻み (m)", boss.reachProbeDistanceStep, 0.05f,
                     5.0f);
        fields.Int("reachProbeAngleStep", "間合いの計測：角度の刻み (度)", boss.reachProbeAngleStep, 1, 90);
    }
}

} // namespace Project_TD2_2
