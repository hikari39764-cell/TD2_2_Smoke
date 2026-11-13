#include "BossVisuals.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/VfxView.h"
#include "BossMath.h"
#include "BossPose.h"
#include "../Burn/BurnFieldRules.h"
#include "../Common/FluidChannels.h"
#include "../Common/VfxParameters.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 資産の入力（エフェクト側の契約）。
constexpr const char* kAmountInput = "input.amount";
constexpr const char* kColorInput = "input.color";
// 剣先の火の粉の光の大きさ（0 で出さない。ナイフの尾は出さない）。
constexpr const char* kEmberInput = "input.ember";
// 同じ光を青白で出す入力（霞の技の構え。火を残す技の間は ember の橙で出す）。
constexpr const char* kMistInput = "input.mist";
// 斬撃のエフェクトの入力：fly＝刃を出す量、impact＝当たって砕ける量。
constexpr const char* kFlyInput = "input.fly";
constexpr const char* kImpactInput = "input.impact";
// 光の層（刃口・炎・光・屈折）の大きさ。霧の源（fly）とは別に、数フレームかけて縮める。
constexpr const char* kGlowInput = "input.glow";
// 炎の輪のシェーダーの層の明るさ（Color。輪が消える時に 0 へ下げる）。
constexpr const char* kFadeInput = "input.fade";
// 霞の大剣のエフェクトの入力：柄から剣先へ満たす割合（0〜1）。出す量は amount。
constexpr const char* kFillInput = "input.fill";
// 剣に沿って場の火の域へ入れる燃料の番号（BossSmoke の注入・押しの番号と重ならない所）。
constexpr uint64_t kBladeFuelBase = 900;

// lit は剣が燃えている間（霞を炎の色にする）。
HIKARI::GameProjectVfxParameterOverrides Parameters(const BossComponent& boss, float amount, float ember,
                                                    bool lit = false) {
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kAmountInput, amount);
    VfxParameters::SetFloat(parameters, kEmberInput, lit ? ember : 0.0f);
    VfxParameters::SetFloat(parameters, kMistInput, lit ? 0.0f : ember);
    VfxParameters::SetColor(parameters, kColorInput, lit ? boss.bladeLitColor : boss.hazeColor);
    return parameters;
}

HIKARI::GameProjectVfxPlaybackHandle Play(HIKARI::WorldView& world, BossComponent& boss,
                                          const HIKARI::GameProjectVfxAssetReference& asset,
                                          const HIKARI::GameProjectTransform& transform, float amount, float ember,
                                          bool lit = false) {
    if (asset.IsEmpty())
        return {};
    HIKARI::GameProjectVfxPlayRequest play{};
    play.asset = asset;
    play.transform = transform;
    play.parameters = Parameters(boss, amount, ember, lit);
    const auto handle = world.GetVfx().Play(play);
    boss.vfxAccepted = handle.IsValid() && boss.vfxAccepted;
    return handle;
}

void StopHandle(HIKARI::WorldView& world, HIKARI::GameProjectVfxPlaybackHandle& handle) {
    if (handle.IsValid())
        world.GetVfx().Stop(handle);
    handle = {};
}

const BossWave* FindWave(const BossComponent& boss, uint64_t id) {
    for (const auto& wave : boss.waves) {
        if (wave.active && !wave.knife && !wave.ring && wave.id == id)
            return &wave;
    }
    return nullptr;
}

void SendSlash(HIKARI::WorldView& world, BossComponent& boss, const BossSlashEffect& effect) {
    HIKARI::GameProjectVfxUpdateRequest update{};
    update.updateTransform = 1u;
    update.updateParameters = 1u;
    update.transform.position = effect.position;
    update.transform.rotation = effect.rotation;
    update.transform.scale = {effect.scale, effect.scale, effect.scale};
    VfxParameters::SetFloat(update.parameters, kFlyInput, effect.flying ? 1.0f : 0.0f);
    VfxParameters::SetFloat(update.parameters, kImpactInput, effect.impactSeconds > 0.0f ? 1.0f : 0.0f);
    VfxParameters::SetFloat(update.parameters, kGlowInput, effect.glow);
    boss.vfxAccepted = world.GetVfx().Update(effect.handle, update) && boss.vfxAccepted;
}

