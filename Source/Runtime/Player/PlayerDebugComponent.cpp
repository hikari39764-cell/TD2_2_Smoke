#include "PlayerDebugComponent.h"

#include "HIKARI/Project/PropertyFields.h"

namespace Project_TD2_2 {

void PlayerDebugComponent::DescribeProperties(HIKARI::PropertyFields& fields) {
    if (!fields.Section("検証専用（正式シーンには付けない）", true)) {
        return;
    }
    static constexpr HIKARI::GameProjectEnumName<PlayerDamageKind> damageKinds[] = {
        {PlayerDamageKind::Stagger, "Stagger"}, {PlayerDamageKind::Knockback, "Knockback"}};
    fields.Bool("enabled", "検証を有効にする", enabled);
    fields.Int("damageAtTick", "最初の被弾 tick（-1 は無効）", damageAtTick, -1, 7200);
    fields.Float("damageAmount", "被弾ダメージ", damageAmount, 0.0f, 100000.0f, 1.0f);
    fields.Choice("damageKind", "被弾の種類", damageKind, damageKinds);
    fields.Float3("damageDirection", "押される世界方向", damageDirection);
    fields.Int("damageRepeatInterval", "被弾の繰り返し間隔（F、0 は一度）", damageRepeatInterval, 0, 7200);
    fields.Int("damageRepeatCount", "被弾の試行回数", damageRepeatCount, 1, 7200);
    fields.Int("traceEveryFrames", "ログ間隔（F、0 は出力なし）", traceEveryFrames, 0, 7200);
    fields.Int("consumeStaminaAtTick", "体力消費 tick（-1 は無効）", consumeStaminaAtTick, -1, 7200);
    fields.Float("consumeStaminaAmount", "体力消費量", consumeStaminaAmount, 0.0f, 100000.0f, 1.0f);
    fields.Bool("consumeAllowInsufficient", "体力不足でも消費する", consumeAllowInsufficient);
    fields.Int("lockAtTick", "動作ロック開始 tick（-1 は無効）", lockAtTick, -1, 7200);
    fields.Int("lockFrames", "動作ロック時間（F）", lockFrames, 0, 7200);
    fields.Int("recoilAtTick", "反動開始 tick（-1 は無効）", recoilAtTick, -1, 7200);
    fields.Float3("recoilDirection", "反動の世界方向", recoilDirection);
    fields.Float("recoilDistance", "反動距離（m）", recoilDistance, 0.0f, 100.0f, 0.05f);
    fields.Int("recoilFrames", "反動時間（F）", recoilFrames, 1, 7200);
}

} // namespace Project_TD2_2
