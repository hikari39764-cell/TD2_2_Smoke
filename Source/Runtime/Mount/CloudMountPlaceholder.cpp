#include "CloudMountPlaceholder.h"

#include "CloudMountComponent.h"
#include "CloudMountRules.h"

namespace Project_TD2_2 {
namespace {

// 仮の雲の物体の形の契約：NimbusSky に置いた Procedural Mesh の球（直径 1 m）。大きさは物体の倍率で決める。
constexpr float kMeshDiameter = 1.0f;

} // namespace

void CloudMountPlaceholder::Update(HIKARI::WorldView& world, HIKARI::ObjectView body, CloudMountComponent& mount) {
    if (mount.cloudObjectId == 0)
        return;
    const auto cloud = world.FindSceneObject(mount.cloudObjectId);
    HIKARI::GameProjectTransform bodyTransform{};
    if (!cloud.IsValid() || !body.GetWorldTransform(bodyTransform)) {
        mount.cloudAccepted = false;
        return;
    }
    HIKARI::GameProjectTransform transform{};
    transform.position = bodyTransform.position;
    transform.position.y -=
        CloudMountRules::IsMounted(mount) ? mount.standOffset + mount.cloudBelowFeet : mount.cloudParkDepth;
    transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    transform.scale = {mount.cloudDiameter / kMeshDiameter, mount.cloudThickness / kMeshDiameter,
                       mount.cloudDiameter / kMeshDiameter};
    mount.cloudAccepted = cloud.SetWorldTransform(transform).Accepted();
}

} // namespace Project_TD2_2
