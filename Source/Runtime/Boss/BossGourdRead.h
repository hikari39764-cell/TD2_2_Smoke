#pragma once

#include "HIKARI/Project/PropertyFields.h"

namespace Project_TD2_2 {

struct BossFrame;
struct BossWave;


class BossGourdRead final {
public:
    // 斬撃を放った：横の剣気なら、ローリングの癖の側へ向きを寄せる。
    static void LeanSlash(const BossFrame& frame, BossWave& wave);
};

} // namespace Project_TD2_2
