#pragma once

#include "PlayerComponent.h"

namespace Project_TD2_2 {

// 呼び出し中に得た PlayerComponent だけを渡す。参照を次フレームへ保持しない。
class PlayerRules final {
public:
    static void Initialize(PlayerComponent& player);
    static bool IsAlive(const PlayerComponent& player);
    static bool CanStartAction(const PlayerComponent& player);
    static bool IsInvulnerable(const PlayerComponent& player);
    // direction は攻撃が押す世界方向。無敵・死亡・無効な量では false。
    static bool ApplyDamage(PlayerComponent& player, float amount, HIKARI::GameProjectFloat3 direction,
                            PlayerDamageKind kind);
    // 燃える地面のような、続けて受けるダメージ。HP だけを減らし、のけぞり・ヒットストップ・向きの変化・入力の取り消しは
    // 起こさない（被弾ではない）。無敵・死亡・無効な量で false。HP が 0 になれば ApplyDamage と同じく死亡にする。
    static bool ApplyDamageOverTime(PlayerComponent& player, float amount);
    // allowInsufficient は残量が正なら不足分を認める。残量 0 では射撃できない。
    static bool TryConsumeStamina(PlayerComponent& player, float amount, bool allowInsufficient = false);
    static void LockActions(PlayerComponent& player, int frames);
    // 射撃の後隙を吸い込みで取り消すときなど、残っている動作ロックをその場で解く。
    static void ReleaseActionLock(PlayerComponent& player);
    // ここから TryEndRollForAction の前までは、外部の動作が出す 1 固定 tick ぶんの要求（PlayerExternalRequests）。
    // 続ける間は毎 tick 要求し直す。
    // その固定 tick の歩行倍率。
    static void SetMovementSpeedScale(PlayerComponent& player, float scale);
    // その固定 tick だけ、向きを入力の方向へ毎秒この角度まで回す（ロックオン中でも）。0 は通常の向き。
    static void SetTurnOverride(PlayerComponent& player, float degreesPerSecond);
    // 次の固定 tick の開始まで、回復の開始を止める（ローリングは止めない）。
    static void BlockHeal(PlayerComponent& player);
    // その固定 tick だけ、動作ロック中でも scale 倍で歩けるようにする。
    static void AllowWalkDuringAction(PlayerComponent& player, float scale);
    // 次の固定 tick の開始まで、押されたローリングを消さずに待たせる（まだ打ち切れない動作の間）。
    static void HoldRollInput(PlayerComponent& player);
    // 次の固定 tick の開始で、動作ロック中でもローリングを出せる（動作を打ち切る）。
    static void AllowRollCancel(PlayerComponent& player);
    // ローリングの移動が終わった後（rollActionCancelFrame 以降）なら、残りを打ち切って通常に戻し true を返す。
    // 砲撃・吸い込みを始めたい側が、始める前に呼ぶ。
    static bool TryEndRollForAction(PlayerComponent& player);
    // その固定 tick だけ、向きを direction（水平）へ毎秒この角度まで回す。ロックオンや入力の向きより優先する。
    // PlayerSystem が使ったら消すので、続ける間は毎 tick 要求する。
    static void SetAimTurn(PlayerComponent& player, HIKARI::GameProjectFloat3 direction, float degreesPerSecond);
    // 向きをその場で direction（水平）にそろえる。発射の瞬間など。
    static void FaceDirection(PlayerComponent& player, HIKARI::GameProjectFloat3 direction);
    // distance m を frames F で動かす。進んだ距離 = distance × (1 − (1 − 経過/frames)^easePower)。
    // easePower 1 は等速、2 は初め速くあとゆっくり。新しい要求は残っている反動を置き換える。
    static void AddRecoil(PlayerComponent& player, HIKARI::GameProjectFloat3 direction, float distance, int frames,
                          float easePower = 1.0f);
    // 0〜frames の経過 F で、ease の式に従って進んだ距離の割合（0〜1）。回避と反動で共用する。
    static float EaseOutFraction(int elapsedFrames, int frames, float easePower);
    static HIKARI::GameProjectFloat3 GetFacing(const PlayerComponent& player);
    static HIKARI::GameProjectObjectHandle GetLockTarget(const PlayerComponent& player);
    // ロックオンを押した時に狙う相手（シーン物体 id）。ロックしていない時の照準補助も同じ相手を見る。
    static uint64_t GetLockCandidateId(const PlayerComponent& player);
    // 照準補助の候補（ボス本体＋その tick に出ている分身）を入れ替える。候補を知っているボスの側が毎 tick 呼ぶ。
    static void SetLockCandidates(PlayerComponent& player, const HIKARI::GameProjectObjectHandle* handles, size_t count);
    static size_t GetLockCandidateCount(const PlayerComponent& player);
    static HIKARI::GameProjectObjectHandle GetLockCandidate(const PlayerComponent& player, size_t index);
    // ロックオン中なら、ロック対象を target に変える（ボスと分身の切り替え）。ロックしていなければ何もしない。
    static void SetLockTarget(PlayerComponent& player, HIKARI::GameProjectObjectHandle target);
    // 「ロック対象を変える」が押されていたら true を返して消す（対象の候補を知っている側が呼ぶ）。
    static bool TakeLockSwitch(PlayerComponent& player);
    // ロック中のカメラが見る点のずれ（世界 m）。外部が入れた値を次に入れ直すまで使う。
    static void SetLockPivotOffset(PlayerComponent& player, HIKARI::GameProjectFloat3 offset);
    static HIKARI::GameProjectFloat3 GetLockPivotOffset(const PlayerComponent& player);
    static float GetHealth(const PlayerComponent& player);
    static float GetStamina(const PlayerComponent& player);
    static int GetHealCharges(const PlayerComponent& player);
    static HIKARI::GameProjectFloat3 FlatNormalize(HIKARI::GameProjectFloat3 value,
                                                   HIKARI::GameProjectFloat3 fallback = {0.0f, 0.0f, 1.0f});
    static void EnterState(PlayerComponent& player, PlayerState state);

private:
    // 死亡へ移す（ロックオンも外す）。被弾でも火でも同じ死に方にする。
    static void EnterDeath(PlayerComponent& player);
};

} // namespace Project_TD2_2
