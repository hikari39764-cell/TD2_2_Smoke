#include "DamageableComponent.h"

namespace Project_TD2_2 {

void DamageableComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (fields.Section("被弾と HP", true)) {
        fields.Float("maxHealth", "最大 HP", maxHealth, 1.0f, 1000000.0f);
        fields.Bool("invulnerable", "無敵（ダメージを受けない）", invulnerable);
        fields.Note("開始時は最大 HP。現在 HP と最後の被弾は DamageableRules の問い合わせで取得できます。");
    }
    if (fields.Section("検証")) {
        fields.Bool("traceEveryHit", "受けたダメージを毎回ログへ記録", traceEveryHit);
    }
}

} // namespace Project_TD2_2
