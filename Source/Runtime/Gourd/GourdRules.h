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
    // まだ吸い込める空きがあるか（空きが kAbsorbRoomEpsilon より多い）。飛ぶ斬撃を吸う時と、火を吸う時の満杯の判定。
    static bool HasRoomToAbsorb(const GourdComponent& gourd);
    static bool TryConsumeSmoke(GourdComponent& gourd, float amount);
    // 吸い込みで霧以外の物（ボスの斬撃・ナイフ）を吸った時に煙を足す。容量を超える分は捨てる。足した量を返す。
    static float AddSmoke(GourdComponent& gourd, float amount);
    // ボスの斬撃・ナイフを count 個吸い取った。手応え（止め・振動・視野角・エフェクト）はヒョウタンが次の描画フレームで出す。
    static void NotifyCatch(GourdComponent& gourd, int count);
    // 火弾（c-ad360d）：吸い込みで消した場の火の面積 (m²) を足す。ためた面積が fireShotTriggerArea に届くたびに、
    // 火弾の残りを fireShotDuration へ戻す（延ばすだけで、それより多くは積まない）。満杯でも呼ばれれば効く。
    static void AddAbsorbedFireArea(GourdComponent& gourd, float area);
    // 火の刃（火を残す斬撃・ナイフ）を count 個吸い取った。1 つでも火弾の残りを fireShotDuration へ戻す。
    static void NotifyFireCatch(GourdComponent& gourd, int count);
    // 固定 tick ごとに火弾の残りを減らす（ヒットストップで固定 tick が止まる間は減らない）。0 になった tick に終わりを記録する。
    static void StepFireShot(GourdComponent& gourd, float dt);
    // いま撃てば火弾か（残りが 0 より大きい）。弾は発射の瞬間にこれで決め、飛んでいる間は変えない。
    static bool IsFireShot(const GourdComponent& gourd);
    // 火弾の残り秒数（火弾でなければ 0）と、戻す長さ（秒）。
    static float GetFireShotSeconds(const GourdComponent& gourd);
    static float GetFireShotDuration(const GourdComponent& gourd);
    static bool IsAbsorbing(const GourdComponent& gourd);
    // 構えか吸い込みの最中。この間は砲撃と回復を始めない（後隙は他の行動で打ち切れるので含まない）。
    static bool IsAbsorbCommitted(const GourdComponent& gourd);
    // 吸い込みを押し続けているか、先行入力が残っている。
    static bool WantsAbsorb(const GourdComponent& gourd);
    // 吸い込みの後隙を打ち切る（砲撃を後隙から始める時）。後隙でなければ何もしない。
    static void CancelAbsorbRecovery(GourdComponent& gourd);
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
