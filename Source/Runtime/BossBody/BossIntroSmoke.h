#pragma once

#include "HIKARI/Project/WorldView.h"
#include "../Boss/BossComponent.h"

namespace Project_TD2_2 {

// 登場の黒い煙（c-e0c500）：コードからだけ煙を入れる流体の域（Assets/Vfx/BossIntroSmoke.hvfx。舞台に置く）へ、効果の拍
// （演出の資産の事件段）に合わせて煙を入れる・押す・消す。地を走って鎧を起こす煙、右の手甲へ伸びる煙、兜を持ち上げる
// 煙、兜が収まった瞬間の襟からの噴き、刀が抜けた瞬間の噴き上げ。演技が終わったら消して域を止める（戦いに残さない）。
namespace BossIntroSmoke {

// 流体への提出（PreRender。BossIntroEffects::Render から毎フレーム呼ぶ）。
void Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds);
// 胴の煙の量の倍率（演技の間）：煙が鎧に届く拍（Smoke.Rise）まで 0、その段の間に 1 まで満ちる。拍が無ければ 1。
float BodyAmount(const BossComponent& boss);
// 体の根の足元（世界）と、地に立つ刀が地面に入る所（世界）。衝撃の輪を置く所に使う。
HIKARI::GameProjectFloat3 FeetWorld(const BossComponent& boss);
HIKARI::GameProjectFloat3 SwordEntryWorld(const BossComponent& boss);

} // namespace BossIntroSmoke
} // namespace Project_TD2_2
