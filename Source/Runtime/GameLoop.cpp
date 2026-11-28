#include "HIKARI/Extend/World.h"
#include "HIKARI/Game.h"

#include <memory>

#include "Boss/BossBehaviorNodes.h"
#include "Boss/BossComponent.h"
#include "Boss/BossSystem.h"
#include "BossBody/BossBodyVisuals.h"
#include "Burn/BurnFieldComponent.h"
#include "Burn/BurnFieldSystem.h"
#include "Flow/FlowComponent.h"
#include "Flow/FlowSystem.h"
#include "Gourd/GourdFeedbackSystem.h"
#include "Gourd/GourdShotSystem.h"
#include "Gourd/GourdSystem.h"
#include "Mount/CloudMountSystem.h"
#include "Gourd/GourdComponent.h"
#include "Gourd/GourdShotComponent.h"
#include "Mount/CloudMountComponent.h"
#include "Player/Player.h"
#include "Player/PlayerDebugComponent.h"
#include "Player/PlayerDebugSystem.h"
#include "Player/PlayerSystem.h"

// ゲームの 1 固定 tick と 1 フレームの中で、何をどの順に動かすかを一か所で決める（c-df5bfd）。
// 旧い IGameSystem は段（PreFixedUpdate・FixedUpdate・PostFixedUpdate・Update・LateUpdate・PreRender・PostRender）と
// 実行順（executionOrder）で並んでいた。新しい層のシステムは段ごとに 1 つの回呼（FixedUpdate・PostUpdate・LateUpdate・
// PreRender）しか持たないので、段の中身（各 *System.cpp の名前空間の関数。名前は旧い段のまま）を、旧い並びのままここから
// 呼ぶ。並びを変えると手応えが変わるので、動かす時は verify.py の logdiff・pixdiff で確かめる。
//
// 旧い実行順：Flow 80・PlayerDebug 90・Player 100・CloudMount 102・GourdShot 105・Gourd 110・GourdFeedback 115・
// 物理（PhysicsSystem）120・Boss 125・BurnField 130。物理は FixedUpdate の途中（120）で運動学の移動を処理して世界を進める。
//   * 固定 tick（FixedUpdate）：新しい層のシステムどうしは HIKARI_AFTER の順で、引擎の物理の前に走る。
//   * 描画のフレームの LateUpdate・PreRender は、引擎の予定表の中の HIKARI_LATE_ORDER の位置で、引擎のシステム（台詞 85・
//     物理の姿勢の補間 120・水 95・特効 98・モデルの提出 100・流体の障害物 185・カメラ 180・190）と交ざって走る。旧い実行順が
//     100 を外れる Flow（80）・Boss（125）・BurnField（130）は、別のシステムに分けて、その旧い実行順を書く。
namespace Project_TD2_2 {

// 1 固定 tick の最初：旧 PreFixedUpdate の全部。
// 旧い並びは「プレイヤーが状態を進める → 筋斗雲・砲撃・吸い込みが要求を出す → プレイヤーが動く」の挟み込み：要求は
// Player の PreFixedUpdate が毎 tick 空にした後、ほかの PreFixedUpdate が入れ直し、Player の FixedUpdate が読む。
struct GameTick : HIKARI::System {
    HIKARI_SYSTEM(GameTick)
    HIKARI_USES(Player, PlayerDebug, CloudMount, Gourd, GourdShot, Boss, Flow, BurnField)

    void FixedUpdate(float) override {
        HIKARI::WorldView world = HIKARI::Extend::CurrentWorld();
        const HIKARI::FrameContext& frame = HIKARI::Extend::CurrentFrame();
        // 旧 PreFixedUpdate：Player 100 → CloudMount 102 → GourdShot 105 → Gourd 110。
        PlayerSystem::PreFixedUpdate(world, frame);
        CloudMountSystem::PreFixedUpdate(world, frame);
        GourdShotSystem::PreFixedUpdate(world, frame);
        GourdSystem::PreFixedUpdate(world, frame);
    }
};

// 戦いの流れと HUD（旧 Flow 80）。固定 tick では、プレイヤーとボスの状態を見て段階を進める（旧 FixedUpdate の先頭：全部の
// PreFixedUpdate の後、プレイヤーなどの FixedUpdate の前）。描画のフレームでは、LateUpdate の段の台詞（85）より前に HUD を出す。
struct FlowLoop : HIKARI::System {
    HIKARI_SYSTEM(FlowLoop)
    HIKARI_LATE_ORDER(80)
    HIKARI_USES(Boss, Flow)
    HIKARI_AFTER(GameTick)

