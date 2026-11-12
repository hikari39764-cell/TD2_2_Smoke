#pragma once

#include "HIKARI/Project/ValueTypes.h"
#include "BossComponent.h"

namespace Project_TD2_2 {

// 技の選び方の入口。実行器（体・剣・斬撃）とは分け、選び方だけを差し替えられるようにする。
// 行動ツリーの brain（BossTreeBrain）はこれを実装し、節が BossRules::RequestMove で技を頼む。
struct BossBrainContext {
    BossComponent& boss;
    // ボスの物体（行動ツリーの部品を持つ物体）。
    HIKARI::GameProjectObjectHandle object{};
    // プレイヤーまでの水平距離（m）。プレイヤーがいなければ負。
    float playerDistance = -1.0f;
    int phaseNumber = 1;
    double healthPercent = 100.0;
    // 後隙の最後の tick（この tick に技を頼まなければ待機へ戻る）。OnRecovery のときだけ意味がある。
    bool recoveryDone = false;
};

class IBossBrain {
public:
    virtual ~IBossBrain() = default;
    // 待機中の固定 tick ごと。技を出すなら RequestMove する。
    virtual void OnIdle(BossBrainContext& context) = 0;
    // いまの技の攻撃中が終わった tick（後隙の前）。コンボを続けるなら RequestMove(..., true)。
    virtual void OnActFinished(BossBrainContext& context) = 0;
    // 後隙の固定 tick ごと。後隙を切り上げて次の技を出すなら RequestMove する
    // （asCombo が true ならコンボの続きとして数える）。
    virtual void OnRecovery(BossBrainContext& context) = 0;
};

// 確かめ用：「試す技」を 1 つ、または表の技を順番に出す。表の順で、実行器が動かせない型は飛ばす。
class BossTestBrain final : public IBossBrain {
public:
    // 確かめ用の項目（試す技・順番に全部）が入っているボスは、行動ツリーではなくこちらが動かす。
    static bool Wants(const BossComponent& boss);
    void OnIdle(BossBrainContext& context) override;
    void OnActFinished(BossBrainContext& context) override;
    void OnRecovery(BossBrainContext& context) override;
};

} // namespace Project_TD2_2
