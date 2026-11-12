#pragma once

#include "GourdComponent.h"

namespace Project_TD2_2 {

// 参照はクエリの中だけで使用し、跨フレームは物体ハンドルを持つ。
class GourdRules final {
  public:
    static void Initialize(GourdComponent& gourd);
    static float GetSmoke(const GourdComponent& gourd);
    static float GetCapacity(const GourdComponent& gourd);
    static bool TryConsumeSmoke(GourdComponent& gourd, float amount);
    static bool IsAbsorbing(const GourdComponent& gourd);
    static HIKARI::GameProjectFloat3 GetMouthPosition(const GourdComponent& gourd);
    static HIKARI::GameProjectFloat3 GetAimDirection(const GourdComponent& gourd);
    // GPU の累計値が 120 フレーム後に失効した場合も、新しい累計の増分だけを収入にする。
    static void ReceiveRemovedSmoke(GourdComponent& gourd, double removedTotal);
};

} // namespace Project_TD2_2
