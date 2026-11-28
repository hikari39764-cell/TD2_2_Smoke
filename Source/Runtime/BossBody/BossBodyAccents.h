#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossBodyComponent.h"
#include "BossBodyRig.h"
#include "../Boss/BossComponent.h"
#include "../Boss/BossIntroComponent.h"

namespace Project_TD2_2 {

// 技の点描の演出（見た目だけ）：刀の振りの刀光など、見える刀・体に付く飾り。
// 規則の剣の線・当たり・F 数は読むだけで変えない。地面や画面への予告は出さない。
class BossBodyAccents final {
public:
    // 胴の煙へ掛ける量と熱さ（溜め・刀の火・振り終わりの吹き出し）を進める。胴の煙を出す前に呼ぶ。
    static void StepSmoke(Boss& boss, BossBody& look, float seconds);
    // 体に付く効果の置き場所（pose）が決まった後に呼ぶ。body は体の物体の世界の変換。intro は同じ物体の登場の部品（無ければ
    // nullptr）。
    static void Update(HIKARI::WorldView& world, Boss& boss, BossBody& look,
                       BossIntro* intro, const HIKARI::GameProjectTransform& body, const BossBodyPose& pose,
                       float seconds);
    static void Stop(HIKARI::WorldView& world, BossBody& look);
    // いまの技の刀の見え方（c-ecd963）：霞＝剣気を飛ばす技の前隙〜振り、実＝実体の刀だけで当てる技の前隙〜振り、
    // どちらでもない＝待機・後隙・歩き・刀を振らない技。
    enum class BladeKind { None, Haze, Real };
    static BladeKind Kind(const Boss& boss);
};

} // namespace Project_TD2_2