// 飛ぶ斬撃の本体：斬撃 1 つにつき、自前の流体域を持つ三日月の霧のエフェクトを 1 つ飛ばす。
void UpdateSlashes(HIKARI::WorldView& world, BossComponent& boss, float seconds) {
    const auto vfx = world.GetVfx();
    for (auto& effect : boss.slashEffects) {
        if (!effect.handle.IsValid())
            continue;
        if (!vfx.IsActive(effect.handle)) {
            effect = {};
            continue;
        }
        const BossWave* wave = effect.flying ? FindWave(boss, effect.waveId) : nullptr;
        if (wave != nullptr) {
            effect.position = wave->position;
            effect.scale = wave->width / std::max(0.01f, boss.slashEffectWidth);
            effect.travelled = wave->travelled;
            if (wave->struck && !effect.struck) {
                // プレイヤーに当たった：刃を出すのをやめて砕けさせ、勢いを落としてその場で散らす
                // （そのままの刃がプレイヤーの後ろのカメラを通り抜けて、画面を白く覆わないように）。
                effect.struck = true;
                effect.flying = false;
                effect.impactSeconds = boss.slashImpactSeconds;
                effect.fadeSeconds = boss.slashHitFadeSeconds;
                effect.velocity = Mul(effect.velocity, boss.slashHitSpeedScale);
            }
        } else {
            // 斬撃が消えた：刃を出すのをやめ、残った霧が消えるまで同じ速さで飛ばしてから止める（域ごと急に消さない）。
            if (effect.flying) {
                effect.flying = false;
                effect.fadeSeconds = boss.slashFadeSeconds;
            }
            effect.fadeSeconds -= seconds;
            if (effect.fadeSeconds <= 0.0f) {
                StopHandle(world, effect.handle);
                effect = {};
                continue;
            }
            effect.position = Add(effect.position, Mul(effect.velocity, seconds));
        }
        if (!effect.flying)
            effect.glow = std::max(0.0f, effect.glow - seconds / std::max(0.001f, boss.slashGlowFadeSeconds));
        effect.impactSeconds = std::max(0.0f, effect.impactSeconds - seconds);
        SendSlash(world, boss, effect);
    }
    if (boss.slashEffect.IsEmpty())
        return;
    for (size_t i = 0; i < boss.waves.size(); ++i) {
        const BossWave& wave = boss.waves[i];
        // waveVfxId は「この斬撃にはもうエフェクトを出した」の印（1 つの斬撃に出すのは 1 回だけ）。
        // 霞の輪（回し蹴り）は斬撃のエフェクトを使わない（UpdateRing が見せる）。
        if (!wave.active || wave.knife || wave.ring || boss.waveVfxId[i] == wave.id)
            continue;
        BossSlashEffect* slot = nullptr;
        BossSlashEffect* oldest = nullptr;
        for (auto& effect : boss.slashEffects) {
            if (!effect.handle.IsValid()) {
                slot = &effect;
                break;
            }
            if (oldest == nullptr || effect.flying < oldest->flying ||
                (effect.flying == oldest->flying && effect.travelled > oldest->travelled))
                oldest = &effect;
        }
        if (slot == nullptr) {
            // 枠が無い：消えかけ（無ければ一番遠くまで飛んだもの）を止め、次のフレームで出す。
            // 止めた域もそのフレームの間は流体域の数に入るので、同じフレームには出さない。
            StopHandle(world, oldest->handle);
            *oldest = {};
            break;
        }
        boss.waveVfxId[i] = wave.id;
        *slot = {};
        slot->waveId = wave.id;
        slot->flying = true;
        slot->position = wave.position;
        slot->velocity = Mul(wave.direction, wave.speed);
        slot->rotation = BasisRotation(wave.direction, wave.across);
        slot->scale = wave.width / std::max(0.01f, boss.slashEffectWidth);
        HIKARI::GameProjectVfxPlayRequest play{};
        // 火を残す一撃は炎の刃、残さない一撃は煙の刃（出た刃を見れば、地面が燃えるかどうかが分かる）。
        play.asset = wave.fire || boss.slashSmokeEffect.IsEmpty() ? boss.slashEffect : boss.slashSmokeEffect;
        play.transform.position = slot->position;
        play.transform.rotation = slot->rotation;
        play.transform.scale = {slot->scale, slot->scale, slot->scale};
        play.randomSeed = static_cast<uint32_t>(wave.id);
        VfxParameters::SetFloat(play.parameters, kFlyInput, 1.0f);
        VfxParameters::SetFloat(play.parameters, kImpactInput, 0.0f);
        VfxParameters::SetFloat(play.parameters, kGlowInput, 1.0f);
        slot->handle = vfx.Play(play);
        boss.vfxAccepted = slot->handle.IsValid() && boss.vfxAccepted;
        // 位置は毎フレーム送る。エフェクト側の速度で二重に動かさない。
        if (slot->handle.IsValid())
            boss.vfxAccepted = vfx.SetVelocity(slot->handle, {}) && boss.vfxAccepted;
    }
}

