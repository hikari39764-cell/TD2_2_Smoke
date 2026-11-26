#pragma once

#include "BossBrain.h"
#include "BossFrame.h"

namespace Project_TD2_2 {

// ボスの状態の進め方（c-855f05 で BossSystem から分けた）：撃破・気絶・待機・技の 4 つの状態を 1 固定 tick 進める。
// どの技を出すかは brain に聞き、頼まれた技を実行器（BossMoveExecutor）で始める。
class BossStateMachine final {
public:
    static void Step(BossFrame& frame, IBossBrain& brain, BossBrainContext& context);
};

} // namespace Project_TD2_2
