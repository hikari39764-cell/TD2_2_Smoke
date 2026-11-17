#include "BossVisuals.h"

#include <algorithm>

#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/VfxView.h"
#include "BossFlameRunVisuals.h"
#include "BossGreatswordVisuals.h"
#include "BossMath.h"
#include "BossPose.h"
#include "BossRules.h"
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

// 当たった斬撃は、当たった tick に終わる（active が落ちる）。砕けさせる見た目のために、当たって終わった斬撃も探す
// （枠は次の斬撃が新しい id で使うまで、終わった時のデータを持っている）。
const BossWave* FindWave(const BossComponent& boss, uint64_t id) {
    for (const auto& wave : boss.waves) {
        if ((wave.active || wave.struck) && !wave.knife && !wave.ring && wave.id == id)
            return &wave;
    }
    return nullptr;
}

// 引き込まれる斬撃の両翼を押し寄せる力の提出 ID（斬撃のエフェクトの枠 1 つに 2 個。ほかのボスの提出と重ねない）と、
// 力を置く所（刃の中心から、捕まった時の幅のこの割合だけ外）。
constexpr uint64_t kSqueezeBase = 1300;
constexpr float kSqueezeReachShare = 0.3f;

// 引き込みの間、域に残った三日月の両翼の霧を、刃の中心（口へ向かっている）へ押し寄せる。域は根と一緒に動くが、
// 中の霧は捕まった時の幅のまま残るので、押さないと、口の左右に大きな翼が残る。斬撃のエフェクトの域だけに届く。
void Squeeze(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, const BossSlashEffect& effect,
             size_t slot) {
    if (!(boss.absorbPullSqueeze > 0.0f) || !(boss.absorbPullSqueezeRadius > 0.0f) || !(effect.pullWidth > 0.0f))
        return;
    const float t = std::clamp(effect.pullProgress, 0.0f, 1.0f);
    const float ramp = t * t * (3.0f - 2.0f * t);
    for (uint64_t wing = 0; wing < 2; ++wing) {
        const float sign = wing == 0 ? 1.0f : -1.0f;
        HIKARI::GameProjectFluidInteraction force{};
        force.kind = HIKARI::GameProjectFluidInteractionKind::Force;
        force.position = Add(effect.position, Mul(effect.pullAcross, sign * effect.pullWidth * kSqueezeReachShare));
        force.extents = {boss.absorbPullSqueezeRadius, 0.0f, 0.0f};
        force.direction = Mul(effect.pullAcross, -sign);
        force.strength = boss.absorbPullSqueeze * ramp;
        force.channels = FluidChannels::kSlash;
        boss.fluidAccepted =
            world.GetFluid().Submit(object.GetHandle(), kSqueezeBase + slot * 2 + wing, force).Accepted() &&
            boss.fluidAccepted;
    }
}

