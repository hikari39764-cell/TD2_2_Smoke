#pragma once

#include "BossFrame.h"

namespace Project_TD2_2 {

// 確かめ用（c-c010ef）：クリップの刀で当たる技が、どの間合い（距離・角度）なら立ったプレイヤーに当たるかを記録に出す。
// ボスの項目 reachProbe が入っていて、クリップの刀を読み終えた最初の tick に 1 度だけ測る。ボスを原点・正面固定に置き、
// プレイヤーの体（当たりの筒）を距離 × 角度の格子に置いて、ゲームと同じ当たりの式（刀の線を前の tick から掃く・刀の太さ・
// プレイヤーの筒）で当たるかを見る。高さを無視した筒でも測り、外れる理由が高さか向きかを分ける。
// 踏み込み・連撃の左右移動・追尾（ボスが振りながら動く・回る分）は入れない（その場で振った時の届く所）。
namespace BossReachProbe {

void Step(BossFrame& frame);

} // namespace BossReachProbe

} // namespace Project_TD2_2
