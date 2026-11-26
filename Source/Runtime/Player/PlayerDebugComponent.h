#pragma once

#include "Player.h"

namespace Project_TD2_2 {

inline constexpr std::string_view kPlayerDebugType = "TD2_2.PlayerDebug";
inline constexpr std::string_view kPlayerDebugSystem = "Project_TD2_2.PlayerDebugSystem";

// プレイヤーの検証用（新しいゲーム層の部品）。決まった tick に被弾・精力の消費・動作ロック・反動を起こし、状態を記録する。
struct PlayerDebug : HIKARI::Component {
    HIKARI_COMPONENT(PlayerDebug)

    // 正式シーンに検証用の被弾やログを持ち込まないため、既定では無効にする。
    FIELD(bool, enabled, false, "検証を有効にする", HIKARI::Group("検証専用（正式シーンには付けない）"))
    FIELD(int, damageAtTick, -1, "最初の被弾 tick（-1 は無効）", HIKARI::Range(-1, 7200))
    FIELD(float, damageAmount, 20.0f, "被弾ダメージ", HIKARI::Range(0.0f, 100000.0f))
    FIELD(PlayerDamageKind, damageKind, PlayerDamageKind::Stagger, "被弾の種類")
    FIELD(HIKARI::Vec3, damageDirection, (HIKARI::Vec3{0.0f, 0.0f, -1.0f}), "押される世界方向")
    FIELD(int, damageRepeatInterval, 0, "被弾の繰り返し間隔（F、0 は一度）", HIKARI::Range(0, 7200))
    FIELD(int, damageRepeatCount, 1, "被弾の試行回数", HIKARI::Range(1, 7200))
    FIELD(int, traceEveryFrames, 1, "ログ間隔（F、0 は出力なし）", HIKARI::Range(0, 7200))
    FIELD(int, consumeStaminaAtTick, -1, "体力消費 tick（-1 は無効）", HIKARI::Range(-1, 7200))
    FIELD(float, consumeStaminaAmount, 20.0f, "体力消費量", HIKARI::Range(0.0f, 100000.0f))
    FIELD(bool, consumeAllowInsufficient, true, "体力不足でも消費する")
    FIELD(int, lockAtTick, -1, "動作ロック開始 tick（-1 は無効）", HIKARI::Range(-1, 7200))
    FIELD(int, lockFrames, 36, "動作ロック時間（F）", HIKARI::Range(0, 7200))
    FIELD(int, recoilAtTick, -1, "反動開始 tick（-1 は無効）", HIKARI::Range(-1, 7200))
    FIELD(HIKARI::Vec3, recoilDirection, (HIKARI::Vec3{0.0f, 0.0f, -1.0f}), "反動の世界方向")
    FIELD(float, recoilDistance, 2.0f, "反動距離（m）", HIKARI::Range(0.0f, 100.0f))
    FIELD(int, recoilFrames, 8, "反動時間（F）", HIKARI::Range(1, 7200))

    uint64_t elapsedTicks = 0;
    uint64_t processedTick = 0;
    int damageAttempts = 0;
};

} // namespace Project_TD2_2