// 飛ぶ斬撃の本体：斬撃 1 つにつき、自前の流体域を持つ三日月の霧のエフェクトを 1 つ飛ばす。
void UpdateSlashes(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds,
                   float alpha) {
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
            if (wave->captured) {
                // 吸い込みに捕まった：規則が口へ引き込む位置・向き・幅へ、根ごと付いて行く（域も一緒に動く）。刃の霧は
                // 弱めて出し続け（すぐ止めると何も見えなくなる）、幅のほかの向きを細くして、筋になって入るように見せる。
                // 入りきった後は、口の所で短く消える（元の速さで後ろへ飛んで行かない）。
                const float progress =
                    static_cast<float>(wave->pullFrame) / static_cast<float>(std::max(1, wave->pullFrames));
                effect.captured = true;
                effect.pullProgress = progress;
                effect.pullAcross = wave->pullAcross;
                effect.pullWidth = wave->pullWidth;
                effect.velocity = {};
                effect.position = Lerp(wave->previousPosition, wave->position, std::clamp(alpha, 0.0f, 1.0f));
                effect.rotation = BasisRotation(wave->direction, wave->across);
                // 霧の源は、入りきる時に 0 になるように絞っていく（終わりまで出し続けると、端の霧が口へ届く前に補われる）。
                effect.fly = boss.absorbPullFly * (1.0f - std::clamp(progress, 0.0f, 1.0f));
                effect.pinch = Lerp(1.0f, boss.absorbPullPinch, std::clamp(progress, 0.0f, 1.0f));
            }
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
                effect.fadeSeconds = effect.captured ? boss.absorbPullFadeSeconds : boss.slashFadeSeconds;
                effect.pullProgress = 1.0f;
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
        if (effect.captured)
            Squeeze(world, object, boss, effect, static_cast<size_t>(&effect - boss.slashEffects.data()));
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
    const auto sword = BossPose::SwordPoints(boss, boss.swordPose);
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
        if (blast.playerHit && blast.fire) {
            // 火がプレイヤーに当たった：火球と光の輪は出さない。爆発なら爆発そのものの火球に任せ、火の刃なら、
            // 攻撃の向きへ体を突き抜ける炎（場の火の域の流体）と、同じ向きへ飛ぶ火花を出す。
            if (blast.explosion)
                continue;
            BurnFieldRules::AddJet(world, blast.position, blast.direction, blast.scale);
            HIKARI::GameProjectVfxPlayRequest sparks{};
            sparks.asset = boss.fireHitEffect;
            sparks.transform = Placed(blast.position, blast.scale,
                                      BasisRotation(blast.direction, Cross({0.0f, 1.0f, 0.0f}, blast.direction)));
            sparks.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
            Start(world, boss, sparks);
            continue;
        }
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
void UpdateKnives(HIKARI::WorldView& world, BossComponent& boss, float alpha) {
    for (size_t i = 0; i < boss.waves.size(); ++i) {
        const BossWave& wave = boss.waves[i];
        auto& handle = boss.waveVfx[i];
        auto& knife = boss.knifeVfx[i];
        // 枠がナイフでなくなった（描画の間に斬撃が同じ枠を取り、waveVfxId を先に書き換えた時も）なら止める。
        if (!wave.active || !wave.knife || boss.waveVfxId[i] != wave.id) {
            StopHandle(world, handle);
            StopHandle(world, knife);
        }
        if (wave.active && wave.knife && wave.captured && boss.waveVfxId[i] == wave.id) {
            // 吸い込みに捕まった：尾の霞を止め、刃を口へ向けて曲げ、細くしながら引き込む（規則の位置へ毎フレーム置く）。
            StopHandle(world, handle);
            if (knife.IsValid()) {
                const float progress =
                    static_cast<float>(wave.pullFrame) / static_cast<float>(std::max(1, wave.pullFrames));
                const float scale =
                    Lerp(1.0f, std::clamp(boss.absorbPullEndScale, 0.01f, 1.0f), std::clamp(progress, 0.0f, 1.0f));
                const auto vfx = world.GetVfx();
                HIKARI::GameProjectVfxUpdateRequest update{};
                update.updateTransform = 1u;
                update.transform = Placed(Lerp(wave.previousPosition, wave.position, std::clamp(alpha, 0.0f, 1.0f)),
                                          scale, BasisRotation(wave.direction, wave.across));
                boss.vfxAccepted = vfx.SetVelocity(knife, {}) && vfx.Update(knife, update) && boss.vfxAccepted;
            }
            continue;
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

// 剣に火が付いた瞬間に、剣先へ小さな火球を出す。剣に沿って燃料を入れ続けることはしない：ふだんの剣は模型が無く
// 見えないので、剣の炎は、ボスのそばの空中に斜めに浮いた火に見え、振った後も淡い煙の弧が空に残る
// （剣が燃えていることは、剣先の橙の光で見せる。斬撃の炎は、斬撃のエフェクトが出す）。
void UpdateIgniteFlash(HIKARI::WorldView& world, BossComponent& boss, bool lit) {
    if (lit && !boss.bladeWasLit && boss.igniteFlashRadius > 0.0f) {
        const BossSwordPoints sword = boss.swordSmoke
                                          ? BossSwordPoints{boss.swordInner, boss.swordInner, boss.swordOuter}
                                          : BossPose::SwordPoints(boss, boss.swordPose);
        BurnFieldRules::AddBlast(world, sword.outer, boss.igniteFlashRadius, BurnBlastKind::Hit);
    }
    boss.bladeWasLit = lit;
}

// 剣先の霞：構えでは剣先に集まり、振っている間は剣先が弧を描く（動く注入は通った線に沿って広がる）。
void UpdateSwordHaze(HIKARI::WorldView& world, BossComponent& boss) {
    const bool swinging = boss.swordSmoke;
    const bool kamae = boss.glintFrames > 0 && boss.state == BossState::Act;
    // 火を残す技の間は、前隙の最初から剣が燃えている（技を出す前の 1 枚の画面で見分けられる）。
    const bool lit = BossRules::IsBladeLit(boss);
    UpdateIgniteFlash(world, boss, lit);
    // 溜める間（天降り斬りの空中・退き溜め）と、天降り斬りで落ちる間も、剣先の霞と光を出す。
    const bool diving = boss.skyfall.phase == BossSkyfallPhase::Dive;
    const bool charging = boss.chargeVisual > 0.0f || diving;
    // 霞の大剣を持っている間は、ふだんの見えない剣の霞と光は出さない（大剣のエフェクトが剣そのもの）。
    // 薙ぐ間だけ、剣先の霞のエフェクトを大剣の上に置いて一緒に動かし、通った所に霞の跡を残す。
    const bool conjured = boss.greatsword.phase != BossGreatswordPhase::None;
    const bool trailing = boss.greatsword.phase == BossGreatswordPhase::Sweep && boss.greatswordTrailAmount > 0.0f;
    // 外形の刀が出ている間は、剣先の霞と光（光の玉）は出さない：刀身の全長に付く刀光・燃える刀身・輪郭の光
    // （BossBodyVisuals の材質の効果）が刀を見せるので、剣先だけが光ると不自然（c-b34a2f）。霞の大剣の薙ぎの跡だけ残す。
    const bool bodyShown = boss.body.visible && !boss.body.model.empty();
    if ((conjured && !trailing) || (bodyShown && !trailing) ||
        (!swinging && !kamae && !lit && !charging && !trailing)) {
        StopHandle(world, boss.swordVfx);
        return;
    }
    const auto sword = BossPose::SwordPoints(boss, boss.swordPose);
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
    // 剣が燃えている間は、霞（煙）を出さずに光だけにする：ふだんの剣は模型が無く見えないので、火の色の霞は、構えでは
    // ボスの上の空中に浮いた塊に、振った後は空に残る弧に見える（斬撃の炎は、斬撃のエフェクトが出す）。
    if (lit && !trailing)
        amount = 0.0f;
    StartOrSend(world, boss, boss.swordVfx, boss.swordEffect, Placed(position, 1.0f),
                Parameters(boss, amount, ember, lit));
}

} // namespace

void BossVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds,
                         float alpha) {
    UpdateChargeGather(world, boss);
    // 天降り斬り・霞の大剣は爆発の枠へ塊・霧の押しを入れるので、枠を出す所より前に進める。
    BossSkyfallVisuals::Update(world, object, boss, seconds);
    BossGreatswordVisuals::Update(world, object, boss, seconds);
    BossFlameRunVisuals::Update(world, object, boss);
    UpdateSlashes(world, object, boss, seconds, alpha);
    UpdateRing(world, boss, seconds);
    PlayBlasts(world, boss);
    UpdateKnives(world, boss, alpha);
    UpdateSwordHaze(world, boss);
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
