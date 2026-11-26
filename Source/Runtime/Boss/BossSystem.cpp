#include "BossSystem.h"

#include <algorithm>
#include <cstdio>

#include "HIKARI/Project/GameProjectSession.h"
#include "HIKARI/Runtime/ComponentTypeId.h"
#include "BossFeedback.h"
#include "BossGourdLink.h"
#include "BossHealth.h"
#include "BossHealthBars.h"
#include "BossIntro.h"
#include "BossIntroComponent.h"
#include "BossLockTarget.h"
#include "BossMath.h"
#include "BossMoveExecutor.h"
#include "BossObserver.h"
#include "BossPhaseShift.h"
#include "BossPhaseShiftComponent.h"
#include "BossPose.h"
#include "BossReachProbe.h"
#include "BossSmoke.h"
#include "BossStateMachine.h"
#include "BossTableLoader.h"
#include "BossTransform.h"
#include "BossVisuals.h"
#include "BossWaves.h"
#include "../Common/ComponentLookup.h"
#include "../Common/GameSession.h"
#include "../Common/SessionMemory.h"
#include "../Common/TraceInterval.h"
#include "../BossBody/BossBodyComponent.h"
#include "../BossBody/BossBodyVisuals.h"
#include "../BossBody/BossCinematicFx.h"
#include "../BossBody/BossIntroActor.h"
#include "../BossBody/BossIntroCamera.h"
#include "../BossBody/BossIntroEffects.h"
#include "../BossBody/BossPhaseTwoField.h"
#include "../Combat/Damageable.h"
#include "../Player/PlayerRules.h"

