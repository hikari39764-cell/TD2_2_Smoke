#pragma once

#include "HIKARI/Project/GameComponent.h"

namespace Project_TD2_2 {

class CloudMountComponent;

// 流体の雲（後のカード）ができるまでの仮の見た目。シーンに置いた当たりの無い球の物体を、乗っている間は足元へ、
// 乗っていない間は地面の下へ動かす（物体の表示を切り替える公開 API が無いので、しまう）。値は CloudMount の「仮の雲」。
namespace CloudMountPlaceholder {

// 描画フレームごとに 1 回呼ぶ。body は CloudMount を付けた物体（プレイヤー）。
void Update(HIKARI::WorldView& world, HIKARI::ObjectView body, CloudMountComponent& mount);

} // namespace CloudMountPlaceholder

} // namespace Project_TD2_2