    void FixedUpdate(float) override {
        HIKARI::WorldView world = HIKARI::Extend::CurrentWorld();
        FlowSystem::FixedUpdate(world, HIKARI::Extend::CurrentFrame(), fallbackFlow_);
    }

    void LateUpdate(float) override {
        HIKARI::WorldView world = HIKARI::Extend::CurrentWorld();
        FlowSystem::LateUpdate(world, HIKARI::Extend::CurrentFrame(), fallbackFlow_);
    }

private:
    // シーンに Flow 部品が無い時に使う、既定値の流れ。世界につき 1 局ぶんで、世界に付くたびに初めから。
    Flow fallbackFlow_;
};

// プレイヤー・筋斗雲・ヒョウタン・砲撃（旧 Player 100〜GourdFeedback 115）の固定 tick の続き・フレームの各段と、物理の後のボス・
// 燃える地面・旧 PostFixedUpdate。
struct GameLoop : HIKARI::System {
    HIKARI_SYSTEM(GameLoop)
    HIKARI_USES(Player, PlayerDebug, CloudMount, Gourd, GourdShot, Boss, Flow, BurnField)
    HIKARI_AFTER(FlowLoop)

    void OnStart() override {
        // 物理の後の分（旧 FixedUpdate の物理より後・旧 PostFixedUpdate）は、固定 tick ごとに WaitFixed で戻る協程で動かす。
        (void)AfterPhysics();
    }

    // 旧 FixedUpdate（物理より前）の、Flow の後：PlayerDebug 90 → Player 100 → CloudMount 102 → GourdShot 105。
    void FixedUpdate(float) override {
        HIKARI::WorldView world = HIKARI::Extend::CurrentWorld();
        const HIKARI::FrameContext& frame = HIKARI::Extend::CurrentFrame();
        PlayerDebugSystem::FixedUpdate(world, frame);
        PlayerSystem::FixedUpdate(world, frame);
        CloudMountSystem::FixedUpdate(world, frame);
        GourdShotSystem::FixedUpdate(world, frame);
    }

    // 1 フレームの、固定 tick を回した後：旧 Update の全部（PostUpdate）→ 旧 LateUpdate の全部（LateUpdate）→ 旧 PreRender の
    // 全部（PreRender）。段をまたいで並びを混ぜない：筋斗雲の Update が出すカメラの要求を、同じフレームのプレイヤーの
    // LateUpdate が使う。
    void PostUpdate(float) override {
        HIKARI::WorldView world = HIKARI::Extend::CurrentWorld();
        const HIKARI::FrameContext& frame = HIKARI::Extend::CurrentFrame();
        // 旧 Update：Player 100 → CloudMount 102 → GourdShot 105 → Gourd 110 → Boss 125。
        PlayerSystem::Update(world, frame);
        CloudMountSystem::Update(world, frame);
        GourdShotSystem::Update(world, frame);
        GourdSystem::Update(world, frame);
        BossSystem::Update(world, frame);
    }

    void LateUpdate(float) override {
        HIKARI::WorldView world = HIKARI::Extend::CurrentWorld();
        const HIKARI::FrameContext& frame = HIKARI::Extend::CurrentFrame();
        // 旧 LateUpdate：Player 100 → CloudMount 102 → GourdShot 105 → Gourd 110 → GourdFeedback 115。
        PlayerSystem::LateUpdate(world, frame);
        CloudMountSystem::LateUpdate(world, frame);
        GourdShotSystem::LateUpdate(world, frame);
        GourdSystem::LateUpdate(world, frame);
        GourdFeedbackSystem::LateUpdate(world, frame);
    }

    void PreRender(float) override {
        HIKARI::WorldView world = HIKARI::Extend::CurrentWorld();
        const HIKARI::FrameContext& frame = HIKARI::Extend::CurrentFrame();
        // 旧 PreRender：Player 100 → GourdShot 105 → Gourd 110。
        PlayerSystem::PreRender(world, frame);
        GourdShotSystem::PreRender(world, frame);
        GourdSystem::PreRender(world, frame);
    }