namespace Project_TD2_2 {
namespace {

// 登場の演出の間、プレイヤーの入力を止めておく長さ（毎 tick 入れ直す。tick の順がずれても途切れない）。
constexpr int kCinematicHoldTicks = 2;
// 体がこの高さ（m）より上にある間は、通った跡の煙を地面に残さない。
constexpr float kTrailMaxHeight = 0.5f;

using ComponentLookup::ForEachOnObject;
using ComponentLookup::WithFirstOnObject;
using GameSession::IsPlaying;
using namespace BossMath;

// 同じ物体の見た目の部品：登場・変わり目の演出・体の見た目（無ければ nullptr：その見た目を出さない）。
struct BossLookParts {
    BossIntroComponent* intro = nullptr;
    BossPhaseShiftComponent* shift = nullptr;
    BossBodyComponent* body = nullptr;
};

// 同じ物体の見た目の部品を集めて callback(const BossLookParts&) に渡す（参照はクエリのコールバックの中だけで使う）。
template <class Callback>
void WithLookParts(HIKARI::WorldView& world, HIKARI::ObjectView object, Callback&& callback) {
    WithFirstOnObject<BossIntroComponent>(world, kBossIntroType, object, [&](BossIntroComponent* intro) {
        WithFirstOnObject<BossPhaseShiftComponent>(
            world, kBossPhaseShiftType, object, [&](BossPhaseShiftComponent* shift) {
                WithFirstOnObject<BossBodyComponent>(world, kBossBodyType, object, [&](BossBodyComponent* body) {
                    callback(BossLookParts{intro, shift, body});
                });
            });
    });
}

void Initialize(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, BossIntroComponent* intro) {
    HIKARI::GameProjectTransform transform{};
    object.GetWorldTransform(transform);
    boss.ground = {transform.position.x, transform.position.y - boss.bodyHalfHeight, transform.position.z};
    const Vec3 forward = Rotate(transform.rotation, {0.0f, 0.0f, 1.0f});
    boss.yaw = YawOf(forward);
    boss.random = static_cast<uint32_t>(boss.randomSeed) * 2654435761u + 1u;
    boss.floatHeight = boss.floatTarget = std::clamp(0.5f, boss.floatMin, std::max(boss.floatMin, boss.floatMax));
    boss.lastBodyPosition = boss.ground;
    boss.state = BossState::Idle;
    boss.initialized = true;
    // 登場（同じ物体の登場の部品。無ければ登場をしない）は Play の間に 1 度だけ：やり直し（シーンの読み直し）の後は、
    // Play session が覚えている。
    if (intro != nullptr) {
        auto* memory = dynamic_cast<ISessionMemory*>(world.GetGameSession());
        BossIntro::Begin(boss, *intro, memory != nullptr && memory->GetMemory().bossIntroSeen);
        // 役者（骨格の動画で演じる体）を探す。いなければ登場をしない。登場をしない時は、役者と落ちている部品を隠す。
        if (!BossIntroActor::Find(world, *intro))
            BossIntro::Cancel(*intro);
        if (!BossIntro::Holding(intro))
            BossIntroActor::Hide(world, *intro);
    }
    BossTableLoader::Load(world, boss);
}

void Trace(const BossComponent& boss, const Damageable* health) {
    if (!TraceInterval::Due(boss.traceEveryFrames, boss.tick))
        return;
    // 規則の剣の構え（握り・向き）。姿勢の切り替えでどれだけ跳ぶかを tick ごとに見るための確かめ用。
    const BossSwordPose& sword = boss.swordPose;
    char message[640]{};
    std::snprintf(
        message, sizeof(message),
        "Trace tick=%llu state=%d phase=%d move=%d stateFrame=%d ground=(%.3f,%.3f,%.3f) yaw=%.3f "
        "height=%.3f float=%.3f stun=%.3f stunCount=%d health=%.3f fluid=%d "
        "grip=(%.3f,%.3f,%.3f) sword=(%.4f,%.4f,%.4f) "
        "bossPhase=%d shiftLeft=%d maxHealth=%.3f invulnerable=%d",
        static_cast<unsigned long long>(boss.tick), static_cast<int>(boss.state), static_cast<int>(boss.phase),
        boss.move, boss.stateFrame, static_cast<double>(boss.ground.x), static_cast<double>(boss.ground.y),
        static_cast<double>(boss.ground.z), static_cast<double>(boss.yaw), static_cast<double>(boss.height),
        static_cast<double>(boss.floatHeight), static_cast<double>(boss.stun), boss.stunCount,
        health ? static_cast<double>(health->Health()) : -1.0, static_cast<int>(boss.fluidAccepted),
        static_cast<double>(sword.grip.x), static_cast<double>(sword.grip.y), static_cast<double>(sword.grip.z),
        static_cast<double>(sword.direction.x), static_cast<double>(sword.direction.y),
        static_cast<double>(sword.direction.z), boss.lastPhaseNumber, boss.phaseShiftLeft,
        health ? static_cast<double>(health->maxHealth) : -1.0,
        health ? static_cast<int>(health->invulnerable) : -1);
    BossLog::Write(message);
}

// ボス 1 体の 1 tick：プレイヤーを見る → 登場・HP の条・変わり目を進める → 状態を進める → 受けた当たりを読む →
// 通った跡の煙 → 斬撃 → 規則の剣の向き → 体の物体の置き場所。
void StepBoss(BossFrame& frame, HIKARI::ObjectView object, Damageable* health, const BossGearSight& gear,
              IBossBrain& brain) {
    HIKARI::WorldView& world = frame.world;
    BossComponent& boss = frame.boss;
    BossIntroComponent* intro = frame.intro;
    Player* player = frame.player;
    const Vec3 playerCenter = frame.playerCenter;
    BossObserver::Observe(world, boss, player, playerCenter, gear, frame.dt);
    // プレイヤーの位置（見た目だけが読む：近い時は胴の煙を薄くする。規則は使わない）。
    boss.lookTarget = playerCenter;
    boss.lookTargetSeen = player != nullptr;
    if (intro != nullptr) {
        if (player != nullptr)
            BossIntro::Face(boss, *intro, playerCenter);
        BossIntro::Step(world, boss, *intro);
    }
    BossHealthBars::Step(boss);
    BossPhaseShift::Step(boss, frame.shift);
    // 演出の間（登場・フェーズの変わり目）は、プレイヤーの入力を止めて無敵にする（毎 tick 入れ直す。
    // PlayerSystem が tick ごとに減らす）。変わり目の閃光の後の tick に、プレイヤーを押し出す。
    if (player != nullptr && (BossIntro::Cinematic(intro) || BossPhaseShift::Cinematic(frame.shift)))
        PlayerRules::HoldForCinematic(*player, kCinematicHoldTicks);
    if (player != nullptr)
        BossPhaseShift::Push(boss, frame.shift, *player, playerCenter);
    boss.shownBar = BossIntro::BarShown(intro) ? 1.0f : 0.0f;
    boss.shownCinematic = BossIntro::Cinematic(intro) ? 1.0f : 0.0f;
    if (BossIntro::Started(intro)) {
        if (auto* memory = dynamic_cast<ISessionMemory*>(world.GetGameSession()))
            memory->GetMemory().bossIntroSeen = true;
    }
    BossMoveExecutor::StepFloat(boss);
    BossBrainContext context{boss, object.GetHandle()};
    if (player != nullptr)
        context.playerDistance = FlatLength(Sub(playerCenter, boss.ground));
    context.phaseNumber = boss.lastPhaseNumber;
    // 「HP60%以下」などは 2 本の条の合計に対する残り（1 本目 600・2 本目 900 なら 60% ちょうどでフェーズ 2）。
    if (health != nullptr && boss.healthBarsReady)
        context.healthPercent = BossHealthBars::TotalPercent(boss, *health);
    // この tick に読む命中は、前の tick の終わり（＝この tick の始め）の状態の間に当たったもの。後隙だったなら
    // その技が咎められた（体を動かすと後隙が終わって待機へ戻ることがあるので、先に覚える）。
    const int recoveryRow = boss.state == BossState::Act && boss.phase == BossActPhase::Post ? boss.move : -1;
    // 狙いのずれのねじりは、状態を進める（刀の当たりを見る）前に決める（この tick の当たりと見た目を同じ向きにする）。
    BossMoveExecutor::StepAimTwist(frame);
    BossStateMachine::Step(frame, brain, context);
    // 受けたダメージ（気絶値・フェーズ・撃破）は体を動かした後に読む（試作と同じ順で、気絶の長さが 1 F ずれない）。
    if (health != nullptr)
        BossObserver::NotePunished(boss, recoveryRow, BossHealth::Sync(frame, object, *health));
    // ボスが動いた跡にも煙が残る（bossResidueStep ごとに 1 回ぶん。吸い込める）。
    // 空中を動いた分は数えない。
    if (boss.height < kTrailMaxHeight)
        boss.bodyTrail += FlatLength(Sub(boss.ground, boss.lastBodyPosition));
    boss.lastBodyPosition = boss.ground;
    if (boss.bodyTrail >= boss.bossResidueStep) {
        boss.bodyTrail = 0.0f;
        boss.bodySmokeRate = boss.bossResidue / frame.dt;
    }
    BossWaves::Step(frame);
    BossWaves::StepDeposits(boss);
    // 規則の剣の向きは、この tick の状態が決まった後に 1 度だけ求める（見た目・記録が読む）。
    // 状態を進めた直後ではなくここに置く：被弾の処理（気絶・撃破）が状態を変えうるので、それを済ませた向きにする。
    boss.swordPose = BossPose::Target(boss);
    BossTransform::Apply(object, boss, intro);
    Trace(boss, health);
    BossReachProbe::Step(frame);
}

} // namespace

void BossSystem::OnWorldAttached(HIKARI::WorldView&) { treeBrain_.Attach(); }

void BossSystem::FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame) || frame.fixedDt <= 0.0f)
        return;
    // プレイヤーのヒョウタンと砲撃を見る（吸い込み中か・捕まえられる空き・口・ボスが見る道具）。
    BossGourdSight sight = BossGourdLink::Sense(world);
    BossCatchHandoff caught;
    // 口へ引き込んでいる途中の斬撃・ナイフの数（ある間は、ヒョウタンが口の吸う力と見た目を保つ）。
    int pulling = 0;
    world.ForEachComponent<BossComponent>(HIKARI::MakeComponentTypeId(kBossType), [&](HIKARI::ObjectView object,
                                                                                      BossComponent& boss) {
        // 同じ物体の登場・変わり目の演出の部品（無ければ nullptr：その演出をしない）。演出の規則は、ボスの tick の中で
        // 前と同じ順に呼ぶ。
        WithFirstOnObject<BossIntroComponent>(world, kBossIntroType, object, [&](BossIntroComponent* intro) {
            WithFirstOnObject<
                BossPhaseShiftComponent>(world, kBossPhaseShiftType, object, [&](BossPhaseShiftComponent* shift) {
                if (!boss.initialized)
                    Initialize(world, object, boss, intro);
                // 技のクリップの刀の骨（c-7b1a09）を、技の規則が使う前に読んでおく（クリップごとに初めての時だけ）。
                BossAnimBody::PrepareSword(world, boss);
                BossAnimBody::CheckAimFingerprints(boss);
                ++boss.tick;
                boss.clockSeconds += frame.fixedDt;
                boss.floatClock += frame.fixedDt;
                boss.swordSmoke = false;
                boss.bodySmokeRate = 0.0f;
                boss.glintFrames = std::max(0, boss.glintFrames - 1);
                IBossBrain& brain = BossTestBrain::Wants(boss) ? static_cast<IBossBrain&>(testBrain_) : treeBrain_;
                // 参照はクエリのコールバックの中だけで使うので、プレイヤー → 同じ物体の Damageable の順に入れ子で処理する。
                auto withHealth = [&](Player* player, Vec3 playerCenter) {
                    BossFrame bossFrame{world,         boss,           player,           playerCenter,
                                        frame.fixedDt, sight.inhaling, &sight.catchRoom, sight.mouth,
                                        sight.hasMouth};
                    bossFrame.intro = intro;
                    bossFrame.shift = shift;
                    StepBoss(bossFrame, object, Damageable::Of(object.GetHandle().value), sight.gear, brain);
                };
                // プレイヤーは 1 人。いなくても体と斬撃は動く（当たる相手がいないだけ）。
                bool ran = false;
                ComponentLookup::ForEachGame<Player>([&](HIKARI::ObjectView playerObject, Player& player) {
                    HIKARI::GameProjectTransform transform{};
                    if (ran || !playerObject.GetWorldTransform(transform))
                        return;
                    ran = true;
                    withHealth(&player, transform.position);
                    BossLockTarget::Step(object, boss, player);
                });
                if (!ran)
                    withHealth(nullptr, {});
                // 吸い取った斬撃・ナイフの渡し分を、ボスごとに集めて空にする（渡すのは下でまとめて 1 回）。
                caught.smoke += boss.caught.smoke;
                caught.count += boss.caught.count;
                caught.fireCount += boss.caught.fireCount;
                boss.caught = {};
                for (const auto& wave : boss.waves)
                    pulling += wave.active && wave.captured ? 1 : 0;
            });
        });
    });
    BossGourdLink::HandOff(world, caught, pulling);
}

