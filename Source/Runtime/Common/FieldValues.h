#pragma once

#include "HIKARI/Game.h"
#include "HIKARI/Project/ValueTypes.h"

#include "VfxPlayback.h"

// 部品の項目（新しいゲーム層の型）を、下の層の型へ写す。項目を全体で読む所に付ける（x・y・z だけ読む所は要らない）。
namespace Project_TD2_2 {

inline HIKARI::GameProjectFloat3 ToGP(const HIKARI::Vec3& value) { return {value.x, value.y, value.z}; }
inline const HIKARI::GameProjectFloat3& ToGP(const HIKARI::GameProjectFloat3& value) { return value; }

} // namespace Project_TD2_2
