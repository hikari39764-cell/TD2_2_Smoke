#pragma once

#include "GourdComponent.h"

namespace Project_TD2_2 {

// 参照はクエリの中だけで使用し、跨フレームは物体ハンドルを持つ。
class GourdRules final {
  public:
    static void Initialize(GourdComponent& gourd);
    static float GetSmoke(const GourdComponent& gourd);
    static float GetCapacity(const GourdComponent& gourd);
    // 煙の量 ÷ 吸った霧の量（密度 × m³）。霧へ戻す煙の量を、同じ換算で霧の量へ直すために使う。
    static float GetSmokePerVolume(const GourdComponent& gourd);
    static bool IsFull(const GourdComponent& gourd);
    static bool TryConsumeSmoke(GourdComponent& gourd, float amount);
    // 吸い込みで霧以外の物（ボスの斬撃・ナイフ）を吸った時に煙を足す。容量を超える分は捨てる。足した量を返す。
    static float AddSmoke(GourdComponent& gourd, float amount);
    static bool IsAbsorbing(const GourdComponent& gourd);
    // 構え・吸い込み・後隙のどれかの最中。この間は砲撃を始めない。
    static bool IsAbsorbBusy(const GourdComponent& gourd);
    static HIKARI::GameProjectFloat3 GetMouthPosition(const GourdComponent& gourd);
    static HIKARI::GameProjectFloat3 GetAimDirection(const GourdComponent& gourd);
    // 射撃側の PreFixedUpdate から呼び、同じ tick の GourdSystem が一歩目を進める。
    // direction は世界方向。反動の距離と F 数は射撃コンポーネント側で調整する。
    static void AddRecoil(GourdComponent& gourd, HIKARI::GameProjectFloat3 direction, float distance, int frames);
    // 射撃側の LateUpdate から毎フレーム要求する。追従処理が消費し、次フレームへ残さない。
    static void SetAimOverride(GourdComponent& gourd, HIKARI::GameProjectFloat3 direction);
    // GPU の累計値が 120 フレーム後に失効した場合も、新しい累計の増分だけを収入にする。
    static void ReceiveRemovedSmoke(GourdComponent& gourd, double removedTotal);
};

} // namespace Project_TD2_2
