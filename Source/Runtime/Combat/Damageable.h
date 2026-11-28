#pragma once

#include <cstdint>

#include "HIKARI/Game.h"

namespace Project_TD2_2 {

// 被弾と HP（ゲーム層の部品）。HP を持ち、受けた時の手応え（止め・カメラの一跳ね・振動・体の光り・色ずれ）も出す。
// 当てる側は Apply を呼ぶ（受け付けたかが返る）。敵の種類や被弾後の動作には依存しない。
// 手応えは次の Update で出す：まだ古い System（ボス・ヒョウタン砲撃）から当てられ、その回呼の中ではサービスを
// 呼べないため（当てる側が移れば Apply の中で直接出せる）。
struct Damageable : HIKARI::Component {
    HIKARI_COMPONENT(Damageable, "被弾と HP")
    FIELD(float, maxHealth, 1000.0f, "最大 HP", HIKARI::Range(1, 1000000), HIKARI::Group("被弾と HP"))
    FIELD(bool, invulnerable, false, "無敵（ダメージを受けない）")
    // 命中の止め。連射（8F ごと）でも止めが積み上がらないよう、止めている間の次の命中では延ばさない（c-d330c0）。
    FIELD(int, hitStopFrames, 2, "命中の停止時間 (F、0 は無効)", HIKARI::Range(0, 30), HIKARI::Group("被弾の手応え"))
    FIELD(float, hitStopScale, 0.05f, "停止中の時間倍率（1 は停止なし）", HIKARI::Range(0, 1))
    // 命中のカメラの手応え。連射が当たり続けても画面が震え続けない強さにする（当たった重さは体の光と振動で出す）。
    FIELD(int, hitCameraFrames, 6, "命中のカメラ揺れ時間 (F、0 は無効)", HIKARI::Range(0, 120))
    FIELD(float, hitCameraAmplitude, 0.03f, "命中のカメラ揺れ幅 (m、0 は無効)", HIKARI::Range(0, 1))
    FIELD(float, hitCameraFrequency, 24.0f, "命中のカメラ揺れ周波数 (Hz)", HIKARI::Range(0, 60))
    FIELD(float, hitCameraKickDistance, 0.05f, "命中でカメラが攻撃の向きへ押される幅 (m、0 は無効)",
          HIKARI::Range(0, 1))
    FIELD(float, hitCameraFovDegrees, 0.4f, "命中で広がる視野角 (度、0 は無効)", HIKARI::Range(0, 20))
    // この量以上の弾が当たった時を揺れの最大とする。距離で威力が減った弾は同じ割合で弱く揺らす（下限あり）。
    FIELD(float, hitFeedbackFullDamage, 40.0f, "カメラの揺れが最大になる弾の威力", HIKARI::Range(1, 1000))
    FIELD(float, hitFeedbackMinScale, 0.3f, "威力が小さい弾の、カメラの揺れの下限（割合）", HIKARI::Range(0, 1))
    FIELD(int, hitRumbleFrames, 9, "命中のコントローラー振動時間 (F、0 は無効)", HIKARI::Range(0, 120))
    FIELD(float, hitRumbleLow, 0.85f, "命中の振動：重いモーター (0〜1)", HIKARI::Range(0, 1))
    FIELD(float, hitRumbleHigh, 0.6f, "命中の振動：軽いモーター (0〜1)", HIKARI::Range(0, 1))
    FIELD(int, hitFlashFrames, 8, "被弾の発光時間 (F、0 は無効)", HIKARI::Range(0, 120))
    FIELD(float, hitFlashStrength, 3.0f, "被弾の発光強度（0 は無効）", HIKARI::Range(0, 16))
    FIELD(HIKARI::Color, hitFlashColor, (HIKARI::Color{0.45f, 0.92f, 1.0f, 1.0f}), "被弾の発光色")
    FIELD(int, hitChromaFrames, 4, "命中の瞬間の色ずれの時間 (F、0 は無効)", HIKARI::Range(0, 60))
    FIELD(float, hitChromaPixels, 1.5f, "命中の瞬間の色ずれの幅 (画素)", HIKARI::Range(0, 24))

    STATE(float, health, 0.0f)
    // 受けるダメージの倍率（ボスの気絶中など、持ち主のルールが毎 tick 決める）。
    STATE(float, incomingScale, 1.0f)
    // 受け付けた命中の数と、倍率をかける前のダメージの累計。気絶値のように「当てた量」に比例する規則が差分を読む
    // （STATE は uint64_t・double を持てないので、普通のメンバー）。
    uint64_t hitCount = 0;
    double baseDamageTotal = 0.0;

    void OnStart() override;
    void Update(float dt) override;

    // 当てる。無敵・死亡・非正の量は拒否し、受け付けた時だけ HP と記録を変える。direction は攻撃が押す世界の向き。
    bool Apply(float amount, HIKARI::Vec3 direction);
    // 直前に受け付けた命中の手応えだけを強める（火弾。Apply の成功の直後に呼ぶ）。pushScale はカメラの押しと視野角に掛け、
    // stopFrames は hitStopFrames より長い時だけ効く。次の命中には持ち越さない。
    void Boost(float pushScale, int stopFrames);
    // 光りだけを frames F 出す（命中ではない場面。ボスの崩し・気絶）。
    void RequestFlash(int frames);
    // 最大 HP を替える（HP は新しい最大を超えないように切る）／替えて満たす（ボスの 2 本目の条）。
    void SetMaxHealth(float value);
    void Refill(float value);
    bool IsAlive() const { return Health() > 0.0f; }
    float Health() const;
    float LastDamage() const { return lastDamage_; }

private:
    void EnsureStarted();
    void PlayHitFeedback();

    bool started_ = false;
    float lastDamage_ = 0.0f;
    float lastRawDamage_ = 0.0f;
    HIKARI::Vec3 lastDirection_{};
    uint64_t feedbackHitCount_ = 0;
    float pushScale_ = 1.0f;
    int pendingStopFrames_ = 0;
    int pendingFlashFrames_ = 0;
    float stopLeft_ = 0.0f; // 前の止めの残り（実時間）。残っている間の命中では止めを延ばさない
};

} // namespace Project_TD2_2
