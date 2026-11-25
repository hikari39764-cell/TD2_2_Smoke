#pragma once

#include "BossFrame.h"

namespace Project_TD2_2 {

// プレイヤーのロックオンへ、ボスの側から渡す物（照準補助の候補・カメラの見る点の高さ）と、倒れたボスのロックを外すこと。
// 候補はボスだけ（霞分身を消した c-69db00 の後。切り替え先が無いので、ロック対象を変える入力は押されたことを消すだけ）。
class BossLockTarget final {
public:
    // 固定 tick の最後（ボスの規則の後）に、プレイヤーがいる時だけ呼ぶ。
    static void Step(HIKARI::ObjectView bossObject, const BossComponent& boss, PlayerComponent& player);
};

} // namespace Project_TD2_2
