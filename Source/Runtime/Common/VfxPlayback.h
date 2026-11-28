#pragma once

#include <cstring>

#include "HIKARI/Extend/World.h"
#include "HIKARI/Project/VfxView.h"
#include "HIKARI/Project/WorldView.h"

namespace Project_TD2_2::VfxPlayback {

// 新しいゲーム層の項目のエフェクト（HIKARI::VfxAsset。GUID だけを持つ）を、下の層の再生の要求が読む参照へ写す。
inline HIKARI::GameProjectVfxAssetReference Reference(const HIKARI::VfxAsset& asset) {
    HIKARI::GameProjectVfxAssetReference reference{};
    static_assert(sizeof(asset.guid) <= sizeof(reference.assetGuid));
    std::memcpy(reference.assetGuid, asset.guid, sizeof(asset.guid));
    return reference;
}

// 下の層の参照は、そのまま返す（項目の型が違う所と同じ呼び方で使えるように）。
inline const HIKARI::GameProjectVfxAssetReference& Reference(const HIKARI::GameProjectVfxAssetReference& asset) {
    return asset;
}

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

inline bool PlayOneShot(HIKARI::WorldView& world, const HIKARI::VfxAsset& asset, HIKARI::GameProjectFloat3 position,
                        float scale = 1.0f) {
    return PlayOneShot(world, Reference(asset), position, scale);
}

} // namespace Project_TD2_2::VfxPlayback
