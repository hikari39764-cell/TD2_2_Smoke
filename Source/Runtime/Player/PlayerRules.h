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
    // allowInsufficient は残量が正なら不足分を認める。残量 0 では射撃できない。
    static bool TryConsumeStamina(PlayerComponent& player, float amount, bool allowInsufficient = false);
    static void LockActions(PlayerComponent& player, int frames);
    // 射撃の後隙を吸い込みで取り消すときなど、残っている動作ロックをその場で解く。
    static void ReleaseActionLock(PlayerComponent& player);
    // その固定 tick の歩行倍率。PlayerSystem が毎 tick 1 に戻してから外部が要求する。
    static void SetMovementSpeedScale(PlayerComponent& player, float scale);
    static float GetMovementSpeedScale(const PlayerComponent& player);
    // その固定 tick だけ、向きを入力の方向へ毎秒この角度まで回す（ロックオン中でも）。0 は通常の向き。
    // PlayerSystem が毎 tick 0 に戻してから外部が要求する。
    static void SetTurnOverride(PlayerComponent& player, float degreesPerSecond);
    // 次の固定 tick の開始まで、ローリングと回復の開始を止める。続ける間は毎 tick 要求し直す。
    static void BlockActions(PlayerComponent& player);
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
    static float GetHealth(const PlayerComponent& player);
    static float GetStamina(const PlayerComponent& player);
    static int GetHealCharges(const PlayerComponent& player);
    static HIKARI::GameProjectFloat3 FlatNormalize(HIKARI::GameProjectFloat3 value,
                                                   HIKARI::GameProjectFloat3 fallback = {0.0f, 0.0f, 1.0f});
    static void EnterState(PlayerComponent& player, PlayerState state);
};

} // namespace Project_TD2_2
