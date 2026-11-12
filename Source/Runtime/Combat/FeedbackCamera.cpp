#include "FeedbackCamera.h"

#include "HIKARI/Project/ActionViews.h"
#include "../Player/PlayerComponent.h"

namespace Project_TD2_2 {

uint64_t FeedbackCamera::Resolve(HIKARI::WorldView& world, uint64_t preferredSceneObjectId) {
    if (preferredSceneObjectId != 0) {
        const auto camera = world.FindSceneObject(preferredSceneObjectId);
        if (camera.IsValid())
            return camera.GetHandle().value;
    }
    HIKARI::GameProjectCameraLens lens{};
    if (world.GetCamera().GetControlLens(lens) && lens.sourceCameraObjectHandle != 0)
        return lens.sourceCameraObjectHandle;
    // 追従カメラの制御視点には Lens の元物体が無い場合がある。既存の明示参照へ戻る。
    uint64_t result = 0;
    world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                            [&](HIKARI::ObjectView, PlayerComponent& player) {
                                                if (result != 0 || player.cameraId == 0)
                                                    return;
                                                const auto camera = world.FindSceneObject(player.cameraId);
                                                if (camera.IsValid())
                                                    result = camera.GetHandle().value;
                                            });
    return result;
}

} // namespace Project_TD2_2
