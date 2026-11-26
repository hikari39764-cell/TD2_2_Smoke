#include "FlowSystem.h"

#include <cmath>
#include <cstdio>

#include "HIKARI/Project/GameFlowViews.h"
#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Runtime/ComponentTypeId.h"

#include "../Common/ComponentLookup.h"
#include "../Common/GameSession.h"
#include "../Boss/BossRules.h"
#include "../Common/ObjectVariables.h"
#include "../Gourd/GourdComponent.h"
#include "../Gourd/GourdRules.h"
#include "../Player/Player.h"
#include "../Player/PlayerRules.h"
#include "FlowComponent.h"
#include "FlowHud.h"
#include "FlowRules.h"

namespace Project_TD2_2 {
namespace {

// 世界変数の書き手の名前。
constexpr std::string_view kWriterId = "Project_TD2_2.Flow";

using GameSession::IsPlaying;

void Log(HIKARI::GameProjectLogLevel level, const char* message) { HIKARI::WriteProjectLog(level, "Flow", message); }

const char* PhaseName(FlowPhase phase) {
    switch (phase) {
    case FlowPhase::Waiting: return "Waiting";
    case FlowPhase::Fighting: return "Fighting";
    case FlowPhase::Lost: return "Lost";
    case FlowPhase::Won: return "Won";
    }
    return "Unknown";
}

// 戦いの登場人物の物体。見つからなければ無効のまま。
struct Actors {
    HIKARI::GameProjectObjectHandle player{};
    HIKARI::GameProjectObjectHandle boss{};
    HIKARI::GameProjectObjectHandle gourd{};
    // 火弾の長さ（HUD の残り時間の条の分母）。物体変数は残り秒だけなので、長さは部品の項目から読む。
    float fireShotDuration = 0.0f;
};

Actors FindActors(HIKARI::WorldView& world) {
    Actors actors;
    ComponentLookup::ForEachGame<Player>([&](HIKARI::ObjectView object, Player&) {
        if (actors.player.value == 0)
            actors.player = object.GetHandle();
    });
    ComponentLookup::ForEachGame<Gourd>([&](HIKARI::ObjectView object, Gourd& gourd) {
        if (actors.gourd.value != 0)
            return;
        actors.gourd = object.GetHandle();
        actors.fireShotDuration = GourdRules::GetFireShotDuration(gourd);
    });
    world.ForEachComponent<BossComponent>(HIKARI::MakeComponentTypeId(kBossType),
                                          [&](HIKARI::ObjectView object, BossComponent&) {
                                              if (actors.boss.value == 0)
                                                  actors.boss = object.GetHandle();
                                          });
    return actors;
}

bool GetPosition(const HIKARI::WorldView& world, HIKARI::GameProjectObjectHandle handle,
                 HIKARI::GameProjectFloat3& out) {
    HIKARI::GameProjectTransform transform{};
    if (handle.value == 0 || !world.FindObject(handle).GetWorldTransform(transform))
        return false;
    out = transform.position;
    return true;
}

// 勝ち負けは固定 tick の中で、プレイヤーとボスの規則の値（Rules）から読む（c-164cab。部品が描画のフレームごとに出す
// 物体変数は HUD だけが読む）。
FlowObservation Observe(HIKARI::WorldView& world, const Actors& actors) {
    FlowObservation observation;
    HIKARI::GameProjectFloat3 playerPosition{};
    if (GetPosition(world, actors.player, playerPosition)) {
        observation.playerPresent = true;
        observation.playerHeight = playerPosition.y;
        ComponentLookup::ForEachOnObject<Player>(world, kPlayerType, world.FindObject(actors.player),
                                                 [&](HIKARI::ObjectView, Player& player) {
                                                     const PlayerState state = PlayerRules::GetState(player);
                                                     observation.playerDead = state == PlayerState::Dead;
                                                     observation.playerRiding = state == PlayerState::Ride;
                                                 });
    }
    HIKARI::GameProjectFloat3 bossPosition{};
    if (GetPosition(world, actors.boss, bossPosition)) {
        ComponentLookup::ForEachOnObject<BossComponent>(
            world, kBossType, world.FindObject(actors.boss), [&](HIKARI::ObjectView, BossComponent& boss) {
                observation.bossPresent = true;
                observation.bossHealth = BossRules::GetShownHealth(boss);
                observation.bossMaxHealth = BossRules::GetShownMaxHealth(boss);
            });
        if (observation.playerPresent)
            observation.distanceToBoss =
                std::hypot(playerPosition.x - bossPosition.x, playerPosition.z - bossPosition.z);
    }
    return observation;
}

void PublishPhase(HIKARI::WorldView& world, FlowComponent& flow) {
    HIKARI::GameProjectBindingValue value{};
    value.type = HIKARI::GameProjectBindingValueType::Int;
    value.integer = static_cast<int64_t>(flow.phase);
    if (world.WriteVariable(kFightStateVariable, value, kWriterId) || flow.variableWriteWarned)
        return;
    flow.variableWriteWarned = true;
    Log(HIKARI::GameProjectLogLevel::Warning,
        "世界変数 Fight.State を書き込めません。ProjectSettings/runtime_bindings.json の宣言（ID と型）を確認してください。");
}

// 戦いが始まるまで、ボスの行動を止める。
void HoldBoss(HIKARI::WorldView& world, bool hold) {
    world.ForEachComponent<BossComponent>(
        HIKARI::MakeComponentTypeId(kBossType),
        [&](HIKARI::ObjectView, BossComponent& boss) { BossRules::SetHeld(boss, hold); });
}

void LogChange(const FlowComponent& flow, FlowPhase from, const char* reason, const FlowObservation& observation) {
    char message[256]{};
    std::snprintf(message, sizeof(message),
                  "Phase tick=%llu from=%s to=%s reason=%s distance=%.2f playerHeight=%.2f bossHealth=%.1f",
                  static_cast<unsigned long long>(flow.tick), PhaseName(from), PhaseName(flow.phase), reason,
                  static_cast<double>(observation.distanceToBoss), static_cast<double>(observation.playerHeight),
                  static_cast<double>(observation.bossHealth));
    Log(HIKARI::GameProjectLogLevel::Info, message);
}

// いまのシーンを読み直す（シーンの切り替えの正式の手段）。受け付けられるまで毎 tick 頼む。
void RequestRestart(HIKARI::WorldView& world, FlowComponent& flow) {
    char guid[64]{};
    uint32_t length = 0;
    const HIKARI::SceneFlowView sceneFlow = world.GetSceneFlow();
    if (sceneFlow.GetCurrentSceneAssetGuid(guid, sizeof(guid), length) &&
        sceneFlow.RequestScene({guid, length}, {}, {}, 0, 0)) {
        flow.restartRequested = true;
        char message[128]{};
        std::snprintf(message, sizeof(message), "Restart tick=%llu scene=%s",
                      static_cast<unsigned long long>(flow.tick), guid);
        Log(HIKARI::GameProjectLogLevel::Info, message);
        return;
    }
    if (flow.restartRejectedLogged)
        return;
    flow.restartRejectedLogged = true;
    Log(HIKARI::GameProjectLogLevel::Warning, "シーンの読み直しが受け付けられません。受け付けられるまで頼み直します");
}

FlowHudValues ReadHudValues(const HIKARI::WorldView& world, const Actors& actors) {
    FlowHudValues values;
    // プレイヤーとヒョウタンの値は新しいゲーム層の部品がゲーム全体の値（Params）へ書く。ボスは物体変数のまま。
    values.playerHealth = HIKARI::Params::GetFloat(kPlayerHealthVariable, values.playerHealth);
    values.playerMaxHealth = HIKARI::Params::GetFloat(kPlayerMaxHealthVariable, values.playerMaxHealth);
    values.playerStamina = HIKARI::Params::GetFloat(kPlayerStaminaVariable, values.playerStamina);
    values.playerMaxStamina = HIKARI::Params::GetFloat(kPlayerMaxStaminaVariable, values.playerMaxStamina);
    values.healCharges = HIKARI::Params::GetInt(kPlayerHealChargesVariable, values.healCharges);
    values.playerBurn = HIKARI::Params::GetFloat(kPlayerBurnVariable, values.playerBurn);
    values.gourdSmoke = HIKARI::Params::GetFloat(kGourdSmokeVariable, values.gourdSmoke);
    values.gourdCapacity = HIKARI::Params::GetFloat(kGourdCapacityVariable, values.gourdCapacity);
    values.fireShotSeconds = HIKARI::Params::GetFloat(kGourdFireShotVariable, values.fireShotSeconds);
    values.gourdBlocked = HIKARI::Params::GetFloat(kGourdBlockedVariable, values.gourdBlocked);
    values.fireShotDuration = actors.fireShotDuration;
    ObjectVariables::ReadFloat(world, actors.boss, kBossHealthVariable, values.bossHealth);
    ObjectVariables::ReadFloat(world, actors.boss, kBossMaxHealthVariable, values.bossMaxHealth);
    int64_t bossPhase = 1;
    if (ObjectVariables::ReadInt(world, actors.boss, kBossPhaseVariable, bossPhase))
        values.bossPhase = static_cast<int>(bossPhase);
    ObjectVariables::ReadFloat(world, actors.boss, kBossPhaseSecondsVariable, values.bossPhaseSeconds);
    return values;
}

void StepFlow(HIKARI::WorldView& world, const HIKARI::FrameContext& frame, FlowComponent& flow) {
    const Actors actors = FindActors(world);
    // プレイヤーとボスの両方がいる場面だけで流れを進める。
    if (actors.player.value == 0 || actors.boss.value == 0)
        return;
    const FlowObservation observation = Observe(world, actors);
    const FlowPhase before = flow.phase;
    const char* reason = FlowRules::Step(flow, observation);
    if (reason != nullptr)
        LogChange(flow, before, reason, observation);
    if (reason != nullptr || flow.tick == 1)
        PublishPhase(world, flow);
    HoldBoss(world, flow.phase == FlowPhase::Waiting);
    if (!flow.restartRequested && FlowRules::IsRestartDue(flow, frame.fixedDt))
        RequestRestart(world, flow);
}

void DrawFlow(HIKARI::WorldView& world, const HIKARI::FrameContext& frame, FlowComponent& flow) {
    const Actors actors = FindActors(world);
    if (actors.player.value == 0 || actors.boss.value == 0)
        return;
    const FlowHudValues values = ReadHudValues(world, actors);
    const HIKARI::UiView ui = world.GetUi();
    // 登場の演出の間（ボスが Boss.Cinematic に 1 を書いている間）は、プレイヤーの条を出さない（画面を演出だけにする）。
    float cinematic = 0.0f;
    ObjectVariables::ReadFloat(world, actors.boss, kBossCinematicVariable, cinematic);
    bool accepted = cinematic >= 0.5f || FlowHud::DrawPlayer(ui, flow, values);
    // ボスの HP の条：戦いの間。ボスが「出さない」と書いている間（登場の演出の見得まで）は出さない。
    float bossBar = 1.0f;
    ObjectVariables::ReadFloat(world, actors.boss, kBossBarShownVariable, bossBar);
    if (flow.phase != FlowPhase::Waiting && bossBar >= 0.5f)
        accepted = FlowHud::DrawBoss(ui, flow, values) && accepted;
    accepted = FlowHud::DrawFade(ui, flow, FlowRules::GetFade(flow, frame.fixedDt)) && accepted;
    if (accepted || flow.hudRejectedLogged)
        return;
    flow.hudRejectedLogged = true;
    Log(HIKARI::GameProjectLogLevel::Warning, "HUD の矩形が受け付けられませんでした（描画の準備ができていない）");
}

} // namespace

void FlowSystem::OnWorldAttached(HIKARI::WorldView&) { fallback_ = FlowComponent{}; }

void FlowSystem::FixedUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame) || frame.fixedDt <= 0.0f)
        return;
    bool placed = false;
    world.ForEachComponent<FlowComponent>(HIKARI::MakeComponentTypeId(kFlowType),
                                          [&](HIKARI::ObjectView, FlowComponent& flow) {
                                              placed = true;
                                              StepFlow(world, frame, flow);
                                          });
    if (!placed)
        StepFlow(world, frame, fallback_);
}

void FlowSystem::LateUpdate(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) {
    if (!IsPlaying(world, frame))
        return;
    bool placed = false;
    world.ForEachComponent<FlowComponent>(HIKARI::MakeComponentTypeId(kFlowType),
                                          [&](HIKARI::ObjectView, FlowComponent& flow) {
                                              placed = true;
                                              DrawFlow(world, frame, flow);
                                          });
    if (!placed)
        DrawFlow(world, frame, fallback_);
}

void FlowSystem::OnWorldDetached(HIKARI::WorldView& world) {
    world.ReleaseVariableWriter(kFightStateVariable, kWriterId);
}

} // namespace Project_TD2_2
