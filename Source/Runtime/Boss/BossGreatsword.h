#pragma once

#include "BossFrame.h"
#include "BossPose.h"

namespace Project_TD2_2 {

// 霞の大剣（型 greatsword）：前隙で右肩の上に大剣を立てて作り出し（柄から剣先へ満ちる）、前隙の最後に右後ろへ下ろして
// 構え、攻撃中にボスの中心を軸に右後ろから正面を通って左後ろへ薙ぐ。当たりは剣の角度と距離で見る（高さは見ない）。
// 剣の形（段階・満ちた割合・柄の位置・向き・長さ）は boss.greatsword に毎 tick 書く（エフェクトが読む）。
// 見えない剣（BossPose の剣の当たり・霞）は使わない（剣を振る型ではないので、斬撃も飛ばない）。
class BossGreatsword final {
public:
    // 技を始めた tick（どの型でも呼ぶ）。霞の大剣なら剣の長さと炎かどうかを決め、ほかの型なら形を消す。
    // 剣が燃えるか（boss.bladeLit）が決まった後に呼ぶ。
    static void Begin(BossFrame& frame, const BossMove& move);
    // 前隙の 1 tick：剣を作り出し（満ちていく）、最後に薙ぎ始めの構えへ下ろす。
    static void Pre(BossFrame& frame, const BossMove& move);
    // 攻撃中に入った tick：薙ぐ向きの基準を、いまのボスの向き（追尾を止めた向き）に決める。
    static void BeginSweep(BossFrame& frame);
    // 攻撃中の 1 tick：薙ぐ。その tick に剣が通った角度にいるプレイヤーに当て、「弧」の火を塗る。
    static void Act(BossFrame& frame, const BossMove& move, const BossSegmentProgress& g);
    // 後隙の 1 tick：薙ぎ終えた所で剣がほどける。ほどけ終えたら形を消す。
    static void Post(BossComponent& boss);
    // 技を終えた・打ち切られた（待機へ戻る・気絶・撃破・次の技）：形を消す。reason はログ用。
    static void Reset(BossComponent& boss, const char* reason);
};

} // namespace Project_TD2_2
