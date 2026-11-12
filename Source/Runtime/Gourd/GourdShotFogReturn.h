#pragma once

#include "GourdShotComponent.h"

namespace Project_TD2_2 {

// 撃った煙を場の霧へ戻す。吸う → 撃つ → 地面へ戻る、の循環を作る。
class GourdShotFogReturn final {
public:
    // from の真下の地面を探す。届けば ground に地面の位置を入れて true。
    static bool FindGround(HIKARI::WorldView& world, HIKARI::GameProjectFloat3 from, float reach,
                           uint64_t ignoredObject, HIKARI::GameProjectFloat3& ground);
    // 炎の域（砲口・命中）が出た場所を覚える。戻す煙はこの近くを避けて待つ。
    static void MarkFire(GourdShotComponent& shot, HIKARI::GameProjectFloat3 position, int frames);
    // 弾が終わった時（命中・射程切れ）に、通り道と終点の地面へ戻す煙を予約する。
    static void Queue(HIKARI::WorldView& world, GourdShotComponent& shot, const GourdProjectile& projectile,
                      uint64_t ignoredObject);
    // 固定 tick：待ち時間と、この tick に戻す量を進める。
    static void Step(GourdShotComponent& shot);
    // 描画フレーム：戻す煙を場の霧へ注入する。smokePerVolume はヒョウタンの換算（煙の量 ÷ 密度 × m³）。
    static void Render(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdShotComponent& shot,
                       uint32_t& fluidCount, float smokePerVolume);
    static void Clear(GourdShotComponent& shot);
};

} // namespace Project_TD2_2
