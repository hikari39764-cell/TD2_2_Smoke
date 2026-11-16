#include "FlowSystem.h"

#include <cmath>
#include <cstdio>

#include "HIKARI/Project/GameFlowViews.h"
#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Runtime/ComponentTypeId.h"

#include "../Common/GameSession.h"
#include "../Boss/BossRules.h"
#include "../Common/ObjectVariables.h"
#include "../Gourd/GourdComponent.h"
#include "../Player/PlayerComponent.h"
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
};

Actors FindActors(HIKARI::WorldView& world) {
    Actors actors;
    world.ForEachComponent<PlayerComponent>(HIKARI::MakeComponentTypeId(kPlayerType),
                                            [&](HIKARI::ObjectView object, PlayerComponent&) {
                                                if (actors.player.value == 0)
                                                    actors.player = object.GetHandle();
                                            });
    world.ForEachComponent<GourdComponent>(HIKARI::MakeComponentTypeId(kGourdType),
                                           [&](HIKARI::ObjectView object, GourdComponent&) {
                                               if (actors.gourd.value == 0)
                                                   actors.gourd = object.GetHandle();
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

FlowObservation Observe(const HIKARI::WorldView& world, const Actors& actors) {
    FlowObservation observation;
    HIKARI::GameProjectFloat3 playerPosition{};
    if (GetPosition(world, actors.player, playerPosition)) {
        observation.playerPresent = true;
        observation.playerHeight = playerPosition.y;
        int64_t state = 0;
        observation.playerDead = ObjectVariables::ReadInt(world, actors.player, kPlayerStateVariable, state) &&
                                 state == static_cast<int64_t>(PlayerState::Dead);
    }
    HIKARI::GameProjectFloat3 bossPosition{};
    if (GetPosition(world, actors.boss, bossPosition)) {
        observation.bossPresent =
            ObjectVariables::ReadFloat(world, actors.boss, kBossHealthVariable, observation.bossHealth) &&
            ObjectVariables::ReadFloat(world, actors.boss, kBossMaxHealthVariable, observation.bossMaxHealth);
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
    ObjectVariables::ReadFloat(world, actors.player, kPlayerHealthVariable, values.playerHealth);
    ObjectVariables::ReadFloat(world, actors.player, kPlayerMaxHealthVariable, values.playerMaxHealth);
    ObjectVariables::ReadFloat(world, actors.player, kPlayerStaminaVariable, values.playerStamina);
    ObjectVariables::ReadFloat(world, actors.player, kPlayerMaxStaminaVariable, values.playerMaxStamina);
    int64_t charges = 0;
    if (ObjectVariables::ReadInt(world, actors.player, kPlayerHealChargesVariable, charges))
        values.healCharges = static_cast<int>(charges);
    ObjectVariables::ReadFloat(world, actors.player, kPlayerBurnVariable, values.playerBurn);
    ObjectVariables::ReadFloat(world, actors.gourd, kGourdSmokeVariable, values.gourdSmoke);
    ObjectVariables::ReadFloat(world, actors.gourd, kGourdCapacityVariable, values.gourdCapacity);
    ObjectVariables::ReadFloat(world, actors.boss, kBossHealthVariable, values.bossHealth);
    ObjectVariables::ReadFloat(world, actors.boss, kBossMaxHealthVariable, values.bossMaxHealth);
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
    bool accepted = FlowHud::DrawPlayer(ui, flow, values);
    if (flow.phase != FlowPhase::Waiting)
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
