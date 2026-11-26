#pragma once

#include "HIKARI/Extend/World.h"

namespace Project_TD2_2 {

// 手応えを出すカメラの物体（ハンドル）を決める：頼まれた物体、操作中のカメラ、プレイヤーのカメラの順。
class FeedbackCamera final {
public:
    static uint64_t Resolve(HIKARI::WorldView& world, HIKARI::Entity preferred = {});
};

} // namespace Project_TD2_2
