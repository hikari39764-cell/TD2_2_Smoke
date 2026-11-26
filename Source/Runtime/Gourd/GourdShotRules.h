#pragma once

#include "GourdShotComponent.h"

namespace Project_TD2_2 {

class GourdShotRules final {
public:
    static float GetFalloffFraction(const GourdShot& shot, float distance);
    static float GetDamageAtDistance(const GourdShot& shot, float distance);
    // 弾がいまの距離で当たった時の威力（距離での減り × 火弾なら fireShotDamageScale）。
    static float GetProjectileDamage(const GourdShot& shot, const GourdProjectile& projectile);
    static float GetSpeedAtDistance(const GourdShot& shot, float distance);
    static float GetRadiusAtDistance(const GourdShot& shot, float distance);
    static float GetDensityAtDistance(const GourdShot& shot, float distance);
    // chainShots 発目を撃った後、次を撃つまでの F。押し続けるほど短くなる。
    static int GetRepeatFrames(const GourdShot& shot, int chainShots);
    static bool IsFiring(const GourdShot& shot);
    static uint32_t GetActiveProjectileCount(const GourdShot& shot);
    // 以下は読むだけ（ボスがプレイヤーの射撃を見る。c-59bd08）。
    // 撃つ構えの間（押してから 1 発目を撃つまで）。
    static bool IsWindingUp(const GourdShot& shot);
    // 撃った後、押し続けて（先行入力を含む）次の弾を待っている。
    static bool IsChaining(const GourdShot& shot);
    // 撃った弾の累計（構えから撃った弾も連射の弾も 1 発ずつ。粒の数ではない）。
    static uint64_t GetShotsFired(const GourdShot& shot);
    // 構えの間と押し続けている間の狙い：撃つ所（ヒョウタンの位置）と撃つ向き（単位ベクトル）。
    static HIKARI::GameProjectFloat3 GetAimOrigin(const GourdShot& shot);
    static HIKARI::GameProjectFloat3 GetAimDirection(const GourdShot& shot);
    // 1 発に使う煙の量。
    static float GetSmokeCost(const GourdShot& shot);
    // 炎の流体域の枠を count 個譲る（ほかの機能の斬撃のエフェクトが使う。描画のフレームごとに入れ直す）。
    static void ReserveFluidDomains(GourdShot& shot, size_t count);
};

} // namespace Project_TD2_2
