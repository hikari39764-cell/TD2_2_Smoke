#pragma once

#include "HIKARI/Project/FrameContext.h"
#include "HIKARI/Project/WorldView.h"
#include "GourdComponent.h"

namespace Project_TD2_2 {

// 吸い込みの見た目と手応え（口の流体エフェクト、視野角、振動）。吸い込みの規則は GourdSystem が持つ。
class GourdAbsorbVisuals final {
  public:
    // 追従で口の位置と向きが確定した後（LateUpdate）に、毎フレーム呼ぶ。
    static void Update(HIKARI::WorldView& world, HIKARI::ObjectView object, GourdComponent& gourd,
                       const HIKARI::FrameContext& frame);
    static void Stop(HIKARI::WorldView& world, GourdComponent& gourd);
};

} // namespace Project_TD2_2
