#pragma once

#include "GourdComponent.h"

namespace Project_TD2_2 {

// 参照はクエリの中だけで使用し、跨フレームは物体ハンドルを持つ。
class GourdRules final {
  public:
      static void Initialize(Gourd& gourd);
      static float GetSmoke(const Gourd& gourd);
      static float GetCapacity(const Gourd& gourd);
      // 煙の量 ÷ 吸った霧の量（密度 × m³）。霧へ戻す煙の量を、同じ換算で霧の量へ直すために使う。
      static float GetSmokePerVolume(const Gourd& gourd);
      static bool IsFull(const Gourd& gourd);
      // 空き room（煙の量）があると見るか（kAbsorbRoomEpsilon より多い）。空きが少しでもあれば吸え、
      // 入りきらない煙は捨てる。
      static bool IsRoomLeft(float room);
      // まだ吸い込める空きがあるか（容量 − 煙の量 に IsRoomLeft）。
      static bool HasRoomToAbsorb(const Gourd& gourd);
      // 満杯で吸えない（absorbWhenFull が切で、空きが無い）。吸い込みの構えをしていても、
      // 霧・火・斬撃・ナイフを取り込まない。
      static bool IsBlocked(const Gourd& gourd);
      // 吸い込み中で、いま物を取り込める（場の霧・場の火を消す、飛ぶ斬撃・ナイフを捕まえる）。
      static bool CanTakeIn(const Gourd& gourd);
      // 飛ぶ斬撃・ナイフをあと取り込める煙の量（容量 − 煙の量。absorbWhenFull が入なら float の最大）。
      // 捕まえる側が、口へ引き込み中の分（入りきった時に足す煙）を引いて使う。
      static float GetCatchRoom(const Gourd& gourd);
      // 固定 tick ごとに、満杯で吸えないまま吸い込みを続けている秒数を進める（吸い込みの段階を決めた後に呼ぶ）。
      // 「入らない」になった tick（吸い込みを始めた時か、吸っている間に満杯になった時）に真を返し、記録する。
      static bool StepBlocked(Gourd& gourd, float dt);
      static bool TryConsumeSmoke(Gourd& gourd, float amount);
      // 吸い込みで霧以外の物（ボスの斬撃・ナイフ）を吸った時に煙を足す。容量を超える分は捨てる。足した量を返す。
      static float AddSmoke(Gourd& gourd, float amount);
      // ボスの斬撃・ナイフを count 個吸い取った。手応え（止め・振動・視野角・エフェクト）はヒョウタンが次の描画フレームで出す。
      // fire：その中に火の刃があった（呑み込みの色）。small：全部ナイフだった（呑み込みを小さく見せる）。
      static void NotifyCatch(Gourd& gourd, int count, bool fire, bool small);
      // ボスの斬撃・ナイフが count 個、吸い込みに捕まった（噛んだ。c-e0a9c3）。position は最後に捕まった所、fire は火の刃が
      // あったか。手応え（短い止め・鋭い振動・口から走る線）はヒョウタンが次の描画フレームで出す。
      static void NotifyBite(Gourd& gourd, int count, bool fire, HIKARI::GameProjectFloat3 position);
      // 引き込みの進み（0〜1。引き込んでいる間、毎 tick 呼ぶ。呼ばれない tick は StepSwallow が 0 に戻す）。
      static void NotePull(Gourd& gourd, float progress);
      // ボスの斬撃・ナイフを口へ引き込んでいる（毎 tick 呼ぶ）。呼ばれなくなってから swallowTailFrames の間まで、
      // 吸い込みをやめていても、口が飲み込み続ける（口の力と見た目）。
      static void NotifySwallowing(Gourd& gourd);
      static void StepSwallow(Gourd& gourd);
      static bool IsSwallowing(const Gourd& gourd);
      // 火弾（c-ad360d）：吸い込みで消した場の火の面積 (m²) を足す。ためた面積が fireShotTriggerArea に届くたびに、
      // 火弾の残りを fireShotDuration へ戻す（延ばすだけで、それより多くは積まない）。満杯で吸えない間は火を消せないので、
      // 呼ばれない（absorbWhenFull が入の時だけ、満杯でも呼ばれる）。
      static void AddAbsorbedFireArea(Gourd& gourd, float area);
      // 火の刃（火を残す斬撃・ナイフ）を count 個吸い取った。1 つでも火弾の残りを fireShotDuration へ戻す。
      static void NotifyFireCatch(Gourd& gourd, int count);
      // 固定 tick ごとに火弾の残りを減らす（ヒットストップで固定 tick が止まる間は減らない）。0 になった tick に終わりを記録する。
      static void StepFireShot(Gourd& gourd, float dt);
      // いま撃てば火弾か（残りが 0 より大きい）。弾は発射の瞬間にこれで決め、飛んでいる間は変えない。
      static bool IsFireShot(const Gourd& gourd);
      // 火弾の残り秒数（火弾でなければ 0）と、戻す長さ（秒）。
      static float GetFireShotSeconds(const Gourd& gourd);
      static float GetFireShotDuration(const Gourd& gourd);
      static bool IsAbsorbing(const Gourd& gourd);
      // 構えか吸い込みの最中。この間は砲撃と回復を始めない（後隙は他の行動で打ち切れるので含まない）。
      static bool IsAbsorbCommitted(const Gourd& gourd);
      // 吸い込みを押し続けているか、先行入力が残っている。
      static bool WantsAbsorb(const Gourd& gourd);
      // 吸い込みの後隙を打ち切る（砲撃を後隙から始める時）。後隙でなければ何もしない。
      static void CancelAbsorbRecovery(Gourd& gourd);
      static HIKARI::GameProjectFloat3 GetMouthPosition(const Gourd& gourd);
      static HIKARI::GameProjectFloat3 GetAimDirection(const Gourd& gourd);
      // 射撃側の PreFixedUpdate から呼び、同じ tick の GourdSystem が一歩目を進める。
      // direction は世界方向。反動の距離と F 数は射撃コンポーネント側で調整する。
      static void AddRecoil(Gourd& gourd, HIKARI::GameProjectFloat3 direction, float distance, int frames);
      // 射撃側の LateUpdate から毎フレーム要求する。追従処理が消費し、次フレームへ残さない。
      // 撃っている間、描画のヒョウタンを撃つ向きへ向ける（見た目だけ。規則の口・向きは変えない）。
      static void SetAimOverride(Gourd& gourd, HIKARI::GameProjectFloat3 direction);
      // 描画のフレームで読み戻した霧の増えを、次の固定 tick の始めに煙の量へ入れる（c-164cab。容量で止める）。
      static void ApplyPendingSmoke(Gourd& gourd);
      // GPU の累計値が 120 フレーム後に失効した場合も、新しい累計の増分だけを収入にする。
      static void ReceiveRemovedSmoke(Gourd& gourd, double removedTotal);
};

} // namespace Project_TD2_2