// 回し蹴りの霞の輪：広がる炎の輪のエフェクトを 1 つ出し、輪の半径に合わせて根の大きさを変える。
// 輪が消えたら燃料を止め、炎が消えるまで待ってから止める（域ごと急に消さない）。
void UpdateRing(HIKARI::WorldView& world, BossComponent& boss, float seconds) {
    const auto vfx = world.GetVfx();
    if (boss.ringVfx.IsValid() && !vfx.IsActive(boss.ringVfx))
        boss.ringVfx = {};
    const BossWave* ring = nullptr;
    for (const auto& wave : boss.waves) {
        if (wave.active && wave.ring)
            ring = &wave;
    }
    if (ring != nullptr && ring->id != boss.ringVfxId && !boss.ringEffect.IsEmpty()) {
        // 前の輪が残っていたら止め、次のフレームで出す（止めた域もそのフレームの間は流体域の数に入る）。
        if (boss.ringVfx.IsValid()) {
            StopHandle(world, boss.ringVfx);
            return;
        }
        boss.ringVfxId = ring->id;
        boss.ringFade = -1.0f;
        boss.ringScale = ring->radius / std::max(0.01f, boss.ringEffectRadius);
        boss.ringCenter = {ring->position.x, boss.ground.y + boss.ringHeight, ring->position.z};
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = boss.ringEffect;
        play.transform.position = boss.ringCenter;
        play.transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        play.transform.scale = {boss.ringScale, boss.ringScale, boss.ringScale};
        play.randomSeed = static_cast<uint32_t>(ring->id);
        VfxParameters::SetFloat(play.parameters, kFlyInput, 1.0f);
        VfxParameters::SetColor(play.parameters, kFadeInput, {1.0f, 1.0f, 1.0f});
        boss.ringVfx = vfx.Play(play);
        boss.vfxAccepted = boss.ringVfx.IsValid() && boss.vfxAccepted;
        return;
    }
    if (!boss.ringVfx.IsValid())
        return;
    const bool flying = ring != nullptr && ring->id == boss.ringVfxId;
    if (flying) {
        boss.ringScale = ring->radius / std::max(0.01f, boss.ringEffectRadius);
    } else {
        if (boss.ringFade < 0.0f)
            boss.ringFade = boss.ringFadeSeconds;
        boss.ringFade -= seconds;
        if (boss.ringFade <= 0.0f) {
            StopHandle(world, boss.ringVfx);
            return;
        }
    }
    // シェーダーの輪は、輪が消えたら明るさを 0 へ下げる（流体の炎は自分で燃え尽きる）。
    const float glow = flying ? 1.0f : std::clamp(boss.ringFade / std::max(0.001f, boss.ringFadeSeconds), 0.0f, 1.0f);
    HIKARI::GameProjectVfxUpdateRequest update{};
    update.updateTransform = 1u;
    update.updateParameters = 1u;
    update.transform.position = boss.ringCenter;
    update.transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    update.transform.scale = {boss.ringScale, boss.ringScale, boss.ringScale};
    VfxParameters::SetFloat(update.parameters, kFlyInput, flying ? 1.0f : 0.0f);
    VfxParameters::SetColor(update.parameters, kFadeInput, {glow, glow, glow});
    boss.vfxAccepted = vfx.Update(boss.ringVfx, update) && boss.vfxAccepted;
}

// 着地・跳ぶ所の塊を、地面からこの高さに出す。
constexpr float kSkyfallPuffLift = 0.5f;
// 炎の輪の出始めの大きさ（広がり切った時に対する割合）。
constexpr float kSkyfallRingStart = 0.2f;

