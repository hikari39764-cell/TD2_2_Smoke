#pragma once

#include "HIKARI/Project/EnvironmentTypes.h"
#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/VfxTypes.h"

namespace Project_TD2_2 {

// 左手の所作（手の光の演出が読む）：ふだんと、投げナイフの前隙。
enum class BossBodyLeftGesture { Balance, Knife };


// 技の演出の実行時の状態（保存しない）。
struct BossBodyAccentState {
    // 刀に付くエフェクトの再生。
    HIKARI::GameProjectVfxPlaybackHandle bladeVfx{};
    // 燃える刀身の流体の炎の再生（刀が燃えている間だけ）と、速く振る間にシェーダーの炎の帯を戻している割合（0〜1）。
    HIKARI::GameProjectVfxPlaybackHandle bladeFireVfx{};
    float bladeFlameFast = 0.0f;
    // 刀光を出し続ける残り時間 (秒)、刀光の明るさ（0〜1）、刀身の炎の明るさ（0〜1）。
    float trailHold = 0.0f;
    float trailGain = 0.0f;
    float heatGain = 0.0f;
    // 振りのゆがみの強さ（0〜1）。
    float hazeGain = 0.0f;
    // 刃口の常時の光のいまの明るさ（倍率）と脈の時計 (秒)、暗い縁のいまの濃さ。
    float edgeGain = 0.0f;
    float edgeClock = 0.0f;
    float edgeDarkGain = 0.0f;
    // 刃文の流れ・霞の帯の段階の倍率と、剣先の残光の明るさ（c-ecd963。0〜1）。
    float flowGain = 0.0f;
    float wakeGain = 0.0f;
    // 目の光・刀身を走る光・溜めの気配の再生。
    HIKARI::GameProjectVfxPlaybackHandle eyesVfx{};
    // 兜の中の灯：ゆらぎの時計 (秒)。
    float helmClock = 0.0f;
    HIKARI::GameProjectVfxPlaybackHandle glintVfx{};
    HIKARI::GameProjectVfxPlaybackHandle auraVfx{};
    // 目の閃きの明るさ（出した瞬間の強さから 0 へ）、気配の強さ（0〜1）、前のフレームに構えの合図が出ていたか。
    float flare = 0.0f;
    // 骨格の体の面頬の隙間の光を、目の色で上書きしているか（c-271cba）。
    bool bodyGlowOverridden = false;
    float aura = 0.0f;
    bool wasGlint = false;
    // 見た剣気を放った数（c-ecd963。Boss::releaseCount と比べて、放つ瞬間を見つける）。
    uint32_t releaseSeen = 0;
    // 見た怯みの数（c-808641。怯みの始まりに胴の煙を吹き出す）。
    uint32_t flinchSeen = 0;
    // 胴の煙へ掛ける量と熱さ（倍率）、振り終わりの吹き出しの残り（1→0）、前のフレームが命中の窓だったか。
    float smokeAmount = 1.0f;
    float smokeHeat = 1.0f;
    float puff = 0.0f;
    bool wasWindow = false;
    // 突進の尾の再生と明るさ（0〜1）、前のフレームの足元（速さを出す）。
    HIKARI::GameProjectVfxPlaybackHandle dashVfx{};
    float dash = 0.0f;
    HIKARI::GameProjectFloat3 previousFeet{};
    bool feetSeen = false;
    // 衝撃の輪を出す瞬間を見分ける、前のフレームの回し蹴りの輪（shockSeen が偽の間は、まだ見ていない）。
    bool shockSeen = false;
    uint64_t previousRing = 0;
    // 左手の光の再生と明るさ（0〜1）、前のフレームの所作・技の段（弾ける瞬間を見分ける。-1＝まだ見ていない）。
    HIKARI::GameProjectVfxPlaybackHandle handVfx{};
    float hand = 0.0f;
    int previousGesture = -1;
    int previousHandPhase = -1;
    // 範囲爆発の溜め：手に集めるエフェクトと押し縮める瞬間のエフェクトの再生、出しているのが火の版か、もう押し縮めたか、
    // 芯の光（追う値）、明滅の時計、叩きつけの後に消すまでの残り (秒)、足元の霧を寄せる強さ（0〜1。BossSmoke が読む）。
    HIKARI::GameProjectVfxPlaybackHandle gatherVfx{};
    HIKARI::GameProjectVfxPlaybackHandle pinchVfx{};
    bool gatherFire = false;
    bool gatherPinched = false;
    float gatherCoreGain = 0.0f;
    float gatherClock = 0.0f;
    float gatherLinger = 0.0f;
    float gatherPull = 0.0f;
    // 掌へ流れ込む腕（BossSmoke が読む）：濃さ（0〜1。0 は出さない）、回りの進み（周の数）、掌の位置（世界）。
    float gatherArm = 0.0f;
    float gatherPhase = 0.0f;
    HIKARI::GameProjectFloat3 gatherPoint{};
};


// 草摺の枚数の上限（表示の番号を先に取っておく数）。
inline constexpr int kBossBodyTassetCapacity = 12;

// 体の見た目の実行時の状態（保存しない）。
struct BossBodyState {
    HIKARI::GameProjectVfxPlaybackHandle smokeVfx{};
    // フェーズ 2 の燃える胴の強さ（0〜1。c-857ae3）。
    float fire = 0.0f;
    // フェーズ 2 の場（BossPhaseTwoField）：場の色の進み（0〜1）と、最後に送った進み（負は「置き換えていない」）、
    // 置き換える前の色調整（初めて置き換える時に覚える）、火の粉と焦げた輪のエフェクト、焦げた輪の場所と進み。
    float fieldWarm = 0.0f;
    float fieldWarmSent = -1.0f;
    bool fieldBaseSeen = false;
    HIKARI::GameProjectColorGrading fieldBase{};
    HIKARI::GameProjectVfxPlaybackHandle fieldEmbersVfx{};
    HIKARI::GameProjectVfxPlaybackHandle fieldScorchVfx{};
    HIKARI::GameProjectFloat3 fieldScorchAt{};
    bool fieldScorchPlaced = false;
    float fieldScorchShow = 0.0f;
    // 左手の所作（技の型から。手の光の演出が読む）。
    BossBodyLeftGesture leftGesture = BossBodyLeftGesture::Balance;
    BossBodyAccentState accents;
};

} // namespace Project_TD2_2