    // 旧 OnWorldDetached（ヒョウタンの吸い込みと砲撃の見た目を止める）。
    void OnDestroy() override {
        HIKARI::WorldView world = HIKARI::Extend::CurrentWorld();
        GourdShotSystem::OnWorldDetached(world);
        GourdSystem::OnWorldDetached(world);
    }

private:
    // 物理を進めた後：旧 FixedUpdate の物理より後の Boss 125 → BurnField 130 → 旧 PostFixedUpdate の PlayerDebug 90 →
    // Player 100 → Gourd 110。ボス・燃える地面が済んでから、旧 PostFixedUpdate が走る。
    HIKARI::Task AfterPhysics() {
        for (;;) {
            co_await HIKARI::WaitFixed{};
            HIKARI::WorldView world = HIKARI::Extend::CurrentWorld();
            const HIKARI::FrameContext& frame = HIKARI::Extend::CurrentFrame();
            BossSystem::FixedUpdate(world, frame, bossBrains_);
            BurnFieldSystem::FixedUpdate(world, frame);
            PlayerDebugSystem::PostFixedUpdate(world, frame);
            PlayerSystem::PostFixedUpdate(world, frame);
            GourdSystem::PostFixedUpdate(world, frame);
        }
    }

    // ボスが技を聞く相手（ふだんは行動ツリー）。世界につき 1 組。
    BossBrains bossBrains_;
};

// ボスの見た目の段（旧 Boss 125 の LateUpdate・PreRender・OnWorldDetached）。物理の姿勢の補間（120）の後・カメラ（180・190）の前。
struct BossLate : HIKARI::System {
    HIKARI_SYSTEM(BossLate)
    HIKARI_LATE_ORDER(125)
    HIKARI_USES(Boss)

    void LateUpdate(float) override {
        HIKARI::WorldView world = HIKARI::Extend::CurrentWorld();
        BossSystem::LateUpdate(world, HIKARI::Extend::CurrentFrame());
    }

    void PreRender(float) override {
        HIKARI::WorldView world = HIKARI::Extend::CurrentWorld();
        BossSystem::PreRender(world, HIKARI::Extend::CurrentFrame());
    }

    // 旧 OnWorldDetached（ボスの見た目を止める）。
    void OnDestroy() override {
        HIKARI::WorldView world = HIKARI::Extend::CurrentWorld();
        BossSystem::OnWorldDetached(world);
    }
};

// 燃える地面の見た目の段（旧 BurnField 130 の PreRender）。燃えているセルを火の流体として見せる。
struct BurnFieldLate : HIKARI::System {
    HIKARI_SYSTEM(BurnFieldLate)
    HIKARI_LATE_ORDER(130)
    HIKARI_USES(BurnField)

    void PreRender(float) override {
        HIKARI::WorldView world = HIKARI::Extend::CurrentWorld();
        BurnFieldSystem::PreRender(world, HIKARI::Extend::CurrentFrame());
    }
};

// 編集中（Play の外）にも動かす分（旧い Game システムと、ボスの System の LateUpdate の Play の外の枝）。
//   * 行動ツリーのボスの節は、ボスのいないシーンを開いている時も編集器が知っているように、ここでも持っておく
//     （登録は GameLoop の行動ツリーと分け合う）。
//   * 演出ワークスペースがボスの登場・変わり目の資産を流している間、役者・骨格の体をその時刻の姿にする（c-e0c500・c-271cba）。
//     旧い Boss（125）の LateUpdate と同じ位置で走る。
struct EditorSupport : HIKARI::System {
    HIKARI_SYSTEM_EDITOR(EditorSupport)
    HIKARI_LATE_ORDER(125)

    void OnStart() override { bossNodes_ = BossBehaviorNodes::Acquire(); }

    void LateUpdate(float) override {
        if (HIKARI::Scene::IsPlaying())
            return;
        HIKARI::WorldView world = HIKARI::Extend::CurrentWorld();
        BossBodyVisuals::UpdatePreviews(world);
    }

    void OnDestroy() override { bossNodes_.reset(); }

private:
    std::shared_ptr<BossBehaviorNodes> bossNodes_;
};

} // namespace Project_TD2_2
