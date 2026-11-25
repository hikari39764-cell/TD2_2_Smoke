#pragma once

#include <cstddef>

#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"
#include "BossObserver.h"

namespace Project_TD2_2 {

// 固定 tick の始めに見た、プレイヤーのヒョウタンと砲撃（最初の 1 つずつ）。
struct BossGourdSight {
    // 吸い込みの構えをしているか（煙が入るかどうかは問わない）。
    bool inhaling = false;
    // 飛ぶ斬撃・ナイフをあと取り込める煙の量（ヒョウタンの空きから、口へ引き込み中の分を引いた値）。
    float catchRoom = 0.0f;
    // 捕まえた斬撃・ナイフを引き込む先（最初のヒョウタンの口）。
    HIKARI::GameProjectFloat3 mouth{};
    bool hasMouth = false;
    // ボスが見るプレイヤーの道具。
    BossGearSight gear;
};

// ボスとプレイヤーのヒョウタン・砲撃のつなぎ（c-855f05 で BossSystem から分けた）。ヒョウタンと砲撃は、その Rules の
// 関数だけで読み書きする。
class BossGourdLink final {
public:
    // 固定 tick の始め：吸い込み中か・捕まえられる空き・口・ボスが見る道具。ボスたちが口へ引き込み中の分を空きから引く。
    static BossGourdSight Sense(HIKARI::WorldView& world);
    // 固定 tick の終わり：吸い取った斬撃・ナイフをヒョウタンへ渡す。引き込み中の数（pulling）がある間は、吸い込みを
    // やめていても口が飲み込み続ける。
    static void HandOff(HIKARI::WorldView& world, BossCatchHandoff& caught, int pulling);
    // 描画のフレーム：斬撃のエフェクトが使う流体域の分だけ、砲撃の炎の枠を譲ってもらう（毎フレーム入れ直す）。
    static void ReserveFluidDomains(HIKARI::WorldView& world, size_t demand);
    // 煙の量の、場の霧の量への換算（最初のヒョウタンの値。いなければ既定値）。
    static float SmokePerVolume(HIKARI::WorldView& world);
};

} // namespace Project_TD2_2
