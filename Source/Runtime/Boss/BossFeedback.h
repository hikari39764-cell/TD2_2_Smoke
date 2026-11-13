#pragma once

#include "HIKARI/Project/GameSystem.h"
#include "HIKARI/Project/WorldView.h"
#include "BossComponent.h"

namespace Project_TD2_2 {

// ボスの溜めが崩れた時・気絶した時の手応え（止め・カメラの揺れ・手柄の振動・本体の光り・合図のエフェクト）。
// 規則（固定 tick）が立てた要求を、描画フレームで見せるだけ。
class BossFeedback final {
  public:
    static void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                       const HIKARI::FrameContext& frame);
};

} // namespace Project_TD2_2