// 天降り斬りの見た目：跳ぶ所の霞、一緒に落ちる刃、着地の周りの塊と炎の輪。溜めの間の、剣へ集まる光の粒もここで出す
// （退き溜めの溜めにも出る）。
// 規則が書く boss.skyfall を読むだけ（当たりや落ちる所は変えない）。地面には何も予告しない。
void UpdateSkyfall(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds) {
    const auto vfx = world.GetVfx();
    const BossSkyfallState& sky = boss.skyfall;
    const BossSkyfallPhase seen = boss.skyfallSeenPhase;
    boss.skyfallSeenPhase = sky.phase;
    const bool fire = boss.lastPhaseNumber >= 2;
    // 跳び上がる瞬間：足元の霧を押しのけ、霞の塊を巻き上げる（爆発の枠へ入れて、同じフレームの後ろで出す）。
    if (sky.phase == BossSkyfallPhase::Rise && seen != BossSkyfallPhase::Rise && boss.skyfallTakeoffBlastScale > 0.0f) {
        BossBlast& blast = boss.blasts[boss.blastCursor++ % boss.blasts.size()];
        blast = {};
        blast.pushPending = blast.effectPending = true;
        blast.puff = true;
        blast.scale = boss.skyfallTakeoffBlastScale;
        blast.position = {sky.takeoff.x, sky.takeoff.y + kSkyfallPuffLift, sky.takeoff.z};
    }
    // 溜める間（天降り斬りの空中・退き溜め）：剣へ集まって来る光の粒。溜め終わったら止める。
    if (boss.chargeVisual > 0.0f && !boss.gatherEffect.IsEmpty()) {
        HIKARI::GameProjectTransform transform{};
        // 剣の真ん中へ集める（頭上に掲げた剣先だと、見上げるカメラの上の縁から外れやすい）。
        const auto sword = BossPose::SwordPoints(boss, BossPose::Target(boss));
        transform.position = Lerp(sword.inner, sword.outer, 0.5f);
        transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        transform.scale = {boss.chargeGatherScale, boss.chargeGatherScale, boss.chargeGatherScale};
        HIKARI::GameProjectVfxParameterOverrides parameters{};
        VfxParameters::SetFloat(parameters, kMistInput, fire ? 0.0f : 1.0f);
        VfxParameters::SetFloat(parameters, kEmberInput, fire ? 1.0f : 0.0f);
        if (!boss.gatherVfx.IsValid()) {
            HIKARI::GameProjectVfxPlayRequest play{};
            play.asset = boss.gatherEffect;
            play.transform = transform;
            play.parameters = parameters;
            boss.gatherVfx = vfx.Play(play);
            boss.vfxAccepted = boss.gatherVfx.IsValid() && boss.vfxAccepted;
        } else {
            HIKARI::GameProjectVfxUpdateRequest update{};
            update.updateTransform = 1u;
            update.updateParameters = 1u;
            update.transform = transform;
            update.parameters = parameters;
            boss.vfxAccepted = vfx.Update(boss.gatherVfx, update) && boss.vfxAccepted;
        }
    } else {
        StopHandle(world, boss.gatherVfx);
    }
    // 落ちる所を決めた瞬間：剣がひときわ光る（光そのものは剣先の霞のエフェクトが出す。ここは強さだけ）。
    if (sky.locked && !boss.skyfallSeenLock)
        boss.skyfallFlare = 1.0f;
    boss.skyfallSeenLock = sky.locked;
    boss.skyfallFlare =
        std::max(0.0f, boss.skyfallFlare - seconds / std::max(0.001f, boss.skyfallLockFlareSeconds));
    // 落ちる間：体と一緒に落ちる刃（斬撃のエフェクトを 1 つ、落ちる向きに向けて持って行く）。着地で砕けさせる。
    BossSlashEffect& blade = boss.skyfallBlade;
    if (blade.handle.IsValid() && !vfx.IsActive(blade.handle))
        blade = {};
    HIKARI::GameProjectTransform body{};
    if (sky.phase == BossSkyfallPhase::Dive && boss.skyfallBladeWidth > 0.0f && !boss.slashEffect.IsEmpty() &&
        object.GetWorldTransform(body)) {
        if (!blade.handle.IsValid()) {
            const Vec3 direction = Normalize(Sub(sky.target, body.position), {0.0f, -1.0f, 0.0f});
            const Vec3 across = Normalize(Cross({0.0f, 1.0f, 0.0f}, direction), {1.0f, 0.0f, 0.0f});
            blade = {};
            blade.flying = true;
            blade.position = body.position;
            blade.rotation = BasisRotation(direction, across);
            blade.scale = boss.skyfallBladeWidth / std::max(0.01f, boss.slashEffectWidth);
            HIKARI::GameProjectVfxPlayRequest play{};
            play.asset = fire || boss.slashSmokeEffect.IsEmpty() ? boss.slashEffect : boss.slashSmokeEffect;
            play.transform.position = blade.position;
            play.transform.rotation = blade.rotation;
            play.transform.scale = {blade.scale, blade.scale, blade.scale};
            VfxParameters::SetFloat(play.parameters, kFlyInput, 1.0f);
            VfxParameters::SetFloat(play.parameters, kImpactInput, 0.0f);
            VfxParameters::SetFloat(play.parameters, kGlowInput, 1.0f);
            blade.handle = vfx.Play(play);
            boss.vfxAccepted = blade.handle.IsValid() && boss.vfxAccepted;
            // 位置は毎フレーム送る。エフェクト側の速度で二重に動かさない。
            if (blade.handle.IsValid())
                boss.vfxAccepted = vfx.SetVelocity(blade.handle, {}) && boss.vfxAccepted;
        } else {
            if (seconds > 0.0f)
                blade.velocity = Mul(Sub(body.position, blade.position), 1.0f / seconds);
            blade.position = body.position;
        }
    } else if (blade.handle.IsValid() && blade.flying) {
        if (fire) {
            // フェーズ 2 の着地は炎の輪と火球が後を引き受けるので、刃はすぐ止める（刃の域と輪の域が重なると、
            // 流体域の数の上限を超えて、砲口の炎などが出ないフレームができる）。
            StopHandle(world, blade.handle);
            blade = {};
        } else {
            // 着地した（か、打ち切られた）：刃を出すのをやめて砕けさせ、勢いを落としてその場で散らす
            // （砕けた霞が、着地の上に大きく立ちのぼる）。
            blade.flying = false;
            blade.impactSeconds = boss.slashImpactSeconds;
            blade.fadeSeconds = boss.slashFadeSeconds;
            blade.velocity = Mul(blade.velocity, boss.slashHitSpeedScale);
        }
    }
    if (blade.handle.IsValid() && !blade.flying) {
        blade.fadeSeconds -= seconds;
        blade.glow = std::max(0.0f, blade.glow - seconds / std::max(0.001f, boss.slashGlowFadeSeconds));
        blade.impactSeconds = std::max(0.0f, blade.impactSeconds - seconds);
        blade.position = Add(blade.position, Mul(blade.velocity, seconds));
        if (blade.fadeSeconds <= 0.0f) {
            StopHandle(world, blade.handle);
            blade = {};
        }
    }
    if (blade.handle.IsValid())
        SendSlash(world, boss, blade);
    // 着地した瞬間：着地点の周りに霞（フェーズ 2 は火球）の塊を立て、フェーズ 2 は炎の輪を広げる。
    // 真ん中の爆発と場の霧の押しは、規則が入れた爆発の枠（skyfallBlastScale）が出す。
    if (sky.phase == BossSkyfallPhase::Landed && seen != BossSkyfallPhase::Landed) {
        for (int i = 0; i < boss.skyfallLandPuffs; ++i) {
            const float angle =
                2.0f * kPi * (static_cast<float>(i) + 0.5f) / static_cast<float>(boss.skyfallLandPuffs);
            const Vec3 at{sky.target.x + std::cos(angle) * boss.skyfallLandPuffDistance,
                          sky.target.y + kSkyfallPuffLift,
                          sky.target.z + std::sin(angle) * boss.skyfallLandPuffDistance};
            BurnFieldRules::AddBlast(world, at, boss.skyfallLandPuffRadius,
                                     fire ? BurnBlastKind::Hit : BurnBlastKind::Smoke);
        }
        boss.skyfallRingPending = fire && !boss.ringEffect.IsEmpty() && boss.skyfallRingRadius > 0.0f;
    } else if (boss.skyfallRingPending) {
        // 着地の次のフレーム：炎の輪を出す（技が打ち切られていたら出さない）。
        boss.skyfallRingPending = false;
        if (sky.phase == BossSkyfallPhase::Landed && !boss.skyfallRingVfx.IsValid()) {
            boss.skyfallRingAge = 0.0f;
            boss.skyfallRingCenter = {sky.target.x, boss.ground.y + boss.ringHeight, sky.target.z};
            const float scale = kSkyfallRingStart * boss.skyfallRingRadius / std::max(0.01f, boss.ringEffectRadius);
            HIKARI::GameProjectVfxPlayRequest play{};
            play.asset = boss.ringEffect;
            play.transform.position = boss.skyfallRingCenter;
            play.transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
            play.transform.scale = {scale, scale, scale};
            VfxParameters::SetFloat(play.parameters, kFlyInput, 1.0f);
            VfxParameters::SetColor(play.parameters, kFadeInput, {1.0f, 1.0f, 1.0f});
            boss.skyfallRingVfx = vfx.Play(play);
            boss.vfxAccepted = boss.skyfallRingVfx.IsValid() && boss.vfxAccepted;
        }
    }
    // 炎の輪：広がり切るまで燃料を出し、その後は光を下げて消す（流体の炎は自分で燃え尽きる）。
    if (boss.skyfallRingVfx.IsValid() && !vfx.IsActive(boss.skyfallRingVfx))
        boss.skyfallRingVfx = {};
    if (boss.skyfallRingVfx.IsValid()) {
        boss.skyfallRingAge += seconds;
        const float fade = boss.skyfallRingAge - boss.skyfallRingSeconds;
        if (fade >= boss.ringFadeSeconds) {
            StopHandle(world, boss.skyfallRingVfx);
            return;
        }
        const float grow = std::clamp(boss.skyfallRingAge / std::max(0.001f, boss.skyfallRingSeconds), 0.0f, 1.0f);
        const float glow = fade <= 0.0f ? 1.0f : 1.0f - fade / std::max(0.001f, boss.ringFadeSeconds);
        const float scale =
            Lerp(kSkyfallRingStart, 1.0f, grow) * boss.skyfallRingRadius / std::max(0.01f, boss.ringEffectRadius);
        HIKARI::GameProjectVfxUpdateRequest update{};
        update.updateTransform = 1u;
        update.updateParameters = 1u;
        update.transform.position = boss.skyfallRingCenter;
        update.transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        update.transform.scale = {scale, scale, scale};
        VfxParameters::SetFloat(update.parameters, kFlyInput, fade <= 0.0f ? 1.0f : 0.0f);
        VfxParameters::SetColor(update.parameters, kFadeInput, {glow, glow, glow});
        boss.vfxAccepted = vfx.Update(boss.skyfallRingVfx, update) && boss.vfxAccepted;
    }
}

