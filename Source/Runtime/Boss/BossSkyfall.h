#pragma once

#include "BossFrame.h"
#include "BossPose.h"

namespace Project_TD2_2 {

// 天降り斬り（型 skyfall）：屈む → 高く跳ぶ（少し後ろへ流れる）→ 空中でプレイヤーを向いたまま溜める →
// 「追尾を止める F」の時に落ちる所を決め、そこへ落ちる → 着地の周りのプレイヤーに当てる。
// 段階・溜めと落ちる進み・跳んだ所と落ちる所は boss.skyfall に毎 tick 書く（エフェクトが読む）。
// 体の高さは boss.height（跳躍と同じ）。空中の体はふだんどおり撃たれる（Damageable は止めない）。
class BossSkyfall final {
public:
    // 技を始めた tick（どの型でも呼ぶ）。天降り斬りなら跳ぶ所と前隙の分け方を決め、ほかの型なら様子を消す。
    // 溜めの揺らぎ（boss.windupHold）が決まった後に呼ぶ（揺らいだ分は空中の溜めに足す）。
    static void Begin(BossFrame& frame, const BossMove& move);
    // 前隙の 1 tick：屈む・跳び上がる・空中で溜める。落ちる所の見込みを更新し、追尾を止めた tick に決める。
    static void Pre(BossFrame& frame);
    // 攻撃中の 1 tick：落ちる区間は落ち、着地の当たりの区間はプレイヤーに当てる。
    static void Act(BossFrame& frame, const BossMove& move, const BossSegmentProgress& g);
    // 着地の当たりの区間に入った tick（着地）：落ちる所へ降ろし、着地の爆発を出す。
    static void Land(BossFrame& frame);
    // 後隙の 1 tick：着地で屈んだ体を起こす。
    static void Post(BossComponent& boss);
    // 技を終えた・打ち切られた（待機へ戻る・気絶・撃破・次の技）：様子を消す。reason はログ用。
    static void Reset(BossComponent& boss, const char* reason);
};

} // namespace Project_TD2_2
