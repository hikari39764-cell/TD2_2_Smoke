#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"
#include "BossIntroComponent.h"
#include "BossPhaseShiftComponent.h"

namespace Project_TD2_2 {

// ボスの体の物体の置き場所（c-855f05 で BossSystem から分けた）：足元の高さに浮遊のゆれを足し、向きだけで回す。
// 撃破の後は縮めて消し、登場の演技の前と間は舞台に止める。
class BossTransform final {
public:
    // 規則の tick の終わりに、体の物体の変換を書く（体の姿勢は骨格の体の動画が見せる）。intro は同じ物体の登場の部品
    // （無ければ nullptr）。
    static void Apply(HIKARI::ObjectView object, Boss& boss, const BossIntro* intro,
                      const BossPhaseShift* shift);
    // 検査器の「撃破の後」の項目（体が縮んで消えるまで）。
};

} // namespace Project_TD2_2
