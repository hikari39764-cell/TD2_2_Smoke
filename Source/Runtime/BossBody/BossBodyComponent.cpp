#include "BossBodyComponent.h"

namespace Project_TD2_2 {

const BossBody& BossBodySettingsOf(const BossBody* look) {
    static const BossBody kDefaults{};
    return look != nullptr ? *look : kDefaults;
}

} // namespace Project_TD2_2
