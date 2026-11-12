#pragma once

#include "BossFrame.h"

namespace Project_TD2_2 {

// 霞分身（試作の doClones・cloneShot・removeClone）と、ロック対象の切り替え（試作の cycleLock・relock）。
// 分身はシーンに置いた BossClone の物体で、出ていない間は地面の下へしまっておく。
class BossClones final {
public:
    // 霞分身の判定区間の最初の tick：本体と分身の場所を決めて本体を移す（分身の物体へは Step が渡す）。
    static void Place(BossFrame& frame, const BossMove& move);
    // 固定 tick の最後（ボスの規則の後）：分身を出す・しまう・時間で消す、撃たれた分身を知らせる、
    // プレイヤーのロック対象をボスと分身の間で切り替える。player はいなければ nullptr。
    static void Step(HIKARI::WorldView& world, HIKARI::ObjectView bossObject, BossComponent& boss,
                     PlayerComponent* player, HIKARI::GameProjectFloat3 playerCenter);
};

} // namespace Project_TD2_2
