#include "HIKARI/Project/GameFlowViews.h"
#include "HIKARI/Project/GameProjectModule.h"
#include <array>
#include <cstdint>
#include <memory>

#include "Player/PlayerComponent.h"
#include "Player/PlayerSystem.h"
#include "Player/PlayerDebugComponent.h"
#include "Player/PlayerDebugSystem.h"
#include "Gourd/GourdComponent.h"
#include "Gourd/GourdSystem.h"
#include "Combat/DamageableComponent.h"
#include "Combat/DamageableSystem.h"
#include "Gourd/GourdShotComponent.h"
#include "Gourd/GourdShotSystem.h"
#include "Gourd/GourdFeedbackSystem.h"
#include "Boss/BossComponent.h"
#include "Boss/BossSystem.h"

namespace Project_TD2_2 {

// Play 中にシーンをまたいで保持するプロジェクト状態。
class GameSession final : public HIKARI::IGameProjectSession {
public:
    void OnGameStarted(HIKARI::WorldView&) override {
        gameplaySeconds = 0.0f;
        enteredScenes = 0;
        sceneActive = false;
    }

    void OnSceneEntered(HIKARI::WorldView&) override {
        ++enteredScenes;
        sceneActive = true;
    }
    void OnSceneExiting(HIKARI::WorldView&) override { sceneActive = false; }

    float gameplaySeconds = 0.0f;
    uint32_t enteredScenes = 0;
    bool sceneActive = false;
};

// システムは通常どおり World に Attach され、ゲーム状態は Play session から取得する。
class Game final : public HIKARI::IGameSystem {
public:
    void Update(HIKARI::WorldView& world, const HIKARI::FrameContext& frame) override {
        auto* session = dynamic_cast<GameSession*>(world.GetGameSession());
        if (session == nullptr || !session->sceneActive || frame.paused) {
            return;
        }
        session->gameplaySeconds += frame.gameDt;
    }
};

// 以下はエンジンへの登録。日常のゲーム処理は上の Game に書く。
// Register で登録したコンポーネントとシステムは自動的にこの Feature の所有になる。
class GameplayFeature final : public HIKARI::IRuntimeFeature {
public:
    std::string_view GetFeatureId() const noexcept override { return "Project_TD2_2.Gameplay"; }
    std::string_view GetDisplayName() const noexcept override { return "Gameplay"; }
    std::string_view GetDescription() const noexcept override { return "Project game logic"; }
    std::span<const HIKARI::RuntimeSystemDefault> GetDefaultSystems() const noexcept override {
        static constexpr std::array<HIKARI::RuntimeSystemDefault, 1> systems{{{"Project_TD2_2.Game", true, 100}}};
        return systems;
    }
    bool Register(HIKARI::RuntimeFeatureContext& context) override {
        return context.RegisterComponent<DamageableComponent>(
                   {.typeName = kDamageableType, .displayName = "被弾と HP"}) &&
               context.RegisterSystem<DamageableSystem>(
                   {.systemId = kDamageableSystem, .displayName = "共通の被弾反応"}) &&
               context.BindComponentSystem(
                   {.componentType = kDamageableType, .systemId = kDamageableSystem, .defaultExecutionOrder = 120}) &&
               context.RegisterSystem<Game>({.systemId = "Project_TD2_2.Game", .displayName = "Game"}) &&
               context.RegisterComponent<PlayerComponent>({.typeName = kPlayerType, .displayName = "プレイヤー"}) &&
               context.RegisterSystem<PlayerSystem>({.systemId = kPlayerSystem, .displayName = "プレイヤー"}) &&
               context.BindComponentSystem(
                   {.componentType = kPlayerType, .systemId = kPlayerSystem, .defaultExecutionOrder = 100}) &&
               context.RegisterComponent<PlayerDebugComponent>(
                   {.typeName = kPlayerDebugType, .displayName = "プレイヤー検証（通常は無効）"}) &&
               context.RegisterSystem<PlayerDebugSystem>(
                   {.systemId = kPlayerDebugSystem, .displayName = "プレイヤー検証"}) &&
               context.BindComponentSystem(
                   {.componentType = kPlayerDebugType, .systemId = kPlayerDebugSystem, .defaultExecutionOrder = 90}) &&
               context.RegisterComponent<GourdComponent>({.typeName = kGourdType, .displayName = "ヒョウタン"}) &&
               context.RegisterSystem<GourdSystem>({.systemId = kGourdSystem, .displayName = "ヒョウタン"}) &&
               context.BindComponentSystem(
                   {.componentType = kGourdType, .systemId = kGourdSystem, .defaultExecutionOrder = 110}) &&
               context.RegisterComponent<GourdShotComponent>(
                   {.typeName = kGourdShotType, .displayName = "ヒョウタン砲撃"}) &&
               context.RegisterSystem<GourdShotSystem>(
                   {.systemId = kGourdShotSystem, .displayName = "ヒョウタン砲撃"}) &&
               context.BindComponentSystem(
                   {.componentType = kGourdShotType, .systemId = kGourdShotSystem, .defaultExecutionOrder = 105}) &&
               context.RegisterSystem<GourdFeedbackSystem>(
                   {.systemId = kGourdFeedbackSystem, .displayName = "発射のカメラ反動"}) &&
               context.BindComponentSystem(
                   {.componentType = kGourdShotType, .systemId = kGourdFeedbackSystem, .defaultExecutionOrder = 115}) &&
               context.RegisterComponent<BossComponent>({.typeName = kBossType, .displayName = "ボス 霞武士"}) &&
               context.RegisterSystem<BossSystem>({.systemId = kBossSystem, .displayName = "ボス 霞武士"}) &&
               context.BindComponentSystem(
                   {.componentType = kBossType, .systemId = kBossSystem, .defaultExecutionOrder = 125});
    }
};

class GameProjectModule final : public HIKARI::RUNTIME::IGameProjectModule {
public:
    std::string_view GetModuleId() const noexcept override { return "Project_TD2_2"; }
    bool RegisterRuntimeFeatures(HIKARI::RuntimeFeatureCatalog& catalog) override {
        return HIKARI::RegisterRuntimeFeature<GameplayFeature>(catalog);
    }
    std::unique_ptr<HIKARI::IGameProjectSession> CreateGameSession() override {
        return std::make_unique<GameSession>();
    }
};
} // namespace

namespace HIKARI::RUNTIME {
std::unique_ptr<IGameProjectModule> CreateGameProjectModule() {
    return std::make_unique<::Project_TD2_2::GameProjectModule>();
}
} // namespace HIKARI::RUNTIME