// 場の霧を押す所を、地面からこの高さに置く。
constexpr float kGreatswordPushLift = 0.5f;

// 霞の大剣の見た目：大剣の模型の輪郭へ流体を流し込むエフェクトを、握りの位置と剣の向きに合わせて持って行く。
// 満ちる割合（柄から剣先へ）は規則が書く boss.greatsword.fill をそのまま送る。規則は変えない。
void UpdateGreatsword(HIKARI::WorldView& world, BossComponent& boss, float seconds) {
    const auto vfx = world.GetVfx();
    const BossGreatswordState& sword = boss.greatsword;
    const BossGreatswordPhase seen = boss.greatswordSeenPhase;
    boss.greatswordSeenPhase = sword.phase;
    if (boss.greatswordVfx.IsValid() && !vfx.IsActive(boss.greatswordVfx))
        boss.greatswordVfx = {};
    const bool held = sword.phase != BossGreatswordPhase::None;
    if (held && seen == BossGreatswordPhase::None && boss.greatswordVfx.IsValid()) {
        // 前の大剣がまだ消えかけている：止めて、次のフレームに出す（止めた域もそのフレームの間は流体域の数に入る）。
        StopHandle(world, boss.greatswordVfx);
        boss.greatswordSeenPhase = BossGreatswordPhase::None;
        return;
    }
    if (held) {
        // 剣の平らな面：作る間はボスの正面へ向け（プレイヤーから剣の形が見える）、薙ぐ時は上へ向ける（刃が進む向きに立つ）。
        // 薙ぐ時の面は真上から少しボスの正面へ傾け、前にいるプレイヤーからも面が見えるようにする。
        const Vec3 front = Forward(boss.yaw);
        const Vec3 swing = Normalize(Add(Vec3{0.0f, 1.0f, 0.0f}, Mul(front, boss.greatswordSweepFaceTilt)), {0.0f, 1.0f, 0.0f});
        const Vec3 normal = Normalize(Lerp(front, swing, sword.wind), {0.0f, 1.0f, 0.0f});
        boss.greatswordTransform.position = sword.hilt;
        boss.greatswordTransform.rotation = BasisRotation(Cross(sword.direction, normal), sword.direction);
        boss.greatswordTransform.scale = {1.0f, 1.0f, 1.0f};
        boss.greatswordFade = boss.greatswordFadeSeconds;
    } else if (boss.greatswordVfx.IsValid()) {
        // 技が終わった・崩された：出すのをやめ、残った霞が消えるまで待ってから止める（域ごと急に消さない）。
        boss.greatswordFade -= seconds;
        if (boss.greatswordFade <= 0.0f) {
            StopHandle(world, boss.greatswordVfx);
            return;
        }
    } else {
        return;
    }
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kFillInput, held ? sword.fill : 1.0f);
    // 薙ぐ間は多めに流し込む。ほどける間と終わった後は、もう出さない（残った霞が自分で消える）。
    const float amount = sword.phase == BossGreatswordPhase::Conjure ? 1.0f
                         : sword.phase == BossGreatswordPhase::Sweep ? boss.greatswordSweepAmount
                                                                     : 0.0f;
    VfxParameters::SetFloat(parameters, kAmountInput, amount);
    if (!boss.greatswordVfx.IsValid()) {
        const auto& asset = sword.fire ? boss.greatswordFireEffect : boss.greatswordMistEffect;
        if (!held || asset.IsEmpty())
            return;
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = asset;
        play.transform = boss.greatswordTransform;
        play.parameters = parameters;
        boss.greatswordVfx = vfx.Play(play);
        boss.vfxAccepted = boss.greatswordVfx.IsValid() && boss.vfxAccepted;
    } else {
        HIKARI::GameProjectVfxUpdateRequest update{};
        update.updateTransform = 1u;
        update.updateParameters = 1u;
        update.transform = boss.greatswordTransform;
        update.parameters = parameters;
        boss.vfxAccepted = vfx.Update(boss.greatswordVfx, update) && boss.vfxAccepted;
    }
    // 薙ぎ終わった瞬間：剣が通った扇に沿って、場の霧を外へ押す（爆発の枠の「押し」だけを使う。光は出さない）。
    if (sword.phase == BossGreatswordPhase::Dissolve && seen == BossGreatswordPhase::Sweep &&
        boss.greatswordSweepPushes > 0 && boss.greatswordSweepPushDistance > 0.0f) {
        const float yawEnd = YawOf(sword.direction);
        const float swept = boss.greatswordSweepDegrees * kRadiansPerDegree;
        for (int i = 0; i < boss.greatswordSweepPushes; ++i) {
            const float back = swept * (static_cast<float>(i) + 0.5f) / static_cast<float>(boss.greatswordSweepPushes);
            const Vec3 at = Add(boss.ground, Mul(Forward(yawEnd - back), boss.greatswordSweepPushDistance));
            BossBlast& blast = boss.blasts[boss.blastCursor++ % boss.blasts.size()];
            blast = {};
            blast.pushPending = true;
            blast.position = {at.x, boss.ground.y + kGreatswordPushLift, at.z};
        }
    }
}

} // namespace

