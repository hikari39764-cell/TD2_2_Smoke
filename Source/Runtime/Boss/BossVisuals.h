#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"
#include "BossPhaseShiftComponent.h"
#include "../BossBody/BossBodyComponent.h"

namespace Project_TD2_2 {

// ボスの霞のエフェクト（流体注入の資産）を出す・動かす・止める。規則（固定 tick）が決めた斬撃と剣の位置を見せるだけ。
class BossVisuals final {
public:
    // alpha は FrameContext::fixedInterpolationAlpha（引き込まれる斬撃・ナイフを、tick の間を補間した位置に置く）。
    // look・phaseShift は同じ物体の体の見た目・変わり目の演出の部品（無ければ nullptr）。
    static void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                       const BossBodyComponent* look, BossPhaseShiftComponent* phaseShift, float seconds, float alpha);
    static void Stop(HIKARI::WorldView& world, BossComponent& boss);
    // 斬撃のエフェクトがいま使っている・すぐ使う流体域の数（砲撃の枠を同じ数だけ譲ってもらう。描画のフレーム）。
    static size_t GetFluidDomainDemand(const BossComponent& boss, const BossBodyComponent* look);
    // 検査器の「見た目」の項目（斬撃・爆発・剣先・ナイフ・霞の輪のエフェクト）。
    static void Describe(HIKARI::PropertyFields& fields, BossComponent& boss);
};

} // namespace Project_TD2_2
