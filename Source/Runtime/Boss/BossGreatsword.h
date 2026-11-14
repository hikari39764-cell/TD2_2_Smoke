#pragma once

#include "BossFrame.h"
#include "BossPose.h"

namespace Project_TD2_2 {

// 霞の大剣（型 greatsword）：前隙で右肩の上に大剣を立てて作り出し（柄から剣先へ満ちる）、前隙の最後に右後ろへ下ろして
// 構え、攻撃中にボスの中心を軸に右後ろから正面を通って左後ろへ薙ぐ。当たりは剣の角度と距離で見る（高さは見ない）。
// 縦割り（型 greatswordCleave）：同じく作り出した剣を前隙の最後に頭上へ振りかぶり、攻撃中に正面へ叩きつける。
// 当たりは 2 つ：叩きつける刃（ボスの正面の線）と、剣先が着いた所から正面へ地を走る噴き上がり（前線が通った所）。
// 連撃（技の表のコンボ先）で横薙ぎ⇔縦割りをつなぐ時は、剣をほどかずに、次の技の前隙で振りかぶる・構え直す。
// 剣の形（段階・満ちた割合・柄の位置・向き・長さ・噴き上がりの前線）は boss.greatsword に毎 tick 書く（エフェクトが読む）。
// 見えない剣（BossPose の剣の当たり・霞）は使わない（剣を振る型ではないので、斬撃も飛ばない）。
class BossGreatsword final {
public:
    // 技を始めた tick（どの型でも呼ぶ）。大剣の型なら剣の長さと炎かどうかを決め、ほかの型なら形を消す。
    // 大剣の技の攻撃中が終わった tick にコンボで大剣の技を始めた時は、剣を消さずに続ける。
    // 剣が燃えるか（boss.bladeLit）が決まった後に呼ぶ。
    static void Begin(BossFrame& frame, const BossMove& move);
    // 前隙の 1 tick：剣を作り出し（満ちていく）、最後に薙ぎ始めの構えへ下ろす・頭上へ振りかぶる。
    // 連撃でつないだ時は、前隙の全部で、前の一振りの終わりの形から構え直す・振りかぶる。
    static void Pre(BossFrame& frame, const BossMove& move);
    // 攻撃中に入った tick：薙ぐ向き・叩きつける向きを、いまのボスの向き（追尾を止めた向き）に決める。
    static void BeginSweep(BossFrame& frame);
    static void BeginSlam(BossFrame& frame);
    // 攻撃中の 1 tick：薙ぐ（通った角度にいるプレイヤーに当て、「弧」の火を塗る）か、叩きつける（正面の線にいる
    // プレイヤーに当て、剣先が地面に着いた tick に噴き上がりを走らせ、刃の下に「線」の火を塗る）。
    static void Act(BossFrame& frame, const BossMove& move, const BossSegmentProgress& g);
    // 後隙の 1 tick：薙ぎ終えた所でほどける。縦割りは刺さったまま待ってからほどける。ほどけ終えたら形を消す。
    static void Post(BossComponent& boss);
    // 技の実行の毎 tick（どの型・どの段階でも、技の 1 tick の始めに呼ぶ）：走っている噴き上がりを進めて当て、火を塗る。
    static void StepEruption(BossFrame& frame);
    // 技を終えた・打ち切られた（待機へ戻る・気絶・撃破・次の技）：形を消す（走っている噴き上がりも止める）。reason はログ用。
    static void Reset(BossComponent& boss, const char* reason);
};

} // namespace Project_TD2_2
