#include "BossPhaseShiftComponent.h"

namespace Project_TD2_2 {

void BossPhaseShiftComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    // 項目の名前・題・範囲は、ボスの部品にあった時のまま（場面は Tools/Verify/migrate_scene_components.py で部品を足した）。
    settings.Describe(fields);
}

} // namespace Project_TD2_2
