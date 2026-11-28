#include "BossStateMachine.h"

#include <string>

#include "BossHealth.h"
#include "BossIntro.h"
#include "BossMoveExecutor.h"
#include "BossPhaseShift.h"
#include "BossRules.h"
#include "BossSelection.h"
#include "BossTableLoader.h"

namespace Project_TD2_2 {
namespace {

void StartRequested(BossFrame& frame, int chain) {
    Boss& boss = frame.boss;
    BossTableLoader::Load(frame.world, boss);
    // 連段の続きでない新しい技の前に、たまった体勢を決める（怯み・ひざまずきが先）。
    if (chain == 0 && BossHealth::ResolvePoise(frame)) {
        BossRules::CancelRequest(boss);
        return;
    }
    const int index = boss.requestedMove;
    BossRules::CancelRequest(boss);
    const BossMove* move = BossRules::GetMove(boss, index);
    if (move == nullptr || !move->executable) {
        // Play 中に表を保存して読み直した時、いまの技の行が無くなった・区間の数が変わったなら、古い番号のまま続けず
        // （次の tick に表の外や区間の外を読む）、打ち切って待機へ戻す。
        if (boss.state == BossState::Act) {
            const BossMove* current = BossRules::GetMove(boss, boss.move);
            if (current == nullptr || current->segments.size() != boss.segmentDone.size()) {
                BossMoveExecutor::Interrupt(boss);
                BossMoveExecutor::EnterIdle(boss);
            }
        }
        return;
    }
    BossMoveExecutor::Start(frame, index, chain);
    BossSelection::NoteStarted(boss, index, chain);
}

} // namespace

void BossStateMachine::Step(BossFrame& frame, IBossBrain& brain, BossBrainContext& context) {
    Boss& boss = frame.boss;
    switch (boss.state) {
    case BossState::Dead: ++boss.stateFrame; return;
    case BossState::Stunned:
        ++boss.stateFrame;
        boss.velocity = {};
        if (boss.stateFrame >= boss.stunFrames) {
            BossLog::Write(("StunEnd tick=" + std::to_string(boss.tick)).c_str());
            BossMoveExecutor::EnterIdle(boss);
        }
        return;
    case BossState::Idle:
        // 戦いが始まるまで（Flow が止めている間）は、歩かず技も出さない。
        // 登場が済むまでも同じ。
        if (boss.held || BossIntroRules::Holding(frame.intro)) {
            boss.velocity = {};
            return;
        }
        // フェーズの変わり目（c-3e0ffa）の間も、歩かず技も出さない（見た目は BossPhaseShiftVisuals）。
        if (BossRules::IsPhaseShifting(boss)) {
            ++boss.idleFrames;
            boss.velocity = {};
            return;
        }
        BossMoveExecutor::StepIdle(frame);
        // 変わり目の演出の後、少しの間は技を選ばない（構えたまま横へ歩く。次の技は行動ツリーが決める）。
        if (boss.requestedMove < 0 && !BossPhaseShiftRules::Calm(frame.shift))
            brain.OnIdle(context);
        if (boss.requestedMove >= 0)
            StartRequested(frame, 0);
        return;
    case BossState::Act: {
        const auto result = BossMoveExecutor::Step(frame);
        if (result == BossMoveExecutor::StepResult::ActFinished) {
            brain.OnActFinished(context);
            // コンボ：攻撃中が終わった時点で続ける。後隙は出さない。
            if (boss.requestedMove >= 0 && boss.requestedAsCombo)
                StartRequested(frame, boss.chain + 1);
            else
                BossMoveExecutor::EnterPost(boss);
        } else if (boss.phase == BossActPhase::Post) {
            // 後隙：brain が技を頼めば切り上げて出す（連続攻撃への割り込み）。頼まれなければ最後まで待って待機へ。
            // 後隙の最初の recoveryGuardFrames は brain に聞かない：大技の後の隙を、撃ち返されて切り上げられる前に咎められる。
            context.recoveryDone = result == BossMoveExecutor::StepResult::Finished;
            if (context.recoveryDone || boss.stateFrame >= boss.recoveryGuardFrames)
                brain.OnRecovery(context);
            if (boss.requestedMove >= 0)
                StartRequested(frame, boss.requestedAsCombo ? boss.chain + 1 : 0);
            else if (context.recoveryDone && !BossHealth::ResolvePoise(frame))
                BossMoveExecutor::EnterIdle(boss);
        } else if (BossMoveExecutor::IsWalking(boss)) {
            // 歩きの技の間も木を進める。「移動中も反応する」反応の枝（射撃をかわす霞歩）だけが、歩きを切り上げて技を出す。
            brain.OnWalking(context);
            if (boss.requestedMove >= 0)
                StartRequested(frame, 0);
        }
        return;
    }
    }
}

} // namespace Project_TD2_2
