#include "BossVisuals.h"

#include <algorithm>

#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/VfxView.h"
#include "BossGreatswordVisuals.h"
#include "BossMath.h"
#include "BossPose.h"
#include "BossSkyfallVisuals.h"
#include "BossVfx.h"
#include "../Burn/BurnFieldRules.h"
#include "../Common/FluidChannels.h"
#include "../Common/VfxParameters.h"
#include "../Common/VfxPlayback.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;
using namespace BossVfx;

// 剣に沿って場の火の域へ入れる燃料の番号（BossSmoke の注入・押しの番号と重ならない所）。
constexpr uint64_t kBladeFuelBase = 900;

// 剣先の霞・ナイフの尾の入力。lit は剣が燃えている間（霞を炎の色にする）。
HIKARI::GameProjectVfxParameterOverrides Parameters(const BossComponent& boss, float amount, float ember,
                                                    bool lit = false) {
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kAmountInput, amount);
    VfxParameters::SetFloat(parameters, kEmberInput, lit ? ember : 0.0f);
    VfxParameters::SetFloat(parameters, kMistInput, lit ? 0.0f : ember);
    VfxParameters::SetColor(parameters, kColorInput, lit ? boss.bladeLitColor : boss.hazeColor);
    return parameters;
}

const BossWave* FindWave(const BossComponent& boss, uint64_t id) {
    for (const auto& wave : boss.waves) {
        if (wave.active && !wave.knife && !wave.ring && wave.id == id)
            return &wave;
    }
    return nullptr;
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
        StartSlash(world, boss, *slot, SlashAsset(boss, wave.fire), static_cast<uint32_t>(wave.id));
    }
}

// 回し蹴りの霞の輪：広がる炎の輪のエフェクトを 1 つ出し、輪の半径に合わせて根の大きさを変える。
// 輪が消えたら燃料を止め、炎が消えるまで待ってから止める（域ごと急に消さない）。
void UpdateRing(HIKARI::WorldView& world, BossComponent& boss, float seconds) {
    if (boss.ringVfx.IsValid() && !world.GetVfx().IsActive(boss.ringVfx))
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
        play.transform = Placed(boss.ringCenter, boss.ringScale);
        play.randomSeed = static_cast<uint32_t>(ring->id);
        play.parameters = RingParameters(true, 1.0f);
        boss.ringVfx = Start(world, boss, play);
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
    Send(world, boss, boss.ringVfx, Placed(boss.ringCenter, boss.ringScale), RingParameters(flying, glow));
}

// 溜める間（天降り斬りの空中・退き溜め）：剣へ集まって来る光の粒（フェーズ 1 は冷たい白、フェーズ 2 は橙）。
// 溜め終わったら止める。
void UpdateChargeGather(HIKARI::WorldView& world, BossComponent& boss) {
    if (!(boss.chargeVisual > 0.0f) || boss.gatherEffect.IsEmpty()) {
        StopHandle(world, boss.gatherVfx);
        return;
    }
    const bool fire = boss.lastPhaseNumber >= 2;
    // 剣の真ん中へ集める（頭上に掲げた剣先だと、見上げるカメラの上の縁から外れやすい）。
    const auto sword = BossPose::SwordPoints(boss, BossPose::Target(boss));
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    VfxParameters::SetFloat(parameters, kMistInput, fire ? 0.0f : 1.0f);
    VfxParameters::SetFloat(parameters, kEmberInput, fire ? 1.0f : 0.0f);
    StartOrSend(world, boss, boss.gatherVfx, boss.gatherEffect,
                Placed(Lerp(sword.inner, sword.outer, 0.5f), boss.chargeGatherScale), parameters);
}

// 当たった所の爆発の光（一度きりのエフェクト。入力は無い）。
void PlayBlasts(HIKARI::WorldView& world, BossComponent& boss) {
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
        if (!asset.IsEmpty())
            boss.vfxAccepted = VfxPlayback::PlayOneShot(world, asset, blast.position, blast.scale) && boss.vfxAccepted;
    }
}

// ナイフ：刃と光点のエフェクトを飛ばし、尾は場の霧へ霞を入れるエフェクトで出す。どちらもエフェクト側の速度で進める。
void UpdateKnives(HIKARI::WorldView& world, BossComponent& boss) {
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
        HIKARI::GameProjectVfxPlayRequest tail{};
        tail.asset = boss.swordEffect;
        tail.transform = Placed(wave.position, 1.0f);
        tail.parameters = Parameters(boss, boss.knifeEffectAmount, 0.0f);
        handle = StartMoving(world, boss, tail, velocity);
        // 刃のエフェクトには入力が無い（霞の尾の入力を渡すと、知らない入力として再生を断られる）。
        HIKARI::GameProjectVfxPlayRequest blade{};
        blade.asset = boss.knifeEffect;
        blade.transform = Placed(wave.position, 1.0f, BasisRotation(wave.direction, wave.across));
        knife = StartMoving(world, boss, blade, velocity);
    }
}

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

// 剣先の霞：構えでは剣先に集まり、振っている間は剣先が弧を描く（動く注入は通った線に沿って広がる）。
void UpdateSwordHaze(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss) {
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
    const auto sword = BossPose::SwordPoints(boss, BossPose::Target(boss));
    // 溜めの光は剣の真ん中に出す（集まる光の粒と同じ所）。
    Vec3 position = swinging                   ? boss.swordOuter
                    : boss.chargeVisual > 0.0f ? Lerp(sword.inner, sword.outer, 0.5f)
                                               : sword.outer;
    float amount = swinging ? boss.swordEffectAmount : boss.kamaeEffectAmount;
    float ember = lit        ? std::max(boss.bladeLitEmber, boss.swordEmberScale)
                  : swinging ? boss.swordEmberScale
                             : boss.kamaeEmberScale;
    if (trailing) {
        const BossGreatswordState& great = boss.greatsword;
        position = Add(great.hilt, Mul(great.direction, great.length * boss.greatswordTrailAlong));
        amount = boss.swordEffectAmount * boss.greatswordTrailAmount;
        ember = boss.swordEmberScale * boss.greatswordTrailGlow;
    }
    if (charging) {
        // 溜めるにつれて剣の光と霞が大きくなる。天降り斬りは、落ちる所を決めた瞬間にひときわ光る
        // （落ちて来る合図は剣の光だけ）。落ちる間は溜め切った大きさのまま。
        const float charge = diving ? 1.0f : boss.chargeVisual;
        amount *= Lerp(1.0f, boss.chargeHazeScale, charge);
        ember *= Lerp(1.0f, boss.chargeGlowScale, charge) *
                 (1.0f + (boss.skyfallLockFlare - 1.0f) * boss.skyfallVisual.flare);
    }
    StartOrSend(world, boss, boss.swordVfx, boss.swordEffect, Placed(position, 1.0f),
                Parameters(boss, amount, ember, lit));
}

} // namespace

void BossVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds) {
    UpdateChargeGather(world, boss);
    // 天降り斬り・霞の大剣は爆発の枠へ塊・霧の押しを入れるので、枠を出す所より前に進める。
    BossSkyfallVisuals::Update(world, object, boss, seconds);
    BossGreatswordVisuals::Update(world, boss, seconds);
    UpdateSlashes(world, boss, seconds);
    UpdateRing(world, boss, seconds);
    PlayBlasts(world, boss);
    UpdateKnives(world, boss);
    UpdateSwordHaze(world, object, boss);
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
    BossSkyfallVisuals::Stop(world, boss);
    BossGreatswordVisuals::Stop(world, boss);
}

} // namespace Project_TD2_2
