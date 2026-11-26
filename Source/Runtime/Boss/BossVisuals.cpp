#include "BossVisuals.h"

#include <algorithm>

#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/VfxView.h"
#include "BossMath.h"
#include "BossPose.h"
#include "BossRules.h"
#include "BossVfx.h"
#include "../BossBody/BossAnimBody.h"
#include "../Burn/BurnField.h"
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

// 当たった所の爆発の光（一度きりのエフェクト。入力は無い）。
void PlayBlasts(HIKARI::WorldView& world, BossComponent& boss) {
    for (auto& blast : boss.blasts) {
        if (!blast.effectPending)
            continue;
        blast.effectPending = false;
        if (blast.playerHit && blast.fire) {
            // 火がプレイヤーに当たった：火球と光の輪は出さない。攻撃の向きへ体を突き抜ける炎（場の火の域の流体）と、
            // 同じ向きへ飛ぶ火花を出す。
            BurnField::AddJet(world, blast.position, blast.direction, blast.scale);
            HIKARI::GameProjectVfxPlayRequest sparks{};
            sparks.asset = boss.fireHitEffect;
            sparks.transform = Placed(blast.position, blast.scale,
                                      BasisRotation(blast.direction, Cross({0.0f, 1.0f, 0.0f}, blast.direction)));
            sparks.loopMode = HIKARI::GameProjectVfxLoopMode::OneShot;
            Start(world, boss, sparks);
            continue;
        }
        // 塊そのものは流体で出す：剣が燃えている技で当てた所は場の火の域の火球、ほかは場の霧へ弾ける霞の塊。
        BurnField::AddBlast(world, blast.position, boss.hitBlastRadius * blast.scale,
                            blast.fire ? BurnBlastKind::Hit : BurnBlastKind::Smoke);
        // その上に重ねるエフェクト（当たった所は光・衝撃の輪・火花）。
        const auto& asset = !blast.fire && !boss.mistBlastEffect.IsEmpty() ? boss.mistBlastEffect : boss.blastEffect;
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
        // 霧の弾（左手の技。c-4be37e）：刃を出さず、尾の霞を弾の大きさにして霧の芯を灯す。
        const bool mist = boss.knifeMistBullet;
        HIKARI::GameProjectVfxPlayRequest tail{};
        tail.asset = boss.swordEffect;
        tail.transform = Placed(wave.position, mist ? boss.knifeMistScale : 1.0f);
        tail.parameters = Parameters(boss, boss.knifeEffectAmount, mist ? boss.knifeMistCore : 0.0f);
        handle = StartMoving(world, boss, tail, velocity);
        if (mist)
            continue;
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
void UpdateIgniteFlash(HIKARI::WorldView& world, BossComponent& boss, BossPhaseShiftComponent* phaseShift, bool lit) {
    // フェーズの変わり目の演出で刀が初めて灯る時（閃光・飛ばした時）は火球を出さない：刀身の火は演出が灯し、飛ばした時は
    // 規則の剣先（構え直す前の所）に火球が残って燃えて見える。
    const bool byShift = lit && phaseShift != nullptr && phaseShift->state.beat != BossShiftBeat::None &&
                         !boss.phaseTwoLookPending && !phaseShift->state.bladeLitSeen;
    if (byShift)
        phaseShift->state.bladeLitSeen = true;
    if (lit && !boss.bladeWasLit && boss.igniteFlashRadius > 0.0f && !byShift) {
        const BossSwordPoints sword = boss.swordSmoke
                                          ? BossSwordPoints{boss.swordInner, boss.swordInner, boss.swordOuter}
                                          : BossPose::SwordPoints(boss, boss.swordPose);
        BurnField::AddBlast(world, sword.outer, boss.igniteFlashRadius, BurnBlastKind::Hit);
    }
    boss.bladeWasLit = lit;
}

// 剣先の霞：構えでは剣先に集まり、振っている間は剣先が弧を描く（動く注入は通った線に沿って広がる）。
void UpdateSwordHaze(HIKARI::WorldView& world, BossComponent& boss, const BossBodyComponent* look,
                     BossPhaseShiftComponent* phaseShift) {
    const bool swinging = boss.swordSmoke;
    const bool kamae = boss.glintFrames > 0 && boss.state == BossState::Act;
    // 火を残す技の間は、前隙の最初から剣が燃えている（技を出す前の 1 枚の画面で見分けられる）。
    const bool lit = BossRules::IsBladeLit(boss);
    UpdateIgniteFlash(world, boss, phaseShift, lit);
    // 骨格の体が出ている間は、剣先の霞と光（光の玉）は出さない：刀身の全長に付く刀光・燃える刀身・輪郭の光
    // （BossBodyAccents）が刀を見せるので、剣先だけが光ると不自然（c-b34a2f）。
    if (BossAnimBody::Showing(look) || (!swinging && !kamae && !lit)) {
        StopHandle(world, boss.swordVfx);
        return;
    }
    const auto sword = BossPose::SwordPoints(boss, boss.swordPose);
    const Vec3 position = swinging ? boss.swordOuter : sword.outer;
    float amount = swinging ? boss.swordEffectAmount : boss.kamaeEffectAmount;
    float ember = lit        ? std::max(boss.bladeLitEmber, boss.swordEmberScale)
                  : swinging ? boss.swordEmberScale
                             : boss.kamaeEmberScale;
    // 剣が燃えている間は、霞（煙）を出さずに光だけにする：ふだんの剣は模型が無く見えないので、火の色の霞は、構えでは
    // ボスの上の空中に浮いた塊に、振った後は空に残る弧に見える（斬撃の炎は、斬撃のエフェクトが出す）。
    if (lit)
        amount = 0.0f;
    StartOrSend(world, boss, boss.swordVfx, boss.swordEffect, Placed(position, 1.0f),
                Parameters(boss, amount, ember, lit));
}

} // namespace

size_t BossVisuals::GetFluidDomainDemand(const BossComponent& boss, const BossBodyComponent* look) {
    size_t live = boss.ringVfx.IsValid() ? 1 : 0;
    for (const auto& effect : boss.slashEffects)
        live += effect.handle.IsValid() ? 1 : 0;
    // 剣を振っている間は、振り抜いた瞬間に出す斬撃の分を先に取っておく（出すフレームに枠が空いているように）。
    // 回し蹴りも、炎の輪を出す前（前隙・攻撃中）から先に取っておく。
    const BossMove* move = BossRules::GetMove(boss, boss.move);
    const bool kicking =
        boss.state == BossState::Act && move != nullptr && move->type == BossMoveType::Kick && !boss.ringVfx.IsValid();
    // 胴の煙は出し続ける域なので、斬撃の枠の上限とは別に数える。
    return std::min(kBossSlashEffectCapacity + 1, live + (boss.swordSmoke ? 1 : 0) + (kicking ? 1 : 0)) +
           (look != nullptr && look->state.smokeVfx.IsValid() ? 1 : 0);
}

void BossVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                         const BossBodyComponent* look, BossPhaseShiftComponent* phaseShift, float seconds,
                         float alpha) {
    UpdateSlashes(world, object, boss, seconds, alpha);
    UpdateRing(world, boss, seconds);
    PlayBlasts(world, boss);
    UpdateKnives(world, boss, alpha);
    UpdateSwordHaze(world, boss, look, phaseShift);
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
}

