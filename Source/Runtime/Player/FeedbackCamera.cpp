#include "FeedbackCamera.h"

#include "HIKARI/Project/ActionViews.h"
#include "../Common/ComponentLookup.h"
#include "Player.h"
#include "HIKARI/Extend/World.h"

namespace Project_TD2_2 {

uint64_t FeedbackCamera::Resolve(HIKARI::WorldView& world, HIKARI::Entity preferred) {
    if (const auto camera = HIKARI::Extend::ObjectOf(preferred); camera.IsValid())
        return camera.GetHandle().value;
    HIKARI::GameProjectCameraLens lens{};
    if (world.GetCamera().GetControlLens(lens) && lens.sourceCameraObjectHandle != 0)
        return lens.sourceCameraObjectHandle;
    // 追従カメラの制御視点には Lens の元物体が無い場合がある。既存の明示参照へ戻る。
    uint64_t result = 0;
    ComponentLookup::ForEachGame<Player>([&](HIKARI::ObjectView, Player& player) {
        if (result != 0)
            return;
        if (const auto camera = HIKARI::Extend::ObjectOf(player.camera); camera.IsValid())
            result = camera.GetHandle().value;
    });
    return result;
}

} // namespace Project_TD2_2
