#pragma once

#include "BossBodyRig.h"

namespace Project_TD2_2 {

// 刀と手の「見せる姿勢」（カード c-a08a28）。規則の剣の線（BossPose）は読むだけで、当たり・F 数は変えない。
// - 刀の向きは、見せる構え（待機の下段・溜めの誇張・後隙で戻る）をばねで追う。振っている間（boss.swordSmoke）だけ
//   規則の剣の線へ重ね、窓が閉じたら勢いのまま行き過ぎて戻る。
// - 刃（模型の -Y）は、振る間は動きの先へ、溜めの間はこれから斬る向きへ、待機では下へ向ける。刀身の軸まわりにだけ回す。
// - 握りは見せる位置をばねで追い、振る間は規則の線の上（腕を伸ばす分だけ線に沿って外へ）。
// - 窓の外の手と刀は体の傾きに付き、窓の間は規則の線の枠（体の向きだけ）へ寄せる。
// - 胴の捻り・兜の向き・左手の釣り合いもここで進め、鎧（BossBodyRig）が読む。
class BossBodySword final {
public:
    // 見せる姿勢を 1 フレーム進める（seconds は描画フレームの長さ。0 なら止めたまま）。
    static void Step(BossComponent& boss, float seconds);
    // 刀・握りの右手・籠手・左手の置き場所を決める。
    static void Solve(const BossComponent& boss, const HIKARI::GameProjectTransform& body, BossBodyPose& pose);
    // 確かめ用（Boss の bodyTraceMotion）：振っている間、刃の向きと規則の線とのずれを記録に出す。斬りの技の間は、
    // 刀身・握りと鎧の近似との隙間、手首から肩までの距離、剣先の高さも出す。
    static void Trace(const BossComponent& boss, const HIKARI::GameProjectTransform& body, const BossBodyPose& pose);
};

} // namespace Project_TD2_2