void BossVisuals::Describe(HIKARI::PropertyFields& fields, BossComponent& boss) {
    if (fields.Section("見た目（霞の色とエフェクト）")) {
        fields.Vfx("slashEffect", "飛ぶ斬撃のエフェクト（三日月の霧。自前の流体域を持つ）", boss.slashEffect);
        fields.Float("slashEffectWidth", "斬撃のエフェクトが作られている幅 (m。判定サイズ ÷ これ が大きさの倍率)",
                     boss.slashEffectWidth, 0.5f, 50.0f);
        fields.Float("slashFadeSeconds", "斬撃が消えた後、霧が消えるまで飛ばし続ける時間 (秒)", boss.slashFadeSeconds,
                     0.0f, 3.0f);
        fields.Float("slashImpactSeconds", "当たった瞬間に刃が砕けて散る時間 (秒)", boss.slashImpactSeconds, 0.0f,
                     2.0f);
        fields.Float("slashHitSpeedScale", "当たった後の霧が進む速さ（元の速さに対する割合。小さいほどその場で散る）",
                     boss.slashHitSpeedScale, 0.0f, 1.0f);
        fields.Float("slashGlowFadeSeconds", "刃が消える・当たる時に、光る刃口や炎の層が縮んで消えるまでの時間 (秒)",
                     boss.slashGlowFadeSeconds, 0.0f, 1.0f);
        fields.Note(
            "斬撃の霧の濃さ・色・光り方・尾の長さは、エフェクト BossSlashWave の Blade Domain と各層で調整します。"
            "同時に見せる斬撃は 2 つまで（流体域の枠）で、その間は砲撃の炎の枠が同じ数だけ減ります。");
        fields.Vfx("mistBlastEffect", "霞の刃が当たった所のエフェクト（青白い光と火花。空なら炎の刃と同じ）",
                   boss.mistBlastEffect);
        fields.Float("wavePartPush", "飛ぶ斬撃が、真下の場の霧を左右へ割る速さ (m/s。0 で割らない)", boss.wavePartPush,
                     0.0f, 40.0f);
        fields.Float("wavePartRadius", "…割る押しの半径 (m)", boss.wavePartRadius, 0.1f, 6.0f);
        fields.Vfx("slashSmokeEffect", "火を残さない斬撃のエフェクト（煙の刃。空なら炎の刃で出す）",
                   boss.slashSmokeEffect);
        fields.Float("bladeLitEmber", "火を残す技の間の、剣先の熾火の強さ (倍)", boss.bladeLitEmber, 0.0f, 20.0f);
        fields.Color3("bladeLitColor", "…剣先の霞の色", boss.bladeLitColor);
        fields.Float("igniteFlashRadius", "剣に火が付く瞬間の火球の半径 (m。0 は出さない)", boss.igniteFlashRadius,
                     0.0f, 5.0f);
        fields.Float("hitBlastRadius", "当たった所の火球・煙の塊の半径 (m)", boss.hitBlastRadius, 0.1f, 10.0f);
        fields.Note("火を残す技（技の表の「残す火」）は、出す前から剣が燃え、斬撃は炎の刃になります。"
                    "残さない技は、剣は霞のまま、斬撃は暗い煙の刃です。"
                    "剣の炎・当たった所の火球・煙の塊は、場の火の域（ArenaFireField）の流体です。");
        fields.Vfx("blastEffect", "当たった所の爆発のエフェクト（光・火花・爆炎）", boss.blastEffect);
        fields.Vfx(
            "fireHitEffect",
            "火の刃がプレイヤーに当たった所の火花（攻撃の向きへ飛ぶ。体を突き抜ける炎は BurnField の jet の項目）",
            boss.fireHitEffect);
        fields.Float("slashHitFadeSeconds", "当たった斬撃の炎を消すまでの時間 (秒。爆発の光に隠してすぐ消す)",
                     boss.slashHitFadeSeconds, 0.0f, 1.0f);
        fields.Float("blastPush", "爆発が足元の霧を外へ押す速さ (m/秒、0 は無効)", boss.blastPush, 0.0f, 100.0f);
        fields.Float("blastPushRadius", "爆発が霧を押す半径 (m)", boss.blastPushRadius, 0.1f, 50.0f);
        fields.Float("knifeBlastScale", "ナイフが当たった時の爆発の大きさ（斬撃に対する倍率）", boss.knifeBlastScale,
                     0.0f, 2.0f);
        fields.Note(
            "爆発は、斬撃・ナイフ・剣がプレイヤーに当たった所に出ます。霧は地面に沿って外へ押すだけで、煙は足しません"
            "（場の霧はほとんど消えないので、足すとプレイヤーの周りに溜まって画面を塞ぎます）。"
            "光・火花・爆炎の形は BossSlashBurst で調整します。");
        fields.Vfx("swordEffect", "剣先の霞・ナイフの尾のエフェクト（流体注入）", boss.swordEffect);
        fields.Color3("hazeColor", "剣先の霞・ナイフの尾の色（霞の刃と同じ青白。火を残す技の間は bladeLitColor）",
                      boss.hazeColor);
        fields.Float("swordEffectAmount", "振っている剣の霞の濃さ（倍）", boss.swordEffectAmount, 0.0f, 20.0f);
        fields.Float("kamaeEffectAmount", "構えで剣先に集まる霞の濃さ（倍）", boss.kamaeEffectAmount, 0.0f, 20.0f);
        fields.Float("swordEmberScale", "振っている剣先の火の粉の光の大きさ（倍、0 は出さない）", boss.swordEmberScale,
                     0.0f, 10.0f);
        fields.Float("kamaeEmberScale", "構えで剣先に灯る火の粉の光の大きさ（倍）", boss.kamaeEmberScale, 0.0f, 10.0f);
        fields.Note(
            "剣先の霞（煙）は場の霧に入って残ります。濃くすると技のたびにボスの頭上へ溜まり、長く戦うと視界を塞ぎます。"
            "構えを目立たせたい時は、煙ではなく火の粉の光を大きくします。");
        fields.Float("knifeEffectAmount", "ナイフの尾の霞の濃さ（倍）", boss.knifeEffectAmount, 0.0f, 20.0f);
        fields.Vfx("knifeEffect", "投げナイフのエフェクト（刃と光点）", boss.knifeEffect);
        fields.Bool("knifeMistBullet",
                    "投げナイフを霧の弾にする（刃を出さず、尾の霞を弾の大きさで、霧の芯を灯して飛ばす）",
                    boss.knifeMistBullet);
        fields.Float("knifeMistScale", "…霧の弾の大きさ（尾の霞の倍率）", boss.knifeMistScale, 0.1f, 10.0f);
        fields.Float("knifeMistCore", "…霧の弾の芯の明るさ（尾の霞のエフェクトの霧の芯）", boss.knifeMistCore, 0.0f,
                     10.0f);
        fields.Float("swingPushForce",
                     "振りで場の霧を揺らす力（m/s²。当たりの窓の間、刃が進む向きへ押す。0 は押さない）",
                     boss.swingPushForce, 0.0f, 400.0f);
        fields.Float("swingPushRadius", "…力が届く半径 (m)", boss.swingPushRadius, 0.1f, 10.0f);
        fields.Vfx("ringEffect", "回し蹴りの霞の輪のエフェクト（広がる炎の輪。自前の流体域を持つ）", boss.ringEffect);
        fields.Float("ringEffectRadius", "…エフェクトが作られている半径 (m。輪の半径 ÷ これ が大きさの倍率)",
                     boss.ringEffectRadius, 0.5f, 50.0f);
        fields.Float("ringFadeSeconds", "…輪が消えた後、炎が消えるまで残す時間 (秒)", boss.ringFadeSeconds, 0.0f, 3.0f);
        fields.Float("ringHeight", "…輪の高さ（地面から m）", boss.ringHeight, 0.0f, 10.0f);
        fields.Note("炎の輪は流体域を 1 つ使います。回し蹴りの間は、砲撃の炎の枠がその分だけ減ります。"
                    "炎・煙の濃さや色は、エフェクト BossKickRing の Fire Domain と各層で調整します。");
        fields.Float("ringBurstScale", "回し蹴りの足元の爆ぜの大きさ（当たった所の爆発の倍率）", boss.ringBurstScale,
                     0.0f, 5.0f);
        fields.Float("ringPush", "霞の輪が足元の霧を外へ押す速さ (m/秒、0 は押さない)", boss.ringPush, 0.0f, 100.0f);
        fields.Float("ringPushRadius", "…押す 1 点の半径 (m)", boss.ringPushRadius, 0.1f, 20.0f);
        fields.Note(
            "飛ぶ斬撃が通った跡の残り煙は、エフェクトではなく場の霧の域へ直接入れます（Emit。域は増やしません）。");
    }
}

} // namespace Project_TD2_2