void BossSystem::Update(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (world.GetGameSession() == nullptr || !IsPlaying(world, frame))
        return;
    // 演出を飛ばす入力を覚える（c-164cab：入力は Update で読み、固定 tick の BossIntro・BossPhaseShift が使う）。
    world.ForEachComponent<BossComponent>(
        HIKARI::MakeComponentTypeId(kBossType), [&](HIKARI::ObjectView object, BossComponent& boss) {
            if (!boss.initialized)
                return;
            // 登場の演出（同じ物体の登場の部品）。
            WithFirstOnObject<BossIntroComponent>(world, kBossIntroType, object, [&](BossIntroComponent* intro) {
                if (BossIntro::Cinematic(intro) && !intro->settings.skipAction.empty() &&
                    GameSession::ReadAction(world.GetInput(), intro->settings.skipAction).pressed)
                    intro->state.skipRequested = true;
            });
            // フェーズの変わり目の演出（同じ物体の変わり目の演出の部品。項目で飛ばせなくできる）。
            WithFirstOnObject<BossPhaseShiftComponent>(
                world, kBossPhaseShiftType, object, [&](BossPhaseShiftComponent* shift) {
                    if (BossPhaseShift::Cinematic(shift) && shift->settings.skippable &&
                        !shift->settings.skipAction.empty() &&
                        GameSession::ReadAction(world.GetInput(), shift->settings.skipAction).pressed)
                        shift->state.skipRequested = true;
                });
        });
}

void BossSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    // 編集中（Play の外）：登場の演出の資産のプレビューだけ（c-e0c500。ほかの見た目・規則は動かさない）。
    if (world.GetGameSession() == nullptr) {
        BossBodyVisuals::UpdatePreviews(world);
        return;
    }
    if (!IsPlaying(world, frame))
        return;
    size_t demand = 0;
    world.ForEachComponent<BossComponent>(
        HIKARI::MakeComponentTypeId(kBossType), [&](HIKARI::ObjectView object, BossComponent& boss) {
            if (!boss.initialized)
                return;
            WithLookParts(world, object, [&](const BossLookParts& parts) {
                BossVisuals::Update(world, object, boss, parts.body, parts.shift, frame.gameDt,
                                    frame.fixedInterpolationAlpha);
                BossBodyVisuals::Update(world, object, boss, parts.body, parts.intro, parts.shift, frame.gameDt,
                                        frame.fixedInterpolationAlpha);
                // フェーズ 2 の場の色・火の粉・焦げた輪（体を出さないフレームも進める。c-857ae3）。
                if (parts.body != nullptr)
                    BossPhaseTwoField::Update(world, boss, *parts.body, frame.gameDt);
                BossFeedback::Update(world, object, boss, frame);
                // 演出の効果の手応え：登場だけの分（衝撃の輪・星・見得の張りつめを口へ書く）→ 登場と変わり目の演出の共通の分。
                if (parts.intro != nullptr)
                    BossIntroEffects::Feedback(world, object, boss, BossBodySettingsOf(parts.body), *parts.intro,
                                               frame.unscaledDt);
                BossCinematicFx::Feedback(world, object, boss, frame.unscaledDt);
                // 登場の演出のカメラ（プレイヤーのカメラへ渡す）。
                if (parts.intro != nullptr)
                    ComponentLookup::ForEachGame<Player>(
                        [&](HIKARI::ObjectView, Player& player) { BossIntroCamera::Update(*parts.intro, player); });
                demand += BossVisuals::GetFluidDomainDemand(boss, parts.body);
            });
        });
    // 斬撃のエフェクトが使う流体域の分だけ、砲撃の炎の枠を譲ってもらう（毎フレーム入れ直す）。
    BossGourdLink::ReserveFluidDomains(world, demand);
}

