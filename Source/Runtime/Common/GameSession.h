#pragma once

#include <string_view>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/FrameContext.h"
#include "HIKARI/Project/WorldView.h"

// 遊んでいる最中かの判定と、入力の読み方（各 System で同じ書き方をしていたものをまとめた）。
namespace Project_TD2_2::GameSession {

// Play 中で、一時停止していない。新しい層の System は Play の間だけ呼ばれるので、一時停止かどうかだけを見る。
inline bool IsPlaying(HIKARI::WorldView& /*world*/, const HIKARI::FrameContext& frame) { return !frame.paused; }

// 固定 tick の中で進めてよいか（遊んでいる最中で、固定 tick の長さがある）。
inline bool IsPlayingFixed(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    return IsPlaying(world, frame) && frame.fixedDt > 0.0f;
}

// 入力の行動 id の今の状態。読めなければ何も押していない状態。
inline HIKARI::GameProjectInputActionState ReadAction(HIKARI::InputView input, std::string_view id) {
    HIKARI::GameProjectInputActionState action{};
    if (!input.GetAction(id, action))
        return {};
    return action;
}

} // namespace Project_TD2_2::GameSession
