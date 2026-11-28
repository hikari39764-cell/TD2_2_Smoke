#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossBodyComponent.h"
#include "BossBodyRig.h"
#include "../Boss/BossPhaseShiftComponent.h"

namespace Project_TD2_2 {

// フェーズの変わり目の演出の見た目（c-271cba で動捕の演出に作り直し）：骨格の体に流すクリップの時刻、打ち退けの停止と揺れ、
// 継ぎ目から漏れる黒い煙、太陽を掴む（画面の星・吸い込む輪・暗さ）、拳から全身へ回る火、目の熱さ、場の色、刀へ移る火、
// 振りかぶりの張りつめ、一閃（閃光・衝撃・火の輪・停止・揺れ）。時刻は演出の資産の時刻、拍は資産の事件段（Shift.<拍>）。
// 段は規則（BossPhaseShift）が進める。設定と状態は同じ物体の BossPhaseShift。shift が nullptr の時は何もしない。
class BossPhaseShiftVisuals final {
public:
    // 描画のフレームの始め：見た目の時計を資産の時刻（の 1 歩先）に合わせ、骨格の体にクリップとその時刻を頼む。
    // 演出の資産のカメラの姿勢を控える（渡しの元）。
    static void Advance(HIKARI::WorldView& world, Boss& boss, BossBody& look,
                        BossPhaseShift* shift, float seconds);
    // 胴の煙の火の強さの下限（0〜1。火が拳から胴へ回る間に上がる。BossBodyVisuals の StepFire が使う）。
    static float FireFloor(const BossPhaseShift* shift);
    // 胴の煙の量に掛ける倍率（打ち退けられてから火が回るまで薄くする。BossBodyVisuals の UpdateSmoke が使う）。
    static float SmokeScale(const BossPhaseShift* shift);
    // 胴の火の粉と熱のゆがみに掛ける倍率（燃え上がりの後しばらく増やす。ふだんは 1。同じく UpdateSmoke が使う）。
    static float BurnBoost(const BossPhaseShift* shift);
    // 場の色の下限（0〜1。太陽を掴んでから変わっていく。BossPhaseTwoField が使う）。
    static float FieldFloor(const BossPhaseShift* shift);
    // 演出の間、胴の煙の域を置く体の変換（骨格の体の腰の真上へ寄せる。クリップの中で体が下がる・戻るのに付いて行く）。
    static HIKARI::GameProjectTransform SmokeBody(const BossPhaseShift* shift, const BossBody& look,
                                                  const HIKARI::GameProjectTransform& body);
    // 演出の間の、刀身の灼け・目の熱さ・刃口の光・気配・張りつめ。
    static void ApplyPose(Boss& boss, BossPhaseShift* shift, BossBodyPose& pose);
    // 煙と火の注入（胴の煙の域）、画面の効果の頼み、火の輪、一閃（演出の後も、火の輪が消えるまで進める）。
    static void Render(HIKARI::WorldView& world, HIKARI::ObjectView object, Boss& boss,
                       BossBody& body, BossPhaseShift* shift, const BossBodyPose& pose,
                       float seconds);
    // 演出を飛ばしたのを見たフレームか（このフレームは火の舌を入れない。BossBodyVisuals の火の舌が見る）。
    static bool HoldTongues(const BossPhaseShift* shift);
    static void Stop(HIKARI::WorldView& world, BossPhaseShift* shift);
    // 編集中（Play の外）：演出の作業場がこの資産をプレビューしている間、骨格の体をその時刻のクリップの姿にする（カメラを
    // 合わせやすく）。プレビューをやめたら姿の層を外す。
    static void Preview(HIKARI::WorldView& world, const Boss& boss, BossPhaseShift& shift);
};

} // namespace Project_TD2_2
