#include "BossBodyComponent.h"

namespace Project_TD2_2 {

void BossBodyComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    // 項目の名前・題・範囲は、ボスの部品にあった時のまま（場面は Tools/Verify/migrate_scene_components.py で移した）。
    settings.Describe(fields);
}

const BossBodySettings& BossBodySettingsOf(const BossBodyComponent* look) {
    static const BossBodySettings kDefaults{};
    return look != nullptr ? look->settings : kDefaults;
}

} // namespace Project_TD2_2