// 火を残す技の間、剣を燃やす：剣に沿って場の火の域へ燃料を入れ（振ると炎が弧を引く）、火が付いた瞬間に小さな火球を出す。
void UpdateBladeFire(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, bool lit) {
    const BossSwordPoints sword = boss.swordSmoke ? BossSwordPoints{boss.swordInner, boss.swordInner, boss.swordOuter}
                                                  : BossPose::SwordPoints(boss, BossPose::Target(boss));
    if (lit && !boss.bladeWasLit && boss.igniteFlashRadius > 0.0f)
        BurnFieldRules::AddBlast(world, sword.outer, boss.igniteFlashRadius, BurnBlastKind::Hit);
    boss.bladeWasLit = lit;
    if (!lit || !(boss.bladeFuel > 0.0f))
        return;
    const Vec3 points[2] = {sword.outer, Mul(Add(sword.inner, sword.outer), 0.5f)};
    for (uint64_t i = 0; i < 2; ++i) {
        // 同じ番号で出し続けるので、剣が動くとエンジンが通った線に沿って燃料を広げる（炎が剣の跡を引く）。
        HIKARI::GameProjectFluidInteraction fuel{};
        fuel.kind = HIKARI::GameProjectFluidInteractionKind::Fuel;
        fuel.position = points[i];
        fuel.extents = {boss.bladeFuelRadius, std::max(0.0f, boss.bladeFuelTemperature), 0.0f};
        fuel.strength = boss.bladeFuel;
        fuel.channels = FluidChannels::kFire;
        boss.fluidAccepted =
            world.GetFluid().Submit(object.GetHandle(), kBladeFuelBase + i, fuel).Accepted() && boss.fluidAccepted;
    }
}

void BossVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds) {
    // 天降り斬りは爆発の枠へ塊を入れるので、枠を出す所より前に進める。
    UpdateSkyfall(world, object, boss, seconds);
    UpdateGreatsword(world, boss, seconds);
    UpdateSlashes(world, boss, seconds);
    UpdateRing(world, boss, seconds);
    // 当たった所の爆発の光（一度きりのエフェクト。入力は無い）。
    for (auto& blast : boss.blasts) {
        if (!blast.effectPending)
            continue;
        blast.effectPending = false;
        // 塊そのものは流体で出す：剣が燃えている技で当てた所は場の火の域の火球、ほかは場の霧へ弾ける霞の塊。
        BurnFieldRules::AddBlast(world, blast.position,
                                 (blast.puff ? boss.puffRadius : boss.hitBlastRadius) * blast.scale,
                                 blast.fire && !blast.puff ? BurnBlastKind::Hit : BurnBlastKind::Smoke);
        // その上に重ねるエフェクト（当たった所は光・衝撃の輪・火花）。
        const auto& asset = blast.puff                                       ? boss.puffEffect
                            : !blast.fire && !boss.mistBlastEffect.IsEmpty() ? boss.mistBlastEffect
                                                                             : boss.blastEffect;
        if (asset.IsEmpty())
            continue;
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = asset;
        play.transform.position = blast.position;
        play.transform.scale = {blast.scale, blast.scale, blast.scale};
        play.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
        boss.vfxAccepted = world.GetVfx().Play(play).IsValid() && boss.vfxAccepted;
    }
    // ナイフ：刃と光点のエフェクトを飛ばし、尾は場の霧へ霞を入れるエフェクトで出す。どちらもエフェクト側の速度で進める。
    for (size_t i = 0; i < boss.waves.size(); ++i) {
        const BossWave& wave = boss.waves[i];
        auto& handle = boss.waveVfx[i];
        auto& knife = boss.knifeVfx[i];
        if (!wave.active || boss.waveVfxId[i] != wave.id) {
            StopHandle(world, handle);
            StopHandle(world, knife);
        }
        if (!wave.active || !wave.knife || boss.waveVfxId[i] == wave.id)
            continue;
        boss.waveVfxId[i] = wave.id;
        const Vec3 velocity = Mul(wave.direction, wave.speed);
        HIKARI::GameProjectTransform transform{};
        transform.position = wave.position;
        transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
        handle = Play(world, boss, boss.swordEffect, transform, boss.knifeEffectAmount, 0.0f);
        if (handle.IsValid())
            boss.vfxAccepted = world.GetVfx().SetVelocity(handle, velocity) && boss.vfxAccepted;
        if (boss.knifeEffect.IsEmpty())
            continue;
        // 刃のエフェクトには入力が無い（霞の尾の入力を渡すと、知らない入力として再生を断られる）。
        HIKARI::GameProjectVfxPlayRequest play{};
        play.asset = boss.knifeEffect;
        play.transform.position = wave.position;
        play.transform.rotation = BasisRotation(wave.direction, wave.across);
        knife = world.GetVfx().Play(play);
        boss.vfxAccepted = knife.IsValid() && boss.vfxAccepted;
        if (knife.IsValid())
            boss.vfxAccepted = world.GetVfx().SetVelocity(knife, velocity) && boss.vfxAccepted;
    }
    // 剣先の霞：構えでは剣先に集まり、振っている間は剣先が弧を描く（動く注入は通った線に沿って広がる）。
    const bool swinging = boss.swordSmoke;
    const bool kamae = boss.glintFrames > 0 && boss.state == BossState::Act;
    // 火を残す技の間は、前隙の最初から剣が燃えている（技を出す前の 1 枚の画面で見分けられる）。
    // フェーズ 2 では、待機中も技の間も、剣がいつも燃えている（剣の斬撃がどれも火を残すようになる）。気絶の間は消える。
    const bool acting = boss.state == BossState::Act;
    const bool lit = (boss.bladeLit && acting) ||
                     (boss.lastPhaseNumber >= 2 && (acting || boss.state == BossState::Idle) && !boss.hidden);
    UpdateBladeFire(world, object, boss, lit);
    // 溜める間（天降り斬りの空中・退き溜め）と、天降り斬りで落ちる間も、剣先の霞と光を出す。
    const bool diving = boss.skyfall.phase == BossSkyfallPhase::Dive;
    const bool charging = boss.chargeVisual > 0.0f || diving;
    // 霞の大剣を持っている間は、ふだんの見えない剣の霞と光は出さない（大剣のエフェクトが剣そのもの）。
    // 薙ぐ間だけ、剣先の霞のエフェクトを大剣の上に置いて一緒に動かし、通った所に霞の跡を残す。
    const bool conjured = boss.greatsword.phase != BossGreatswordPhase::None;
    const bool trailing = boss.greatsword.phase == BossGreatswordPhase::Sweep && boss.greatswordTrailAmount > 0.0f;
    if ((conjured && !trailing) || (!swinging && !kamae && !lit && !charging && !trailing)) {
        StopHandle(world, boss.swordVfx);
        return;
    }
    HIKARI::GameProjectTransform transform{};
    const auto sword = BossPose::SwordPoints(boss, BossPose::Target(boss));
    // 溜めの光は剣の真ん中に出す（集まる光の粒と同じ所）。
    transform.position = swinging                   ? boss.swordOuter
                         : boss.chargeVisual > 0.0f ? Lerp(sword.inner, sword.outer, 0.5f)
                                                    : sword.outer;
    transform.rotation = {0.0f, 0.0f, 0.0f, 1.0f};
    float amount = swinging ? boss.swordEffectAmount : boss.kamaeEffectAmount;
    float ember = lit        ? std::max(boss.bladeLitEmber, boss.swordEmberScale)
                  : swinging ? boss.swordEmberScale
                             : boss.kamaeEmberScale;
    if (trailing) {
        const BossGreatswordState& great = boss.greatsword;
        transform.position = Add(great.hilt, Mul(great.direction, great.length * boss.greatswordTrailAlong));
        amount = boss.swordEffectAmount * boss.greatswordTrailAmount;
        ember = boss.swordEmberScale * boss.greatswordTrailGlow;
    }
    if (charging) {
        // 溜めるにつれて剣の光と霞が大きくなる。天降り斬りは、落ちる所を決めた瞬間にひときわ光る
        // （落ちて来る合図は剣の光だけ）。落ちる間は溜め切った大きさのまま。
        const float charge = diving ? 1.0f : boss.chargeVisual;
        amount *= Lerp(1.0f, boss.chargeHazeScale, charge);
        ember *= Lerp(1.0f, boss.chargeGlowScale, charge) * (1.0f + (boss.skyfallLockFlare - 1.0f) * boss.skyfallFlare);
    }
    if (!boss.swordVfx.IsValid()) {
        boss.swordVfx = Play(world, boss, boss.swordEffect, transform, amount, ember, lit);
        return;
    }
    HIKARI::GameProjectVfxUpdateRequest update{};
    update.updateTransform = 1u;
    update.updateParameters = 1u;
    update.transform = transform;
    update.parameters = Parameters(boss, amount, ember, lit);
    boss.vfxAccepted = world.GetVfx().Update(boss.swordVfx, update) && boss.vfxAccepted;
}

void BossVisuals::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    for (auto& handle : boss.waveVfx)
        StopHandle(world, handle);
    for (auto& handle : boss.knifeVfx)
        StopHandle(world, handle);
    StopHandle(world, boss.ringVfx);
    boss.ringVfxId = 0;
    for (auto& effect : boss.slashEffects) {
        StopHandle(world, effect.handle);
        effect = {};
    }
    StopHandle(world, boss.swordVfx);
    StopHandle(world, boss.gatherVfx);
    StopHandle(world, boss.skyfallBlade.handle);
    boss.skyfallBlade = {};
    StopHandle(world, boss.skyfallRingVfx);
    StopHandle(world, boss.greatswordVfx);
}

} // namespace Project_TD2_2
