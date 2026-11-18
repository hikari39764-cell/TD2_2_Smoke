#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossBodyRig.h"

namespace Project_TD2_2 {

// フェーズの変わり目の演出の見た目（c-8dd296）：膝をついて刀を突き立てる・左手が刀身を拭う・立ち上がって斬り上げて
// 抜く・フェーズ 2 の構えへ収まる体の動きと、継ぎ目から漏れる黒い煙・胸から回る火・刀身に灯る火・火の輪・閃光。
// 拍と F は規則（BossPhaseShift）が進める。ここは描画のフレームでそれを読み、外形の置き場所と煙・火を決める。
class BossPhaseShiftVisuals final {
public:
    // 描画のフレームごとに、拍に入ってからの秒を進める（外形を置く前に呼ぶ）。
    static void Advance(BossComponent& boss, float seconds);
    // 胴の煙の火の強さの下限（0〜1。燃の間に内から火が回る。BossBodyVisuals の StepFire が使う）。
    static float FireFloor(const BossComponent& boss);
    // 待機の姿に解いた外形（pose）を、変わり目の姿へ直す（演出の間だけ。ほかは何もしない）。
    static void ApplyPose(BossComponent& boss, const HIKARI::GameProjectTransform& body, BossBodyPose& pose);
    // 煙と火の注入（胴の煙の域）、火の輪、閃光の手応えの頼み（演出の後も、火の輪が消えるまで進める）。
    static void Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                       const BossBodyPose& pose, float seconds);
    static void Stop(HIKARI::WorldView& world, BossComponent& boss);
};

} // namespace Project_TD2_2
