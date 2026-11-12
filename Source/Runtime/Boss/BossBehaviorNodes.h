#pragma once

#include <memory>

#include "HIKARI/BehaviorKit/BehaviorNodeApi.h"
#include "BossComponent.h"

namespace Project_TD2_2 {

// 行動ツリーを進める場面。木は「ボスが次を決められる時」にだけ進める（前隙・攻撃中は進めない）。
enum class BossBrainMoment {
    // 待機中の固定 tick ごと。
    Idle,
    // いまの技の攻撃中が終わった tick（コンボを決める）。
    ActFinished,
    // 後隙の固定 tick ごと（連続攻撃されたら切り上げて割り込める）。
    Recovery,
};

// 行動ツリーを 1 回進める間、木の節が読み書きするもの。参照はその 1 回の間だけ有効。
struct BossThought {
    BossComponent& boss;
    BossBrainMoment moment = BossBrainMoment::Idle;
    BossSituation situation;
    // 後隙の最後の tick（技を頼まなければ、この tick で待機へ戻る）。
    bool recoveryDone = false;
    // 「連続攻撃された」が成り立った。この回に割り込みの技が始まったら、反応を使ったことにする。
    bool pressureArmed = false;
    float pressureCooldown = 0.0f;
};

// 行動ツリー（HIKARI.BehaviorKit）へ登録する、ボスの節：
//   待ち（横歩き）／技を実行／連続攻撃された／フェーズ・HP
// 節の名前はプロセスで 1 つなので、登録は 1 つを分け合う（Acquire）。最後の持ち主が消えると登録も外れる。
class BossBehaviorNodes final {
public:
    // モジュールが全部読み込まれた後（システムの OnWorldAttached）に呼ぶ。
    // 行動ツリーのモジュールが無い・登録できない時は nullptr。
    static std::shared_ptr<BossBehaviorNodes> Acquire();

    // object の行動ツリー（Behavior Tree の部品、Update＝GameCode）を 1 回進める。節は thought を読んで技を頼む。
    HIKARI::BEHAVIOR::TickOutcome Think(BossThought& thought, HIKARI::GameProjectObjectHandle object,
                                        float deltaSeconds);
    // いま進めている 1 回ぶん。進めていない間（編集器が進み具合を読む時など）は nullptr。
    BossThought* Current() const { return thought_; }
    // 行動ツリーのモジュールが最後に知らせた誤り（木が読めない・節の種類が無いなど）。無ければ空。
    void CopyLastError(char* buffer, uint32_t capacity) const;

private:
    BossBehaviorNodes() = default;
    bool Register();

    HIKARI::BEHAVIOR::NodeRegistration registration_;
    const HIKARI::BEHAVIOR::BehaviorApi* api_ = nullptr;
    BossThought* thought_ = nullptr;
};

} // namespace Project_TD2_2
