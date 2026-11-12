#pragma once

#include <memory>
#include <string_view>

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
    // 「反応の間隔」が通った。この回にすぐ後ろの技が始まったら、その節の間隔を始める（Pick のログは why=reaction）。
    // 技が出せなかったら取り消す（同じ回の別の枝の技を、反応として数えない）。
    bool reactionArmed = false;
    size_t reactionKey = 0;
    float reactionCooldown = 0.0f;
    char reactionLabel[64] = {};
};

// 行動ツリー（HIKARI.BehaviorKit）へ登録する、ボスの節：
//   待ち（横歩き）／技を実行／連続攻撃された／フェーズ・HP／分身を出せる／分身を撃たれた
//   ＋プレイヤーへの反応の節（BossReactionNodes）
// 節の名前はプロセスで 1 つなので、登録は 1 つを分け合う（Acquire）。最後の持ち主が消えると登録も外れる。
class BossBehaviorNodes final {
public:
    // モジュールが全部読み込まれた後（システムの OnWorldAttached）に呼ぶ。
    // 行動ツリーのモジュールが無い・登録できない時は nullptr。
    static std::shared_ptr<BossBehaviorNodes> Acquire();
    // 節の関数に渡る user（登録した BossBehaviorNodes）から、いま進めている 1 回ぶん。
    static BossThought* ThoughtOf(void* user) { return static_cast<BossBehaviorNodes*>(user)->Current(); }
    // UTF-8 の文字の途中で切らないように写す（target は capacity バイト、必ず 0 で終わる）。
    static void CopyText(char* target, size_t capacity, std::string_view text);

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
