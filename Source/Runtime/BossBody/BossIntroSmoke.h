#pragma once

#include "HIKARI/Project/WorldView.h"
#include "BossBodyComponent.h"
#include "../Boss/BossComponent.h"
#include "../Boss/BossIntroComponent.h"

namespace Project_TD2_2 {

// 登場の黒い煙（c-e0c500）：コードからだけ煙を入れる流体の域（Assets/Vfx/BossIntroSmoke.hvfx。舞台に置く）へ、効果の拍
// （演出の資産の事件段）に合わせて煙を入れる・押す・消す。地を走って鎧を起こす煙、右の手甲へ伸びる煙、兜を持ち上げる
// 煙、兜が収まった瞬間の襟からの噴き、刀が抜けた瞬間の噴き上げ。演技が終わったら消して域を止める（戦いに残さない）。
namespace BossIntroSmoke {

// 流体への提出（PreRender。BossIntroEffects::Render から毎フレーム呼ぶ）。body は体の見た目の設定（見える刀の長さ）。
void Render(HIKARI::WorldView& world, HIKARI::ObjectView object, Boss& boss, const BossBody& body,
            BossIntro& intro, float seconds);
// 胴の煙の量の倍率（演技の間）：煙が鎧に届く拍（Smoke.Rise）まで 0、その段の間に 1 まで満ちる。拍が無ければ 1。
float BodyAmount(const BossIntro& intro);
// 体の根の足元（世界）と、地に立つ刀が地面に入る所（世界）。衝撃の輪を置く所に使う。
HIKARI::GameProjectFloat3 FeetWorld(const BossIntro& intro);
HIKARI::GameProjectFloat3 SwordEntryWorld(const BossIntro& intro);

} // namespace BossIntroSmoke
} // namespace Project_TD2_2
