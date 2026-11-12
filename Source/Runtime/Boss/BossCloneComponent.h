#pragma once

#include <cstdint>
#include <string_view>

#include "HIKARI/Project/GameComponent.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kBossCloneType = "Project_TD2_2.BossClone";

// ボスの霞分身 1 体（シーンに置いた物体に付ける。見た目はボスと同じモデル、同じ物体に Damageable も付ける）。
// 出ていない間は地面の下へしまう。出す・消す・撃たれたは BossSystem（BossClones）が決める。
class BossCloneComponent final : public HIKARI::IGameComponent {
public:
    void DescribeProperties(HIKARI::PropertyFields& fields) override;

    // 出ていない間にしまっておく深さ（ボスの足元の地面から下へ m）。
    float parkDepth = 40.0f;

    // 実行時の状態は保存しない。
    bool initialized = false;
    bool active = false;
    HIKARI::GameProjectFloat3 ground{};
    float yaw = 0.0f;
    float untilSeconds = 0.0f;
    // 出てから数えた F（霧から浮かび上がる見せ方）。
    int age = 0;
    // 同じ物体の Damageable の命中の数（増えたら撃たれた）。
    uint64_t seenHits = 0;
};

} // namespace Project_TD2_2
