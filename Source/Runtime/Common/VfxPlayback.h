#pragma once

#include "HIKARI/Project/VfxView.h"
#include "HIKARI/Project/WorldView.h"

namespace Project_TD2_2::VfxPlayback {

// 一度きりのエフェクト（当たった所の光・吸い取った合図など。入力は無い）を、世界の向きのまま一様な大きさで出す。
// 受け付けられたら true。資産が空かどうかは呼ぶ側が見る（空なら出さないのが呼ぶ側の決まり）。
inline bool PlayOneShot(HIKARI::WorldView& world, const HIKARI::GameProjectVfxAssetReference& asset,
                        HIKARI::GameProjectFloat3 position, float scale = 1.0f) {
    HIKARI::GameProjectVfxPlayRequest play{};
    play.asset = asset;
    play.transform.position = position;
    play.transform.scale = {scale, scale, scale};
    play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
    return world.GetVfx().Play(play).IsValid();
}

} // namespace Project_TD2_2::VfxPlayback