void BossSystem::PreRender(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    const float smokePerVolume = BossGourdLink::SmokePerVolume(world);
    world.ForEachComponent<BossComponent>(
        HIKARI::MakeComponentTypeId(kBossType), [&](HIKARI::ObjectView object, BossComponent& boss) {
            if (!boss.initialized)
                return;
            WithLookParts(world, object, [&](const BossLookParts& parts) {
                BossSmoke::Render(world, object, boss, parts.body, smokePerVolume);
                // 登場の黒い煙・落ちている兜の瞬き（同じ物体の登場の部品）。
                if (parts.intro != nullptr)
                    BossIntroEffects::Render(world, object, boss, BossBodySettingsOf(parts.body), *parts.intro,
                                             frame.gameDt);
            });
        });
}

void BossSystem::OnWorldDetached(HIKARI::WorldView& world) {
    world.ForEachComponent<BossComponent>(
        HIKARI::MakeComponentTypeId(kBossType), [&](HIKARI::ObjectView object, BossComponent& boss) {
            BossVisuals::Stop(world, boss);
            WithLookParts(world, object, [&](const BossLookParts& parts) {
                BossBodyVisuals::Stop(world, boss, parts.body, parts.shift);
                if (parts.body != nullptr)
                    BossPhaseTwoField::Stop(world, *parts.body);
            });
            BossWaves::Clear(boss);
        });
}

} // namespace Project_TD2_2
