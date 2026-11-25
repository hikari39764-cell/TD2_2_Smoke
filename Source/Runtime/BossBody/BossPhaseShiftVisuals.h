#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossBodyComponent.h"
#include "BossBodyRig.h"
#include "../Boss/BossPhaseShiftComponent.h"

namespace Project_TD2_2 {

// フェーズの変わり目の演出の見た目（c-8dd296）：継ぎ目から漏れる黒い煙・胸から回る火・刀身に灯る火・火の輪・閃光。
// 体は骨格の体が待機の姿で見せる。
// 拍と F は規則（BossPhaseShift）が進める。ここは描画のフレームでそれを読み、体に付く効果の置き場所と煙・火を決める。
// 設定と状態は同じ物体の BossPhaseShiftComponent（c-3a0f87）。shift が nullptr（部品が無い）の時は何もしない。
class BossPhaseShiftVisuals final {
public:
    // 描画のフレームごとに、拍に入ってからの秒を進める（効果の置き場所を決める前に呼ぶ）。
    static void Advance(BossPhaseShiftComponent* shift, float seconds);
    // 胴の煙の火の強さの下限（0〜1。燃の間に内から火が回る。BossBodyVisuals の StepFire が使う）。
    static float FireFloor(const BossPhaseShiftComponent* shift);
    // 演出の間の、刀身の灼け・刃口の光・気配（体は骨格の体が待機の姿で演じる。ほかは何もしない）。
    static void ApplyPose(BossComponent& boss, BossPhaseShiftComponent* shift, BossBodyPose& pose);
    // 煙と火の注入（胴の煙の域）、火の輪、閃光の手応えの頼み（演出の後も、火の輪が消えるまで進める）。
    static void Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                       BossBodyComponent& body, BossPhaseShiftComponent* shift, const BossBodyPose& pose,
                       float seconds);
    // 演出を飛ばしたのを見たフレームか（このフレームは火の舌を入れない。元の籠手の所から、構え直した籠手の所まで
    // 燃料が線に引かれないように。BossBodyVisuals の火の舌が見る）。
    static bool HoldTongues(const BossPhaseShiftComponent* shift);
    static void Stop(HIKARI::WorldView& world, BossPhaseShiftComponent* shift);
};

} // namespace Project_TD2_2
