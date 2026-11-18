#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossBodyRig.h"

namespace Project_TD2_2 {

// 登場の見た目（c-429242）：立ち込める黒い煙・とぐろの煙・散らばった鎧・地面に刺さった刀と、煙が鎧に憑いて形になる動き・抜刀。
// 段と F は規則（BossIntro）が進める。ここは、描画のフレームでそれを読んで、外形の置き場所と煙を決めるだけ。
class BossIntroVisuals final {
public:
    // 描画のフレームごとに、段に入ってからの秒を進める（外形を置く前に呼ぶ）。
    static void Advance(BossComponent& boss, float seconds);
    // 待機の姿に解いた外形（pose）を、登場の姿へ直す：置いてある所・飛んでいる途中の鎧、刺さった刀、柄へ伸びる右手。
    // 構えに収まった後は何もしない。
    // 部品は、煙（付く所）にばねで遅れて付いて行く（seconds＝このフレームの秒）。部品・刀が出す煙も決める（Render が出す）。
    static void ApplyPose(BossComponent& boss, const HIKARI::GameProjectTransform& body, BossBodyPose& pose,
                          float seconds);
    // 胴の煙の量に掛ける倍率（組み上がる前は 0＝出さない。組み上がりの始めは柱として濃い）。
    static float SmokeScale(const BossComponent& boss);
    // とぐろの黒い煙：エフェクトを置き、巡る頭の位置へ煙を入れる（流体への提出なので PreRender で呼ぶ）。
    static void Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds);
    // 拍の手応え（カメラの揺れ・時間の停止・振動）を出す（描画のフレームごとに呼ぶ）。
    static void Feedback(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                         float unscaledSeconds);
    static void Stop(HIKARI::WorldView& world, BossComponent& boss);
};

} // namespace Project_TD2_2
