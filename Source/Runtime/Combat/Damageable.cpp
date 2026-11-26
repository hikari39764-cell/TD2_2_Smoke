#include "Damageable.h"

#include <algorithm>
#include <cmath>
#include <vector>

namespace Project_TD2_2 {
namespace {

using namespace HIKARI;

constexpr float kFramesPerSecond = 60.0f;

// Play 中に始まった Damageable（古い System から物体で引くための表。ボスとヒョウタン砲撃が移れば消す）。
std::vector<Damageable*>& Started() {
    static std::vector<Damageable*> started;
    return started;
}

float Seconds(int frames) { return static_cast<float>(std::max(frames, 0)) / kFramesPerSecond; }

} // namespace

void Damageable::OnStart() {
    EnsureStarted();
    Started().push_back(this);
}

void Damageable::OnDestroy() { std::erase(Started(), this); }

Damageable::~Damageable() { std::erase(Started(), this); }

Damageable* Damageable::Of(uint64_t objectHandle) {
    for (Damageable* damageable : Started())
        if (damageable->Self().Raw() == objectHandle)
            return damageable;
    return nullptr;
}

void Damageable::EnsureStarted() {
    if (started_)
        return;
    started_ = true;
    health = std::max(0.0f, maxHealth);
}

float Damageable::Health() const {
    const float max = std::max(0.0f, maxHealth);
    return started_ ? std::clamp(health, 0.0f, max) : max;
}

bool Damageable::Apply(float amount, Vec3 direction) {
    EnsureStarted();
    if (invulnerable || !IsAlive() || !std::isfinite(amount) || amount <= 0.0f || !std::isfinite(direction.x) ||
        !std::isfinite(direction.y) || !std::isfinite(direction.z))
        return false;
    const float scale = std::isfinite(incomingScale) ? std::max(0.0f, incomingScale) : 1.0f;
    health = std::max(0.0f, Health() - amount * scale);
    lastDamage_ = amount * scale;
    lastRawDamage_ = amount;
    baseDamageTotal += static_cast<double>(amount);
    lastDirection_ = direction.Normalized();
    ++hitCount;
    // 手応えの強めは命中ごとに頼み直してもらう（前の命中の分を持ち越さない）。
    pushScale_ = 1.0f;
    pendingStopFrames_ = 0;
    return true;
}

void Damageable::Boost(float pushScale, int stopFrames) {
    pushScale_ = std::isfinite(pushScale) ? std::max(0.0f, pushScale) : 1.0f;
    pendingStopFrames_ = std::max(0, stopFrames);
}

void Damageable::RequestFlash(int frames) { pendingFlashFrames_ = std::max(pendingFlashFrames_, frames); }

void Damageable::SetMaxHealth(float value) {
    maxHealth = value;
    EnsureStarted();
    // 先に当たっていた時も、新しい最大を超えないようにする。
    health = std::min(health, maxHealth);
}

void Damageable::Refill(float value) {
    EnsureStarted();
    maxHealth = value;
    health = maxHealth;
}

void Damageable::Update(float) {
    // 止めの残りは実時間で数える（止めている間もゲーム時間はほとんど進まない）。
    stopLeft_ = std::max(0.0f, stopLeft_ - Time::Unscaled());
    if (feedbackHitCount_ != hitCount) {
        feedbackHitCount_ = hitCount;
        PlayHitFeedback();
    }
    if (pendingFlashFrames_ > 0) {
        if (hitFlashStrength > 0.0f)
            Self().Flash(hitFlashColor, hitFlashStrength, Seconds(pendingFlashFrames_));
        pendingFlashFrames_ = 0;
    }
}

void Damageable::PlayHitFeedback() {
    // 前の止めがまだ続いている間の命中では、止めを延ばさず作り直さない（連射で止めが積み上がらない）。
    if (stopLeft_ <= 0.0f && hitStopScale < 1.0f) {
        const int frames = std::max(hitStopFrames, pendingStopFrames_);
        if (frames > 0) {
            Time::HitStop(Seconds(frames), hitStopScale);
            stopLeft_ = Seconds(frames);
        }
    }
    // 威力が減った弾（距離で減衰した弾）は、カメラも同じ割合で弱く揺らす。押しと視野角は、攻撃側が頼んだ強め（火弾）の
    // 分だけ大きくする。揺れ幅は変えない（連射で震え続けない）。
    const float scale = std::clamp(lastRawDamage_ / std::max(1.0f, hitFeedbackFullDamage),
                                   std::clamp(hitFeedbackMinScale, 0.0f, 1.0f), 1.0f);
    const float cameraSeconds = Seconds(hitCameraFrames);
    if (hitCameraAmplitude > 0.0f)
        Camera::Shake(hitCameraAmplitude * scale, cameraSeconds, hitCameraFrequency);
    if (hitCameraKickDistance > 0.0f)
        Camera::Kick(lastDirection_, hitCameraKickDistance * scale * pushScale_, cameraSeconds);
    if (hitCameraFovDegrees > 0.0f)
        Camera::Fov(hitCameraFovDegrees * scale * pushScale_, cameraSeconds);
    if (hitChromaPixels > 0.0f)
        Camera::PostPulse("chroma.pixels", hitChromaPixels, Seconds(hitChromaFrames));
    if (hitRumbleFrames > 0 && (hitRumbleLow > 0.0f || hitRumbleHigh > 0.0f))
        Input::Rumble(hitRumbleLow, hitRumbleHigh, Seconds(hitRumbleFrames));
    RequestFlash(hitFlashFrames);
    pushScale_ = 1.0f;
    pendingStopFrames_ = 0;
}

} // namespace Project_TD2_2
