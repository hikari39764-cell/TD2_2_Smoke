#pragma once

#include <memory>

#include "BossBehaviorNodes.h"
#include "BossBrain.h"

namespace Project_TD2_2 {

// 行動ツリーで技を選ぶ brain。ボスの物体に付いた Behavior Tree（Update＝GameCode）を、ボスが次を決められる
// 固定 tick（待機中・攻撃中が終わった時・後隙）にだけ 1 回進める。選び方の中身は木と、木の節（BossBehaviorNodes）。
class BossTreeBrain final : public IBossBrain {
public:
    // 木の節を登録する。モジュールが全部読み込まれた後（システムの OnWorldAttached）に呼ぶ。
    void Attach();
    void OnIdle(BossBrainContext& context) override;
    void OnActFinished(BossBrainContext& context) override;
    void OnRecovery(BossBrainContext& context) override;

private:
    void Think(BossBrainContext& context, BossBrainMoment moment);

    std::shared_ptr<BossBehaviorNodes> nodes_;
};

} // namespace Project_TD2_2
