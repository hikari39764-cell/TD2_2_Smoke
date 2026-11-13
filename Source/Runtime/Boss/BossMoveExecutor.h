#pragma once

#include "BossFrame.h"

namespace Project_TD2_2 {

// 技の実行器：表の 1 行を、前隙（体の構え）→ 攻撃中（剣を振って斬撃を飛ばす・移動）→ 後隙の順に最後まで動かす。
// どの技を出すかは決めない（IBossBrain が BossRules::RequestMove で頼む）。
class BossMoveExecutor final {
public:
    enum class StepResult { Running, ActFinished, Finished };

    static void Start(BossFrame& frame, int moveIndex, int chain);
    // 技の 1 tick。攻撃中が終わった tick は ActFinished（そこでコンボの要求があれば後隙を出さずに続ける）。
    static StepResult Step(BossFrame& frame);
    // 攻撃中が終わったあと、後隙へ入る。
    static void EnterPost(BossComponent& boss);
    static void EnterIdle(BossComponent& boss);
    // 待機中の 1 tick：プレイヤーを見たまま横へ漂う。
    static void StepIdle(BossFrame& frame);
    // 気絶・死亡などで技を打ち切る。飛んでいる斬撃はそのまま飛ぶ。
    static void Interrupt(BossComponent& boss);
    // 浮遊（地面から少し浮いて上下に漂う。攻撃するときは低く降りる）。
    static void StepFloat(BossComponent& boss);
    // プレイヤーの方へ毎秒 degreesPerSecond まで向く。
    static void FaceRate(BossFrame& frame, float degreesPerSecond);
    // 後ろへ跳ぶ（後ろへ下がる・退き溜めの退く所）。progress は跳ぶ間の進み（0〜1）。boss.stepFrom から、
    // プレイヤーと反対へ backstepDistance、高さ backstepHeight の弧で跳ぶ。
    static void StepBack(BossFrame& frame, float progress);
    // 動ける範囲（戦場の円）に収める。
    static void ClampToArena(const BossComponent& boss, HIKARI::GameProjectFloat3& point);
};

} // namespace Project_TD2_2
