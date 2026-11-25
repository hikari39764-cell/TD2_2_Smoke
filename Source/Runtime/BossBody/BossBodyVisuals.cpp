#include "BossBodyVisuals.h"

#include <algorithm>
#include <array>
#include <string_view>

#include "HIKARI/Project/FluidView.h"
#include "HIKARI/Project/PresentationViews.h"
#include "BossAnimBody.h"
#include "BossBodyAccents.h"
#include "BossBodyMath.h"
#include "BossBodyRig.h"
#include "BossBodySword.h"
#include "BossIntroVisuals.h"
#include "BossPhaseShiftVisuals.h"
#include "../Boss/BossCloneComponent.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossMath.h"
#include "../Boss/BossVfx.h"
#include "../Common/FluidChannels.h"
#include "../Common/VfxParameters.h"

namespace Project_TD2_2 {
namespace {

// 模型のノードの名前（模型側の契約。作るスクリプト Tools/BossBody と同じ名前にする）と、表示専用の模型の番号（持ち主の物体ごと）。
struct BossBodyPart {
    std::string_view node;
    uint64_t instance;
};
constexpr BossBodyPart kHilt{"KatanaHilt", 1u};
constexpr BossBodyPart kBlade{"KatanaBlade", 2u};
// 灼けた刀身は、ふつうの刀身と同じ番号で出す（差し替え。同じフレームに両方は出ない）。
constexpr BossBodyPart kBladeHot{"KatanaBladeHot", 2u};
constexpr BossBodyPart kRightHand{"HandR", 3u};
constexpr BossBodyPart kRightCuff{"Cuff", 4u};
constexpr BossBodyPart kLeftCuff{"Cuff", 5u};
constexpr uint64_t kLeftHandInstance = 6u;
// 胸当ては騎士の胸当て（首当て付き・写実の貼図。c-584825。模型を作るスクリプト Tools/BossBody/bb_knight_armor.py）。
constexpr BossBodyPart kTorso{"KnightBody", 7u};
// 兜は騎士の兜（角付き・写実の貼図。c-08863a。模型を作るスクリプト Tools/BossBody/bb_knight_helm.py）。
constexpr BossBodyPart kHead{"KnightHelm", 8u};
// 灼けた鎧（フェーズ 2）は、ふだんの鎧と同じ番号でノードだけ差し替える（同じフレームに両方は出ない。c-857ae3）。
constexpr BossBodyPart kTorsoHot{"KnightBodyHot", 7u};
constexpr BossBodyPart kHeadHot{"KnightHelmHot", 8u};
// 面頬の隙間の中の光（模型に足した自発光の面。Tools/BossBody/add_helm_glow.py）。目が灯っている時だけ、兜と同じ所へ出す。
// フェーズ 2 は火の色の方へ差し替える。
constexpr BossBodyPart kHeadGlow{"HelmGlow", 9u};
constexpr BossBodyPart kHeadGlowHot{"HelmGlowHot", 9u};
constexpr std::string_view kTassetHotNode = "KusazuriPanelHot";
// 兜の中の光（面頬の 5 本の隙間から透ける一筋の光と尾）は模型に入れず、目のエフェクト（BossBodyAccents の
// eyesEffect）が受け持つ。
// 肩当て（右・左。c-584825）：左右は鏡の形で、模型の置き方は鏡にできない（拡大は一様）ので、ノードを分けてある。
constexpr std::array<std::string_view, 2> kSodeNodes{"KnightPauldronR", "KnightPauldronL"};
constexpr std::array<std::string_view, 2> kSodeHotNodes{"KnightPauldronRHot", "KnightPauldronLHot"};
constexpr uint64_t kSodeInstance = 10u;
// 草摺は、同じノードを枚数ぶん、この番号から続けて出す。
constexpr std::string_view kTassetNode = "KusazuriPanel";
constexpr uint64_t kTassetInstance = 20u;
// 胴の煙のエフェクトの入力（資産側の契約）：出す量の倍率と、源の温度の倍率。
constexpr const char* kSmokeHeatInput = "input.heat";
// 刀身に掛ける材質の効果（エンジンに組み込みの溶かす効果。溶かす量は 0 のまま、輪郭の発光だけを使う）と、その項目の名前。
constexpr std::string_view kBladeFxProfile = "HIKARI_Dissolve";
constexpr std::string_view kRimStrength = "rimStrength";
constexpr std::string_view kRimPower = "rimPower";
constexpr std::string_view kPulseSpeed = "pulseSpeed";
// 溶かす量（0〜1）。霞の大剣の間、太刀を霞にほどく（c-9143b5）。
constexpr std::string_view kDissolveAmount = "amount";
// 太刀がこれ以上ほどけたら、柄も刀身も描かない（大剣だけを持つ）。
constexpr float kTachiHiddenAmount = 0.98f;
// 輪郭の光がこれより弱ければ、強さ 0 を送る。
constexpr float kBladeRimMinimum = 0.01f;

// 胴の煙の入力：火の粉・熱のゆがみの粒・裾と首の熾火の源の量（フェーズ 1 は 0）。
constexpr const char* kSmokeBurnInput = "input.burn";
// 火の舌の注入の番号（ボスの物体ごと）：襟の右 2 本・左 2 本、右・左の袖口。
constexpr uint64_t kFireTongueBase = 1600;

// フェーズ 2 の燃える胴の強さ（0〜1）を進める：フェーズ 2 で付き、撃破で消える。気絶の間は弱める。一定の速さで寄せる
// （火の大きさが急に跳ばない）。
void StepFire(BossComponent& boss, float seconds) {
    const BossBodySettings& s = boss.body;
    float target = 0.0f;
    if (BossBodyMath::PhaseTwoLook(boss) && boss.state != BossState::Dead)
        target = boss.state == BossState::Stunned ? std::clamp(s.fireStunnedShare, 0.0f, 1.0f) : 1.0f;
    // 変わり目の演出（c-8dd296）：閃光より前から、内から火が回る。
    target = std::max(target, BossPhaseShiftVisuals::FireFloor(boss));
    float& fire = boss.bodyState.fire;
    const float step = s.fireRiseSeconds > 0.0f ? std::max(0.0f, seconds) / s.fireRiseSeconds : 1.0f;
    fire = fire < target ? std::min(target, fire + step) : std::max(target, fire - step);
}

// 胴の煙：体の足元に付けて出し続ける。量と熱さは、気絶・フェーズで変える。
void UpdateSmoke(HIKARI::WorldView& world, BossComponent& boss, const HIKARI::GameProjectTransform& body) {
    using namespace BossVfx;
    if (boss.body.smokeEffect.IsEmpty())
        return;
    // 体の物体の原点は体の中心なので、足元まで下げる（エフェクトの原点＝足元）。域の大きさは拡大の影響を受けない。
    HIKARI::GameProjectTransform transform{};
    transform.rotation = body.rotation;
    transform.position = BossMath::Add(
        body.position, BossMath::Rotate(body.rotation, {0.0f, -boss.bodyHalfHeight * body.scale.y, 0.0f}));
    HIKARI::GameProjectVfxParameterOverrides parameters{};
    const bool stunned = boss.state == BossState::Stunned || boss.state == BossState::Dead;
    // 技に合わせた量と熱さ（溜め・刀の火・振り終わりの吹き出し）は、演出の層（BossBodyAccents::StepSmoke）が決める。
    const BossBodyAccentState& accents = boss.bodyState.accents;
    // 登場：組み上がる前は出さず（域も持たない）、組み上がりの始めは柱として濃く立つ。
    const float intro = BossIntroVisuals::SmokeScale(boss);
    if (intro <= 0.0f && !boss.bodyState.smokeVfx.IsValid())
        return;
    // フェーズ 2 の燃える胴（c-857ae3）：火の強さに合わせて、熱さを smokeHeatPhaseTwo へ寄せ、量を変え、火の粉と
    // 熱のゆがみを出す。フェーズ 1 は火の強さが 0 なので、いままでと同じ値を送る。
    const float fire = boss.bodyState.fire;
    VfxParameters::SetFloat(parameters, kAmountInput,
                            boss.body.smokeAmount * intro *
                                (stunned ? boss.body.smokeAmountStunned : accents.smokeAmount) *
                                BossMath::Lerp(1.0f, boss.body.smokeAmountPhaseTwo, fire));
    VfxParameters::SetFloat(parameters, kSmokeHeatInput,
                            BossMath::Lerp(stunned ? 1.0f : accents.smokeHeat, boss.body.smokeHeatPhaseTwo, fire));
    VfxParameters::SetFloat(parameters, kSmokeBurnInput, boss.body.fireBurn * fire);
    StartOrSend(world, boss, boss.bodyState.smokeVfx, boss.body.smokeEffect, transform, parameters);
}

std::string_view LeftHandNode(BossBodyHandShape shape) {
    switch (shape) {
    case BossBodyHandShape::Fist: return "HandL_Fist";
    case BossBodyHandShape::Seal: return "HandL_Seal";
    default: return "HandL_Open";
    }
}

void Submit(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, const BossBodyPart& part,
            const BossBodyPlacement& placement) {
    HIKARI::GameProjectPresentationModelRequest request{};
    request.ownerObjectHandle = object.GetHandle().value;
    request.instanceId = part.instance;
    const std::string& guid = boss.body.model;
    std::copy_n(guid.data(), std::min(guid.size(), sizeof(request.model.assetGuid) - 1), request.model.assetGuid);
    request.castShadow = boss.body.castShadow ? 1u : 0u;
    request.worldTransform.position = placement.position;
    request.worldTransform.rotation = placement.rotation;
    request.worldTransform.scale = {placement.scale, placement.scale, placement.scale * placement.lengthScale};
    if (world.GetPresentationModels().SubmitExclusiveNode(request, part.node) || boss.bodyState.submitWarned)
        return;
    boss.bodyState.submitWarned = true;
    BossLog::Warn("[BossBody] 外形の模型を出せませんでした（Boss の「外形の模型」とノードの名前を確かめてください）");
}

// 太刀のほどけ（0〜1。c-9143b5）：霞の大剣が満ちるのに合わせて太刀が霞にほどけ、大剣がほどける間に戻る。
float TachiDissolve(const BossComponent& boss) {
    const BossGreatswordState& sword = boss.greatsword;
    switch (sword.phase) {
    case BossGreatswordPhase::None: return 0.0f;
    case BossGreatswordPhase::Conjure: return std::clamp(sword.fill * boss.body.accents.tachiDissolveSpeed, 0.0f, 1.0f);
    case BossGreatswordPhase::Dissolve: return std::clamp(1.0f - sword.dissolve, 0.0f, 1.0f);
    default: return 1.0f;
    }
}

// 刀身の輪郭の光の目標：両手の斬りの構え・溜め（前隙と、連撃の振りの間）で灯り、大きな溜めで増し、振る間は強い。
float BladeRimTarget(const BossComponent& boss, bool hot) {
    const BossBodyAccentSettings& a = boss.body.accents;
    if (boss.state != BossState::Act || boss.swordPose.twoHand < 0.5f)
        return 0.0f;
    float rim = 0.0f;
    if (boss.swordSmoke)
        rim = a.bladeRimSwing;
    else if (boss.phase != BossActPhase::Post)
        rim = a.bladeRimKamae + a.bladeRimCharge * std::max(0.0f, boss.chargeVisual);
    // 実体の刀だけで当てる技の間は、刀身を光らせない（霞の技と見分ける。c-ecd963）。刀を振る技でない時（散歩の構え・
    // 霞の大剣）も光らせない（散歩の構えで光ると、剣気の技が来ると読めてしまう。c-44fdc5）。
    const BossBodyAccents::BladeKind kind = BossBodyAccents::Kind(boss);
    if (kind == BossBodyAccents::BladeKind::Real)
        rim *= a.realRimShare;
    else if (kind == BossBodyAccents::BladeKind::None)
        rim = 0.0f;
    return rim * (hot ? a.bladeRimHotShare : 1.0f);
}

// 刀身（模型の刀身の番号）に材質の効果を掛け、輪郭の光を時間で寄せて送る。効果を掛ける前に同じフレームで刀身を頼んで
// あること（頼まれていない番号には掛けられない）。効果（プロファイル）は掛かっていない時だけ掛け、外さない：掛け外しや
// 掛け直しは描画の場面の作り直し（光の追跡の場面を含む）を毎フレーム起こして重い。光らない時は強さ 0 を送る。
void UpdateBladeShader(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, bool hot,
                       float seconds) {
    const BossBodyAccentSettings& a = boss.body.accents;
    float& rim = boss.bodyState.accents.bladeRim;
    const float target = BladeRimTarget(boss, hot);
    const float time = target > rim ? a.bladeRimRiseSeconds : a.bladeRimFallSeconds;
    rim += (target - rim) * (1.0f - std::exp(-std::max(0.0f, seconds) / std::max(1e-3f, time)));
    // 構えも振りも 0 なら効果を掛けない（材質のまま描く）。
    if (a.bladeRimKamae <= 0.0f && a.bladeRimSwing <= 0.0f)
        return;
    const HIKARI::MaterialFxView fx =
        world.GetPresentationModels().GetMaterialFx(object.GetHandle().value, kBlade.instance);
    if (!fx.IsValid())
        return;
    char current[64]{};
    if (fx.GetProfileIdSize() != kBladeFxProfile.size() || !fx.CopyProfileId(current, sizeof(current)) ||
        kBladeFxProfile != current) {
        if (!fx.SetProfileId(kBladeFxProfile))
            return;
        fx.SetFloat(kRimPower, a.bladeRimPower);
        fx.SetFloat(kPulseSpeed, a.bladeRimPulse);
    }
    fx.SetFloat(kRimStrength, rim < kBladeRimMinimum ? 0.0f : rim);
    fx.SetFloat(kDissolveAmount, TachiDissolve(boss));
}

// 置き場所の一式を、持ち主の物体（本体・分身）の表示専用の模型として頼む。番号は持ち主ごとなので、本体と分身で
// 同じ番号を使っても重ならない。
void SubmitPose(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, const BossBodyPose& pose) {
    // 霞の大剣の間は、ほどけきった太刀を描かない（両手は大剣の柄を握る。c-9143b5）。
    if (TachiDissolve(boss) < kTachiHiddenAmount) {
        Submit(world, object, boss, kHilt, pose.sword);
        Submit(world, object, boss, pose.bladeHot ? kBladeHot : kBlade, pose.sword);
    }
    if (pose.showHands) {
        Submit(world, object, boss, kRightHand, pose.rightHand);
        Submit(world, object, boss, {LeftHandNode(pose.leftShape), kLeftHandInstance}, pose.leftHand);
    }
    Submit(world, object, boss, kRightCuff, pose.rightCuff);
    Submit(world, object, boss, kLeftCuff, pose.leftCuff);
    Submit(world, object, boss, pose.armorHot ? kTorsoHot : kTorso, pose.torso);
    Submit(world, object, boss, pose.armorHot ? kHeadHot : kHead, pose.head);
    if (pose.eyesLit)
        Submit(world, object, boss, pose.eyesFire ? kHeadGlowHot : kHeadGlow, pose.head);
    for (size_t side = 0; side < pose.sode.size(); ++side)
        Submit(world, object, boss, {(pose.armorHot ? kSodeHotNodes : kSodeNodes)[side], kSodeInstance + side},
               pose.sode[side]);
    for (int k = 0; k < pose.tassetCount; ++k)
        Submit(world, object, boss,
               {pose.armorHot ? kTassetHotNode : kTassetNode, kTassetInstance + static_cast<uint64_t>(k)},
               pose.tassets[static_cast<size_t>(k)]);
}

// 火の舌を 1 つ入れる：胴の煙の域だけに届く燃料（燃え始める温度より熱くして入れるので、すぐ燃える）。燃料は昇りながら
// 燃えて熱と煤になり、熱い煤が光る（燃焼の値は胴の煙の資産の域）。冷めた煤は薄い黒い煙になって昇る。
void EmitTongue(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, uint64_t id,
                HIKARI::GameProjectFloat3 at, float share) {
    const BossBodySettings& s = boss.body;
    const float strength = s.fireTongueDensity * share * boss.bodyState.fire;
    if (strength <= 0.0f)
        return;
    HIKARI::GameProjectFluidInteraction emit{};
    emit.kind = HIKARI::GameProjectFluidInteractionKind::Fuel;
    emit.position = at;
    // extents.x は半径、extents.y は温度。
    emit.extents = {std::max(0.02f, s.fireTongueRadius * s.bodyScale), s.fireTongueHeat, 0.0f};
    emit.direction = {0.0f, s.fireTongueRise, 0.0f};
    emit.strength = std::min(10000.0f, strength);
    emit.channels = FluidChannels::kBodyFire;
    boss.fluidAccepted = world.GetFluid().Submit(object.GetHandle(), id, emit).Accepted() && boss.fluidAccepted;
}

// 籠手の筒の口（手首 0 〜 肘の側 1 の割合 along）。筒は手首から局所 -Z へ伸びる。
HIKARI::GameProjectFloat3 CuffMouth(const BossBodySettings& s, const BossBodyPlacement& cuff, float along) {
    const float length = s.cuffModelLength * cuff.scale * cuff.lengthScale * along;
    return BossMath::Add(cuff.position, BossMath::Rotate(cuff.rotation, {0.0f, 0.0f, -length}));
}

// フェーズ 2 の火の舌：襟の両脇と両の袖口から、燃料を上へ吹き出す（見える鎧の口に付けて動かす）。瞬間移動した
// フレームと、霧へ沈んでいる間は入れない（移った線に沿って火を引かない。地面の下で燃えない）。
void UpdateFireTongues(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                       const BossBodyPose& pose) {
    const BossBodySettings& s = boss.body;
    BossBodyState& state = boss.bodyState;
    const bool teleported = boss.teleportCount != state.fireTeleports;
    state.fireTeleports = boss.teleportCount;
    // 変わり目の演出を飛ばしたフレームも瞬間移動と同じに扱う（膝をついた姿から構えへ一度に移るので）。
    if (state.fire <= 0.0f || teleported || BossPhaseShiftVisuals::HoldTongues(boss) || boss.hidden ||
        BossPose::RootHeight(boss) < -BossBodyMath::kTiny || s.smokeEffect.IsEmpty())
        return;
    // 襟：左右に 2 本ずつ（前後へ fireCollarSpread ずらす）。左は右を左右に反転した所。
    const float spread = 0.5f * s.fireCollarSpread;
    for (uint64_t k = 0; k < 4u; ++k) {
        const float side = k < 2u ? 1.0f : -1.0f;
        const float along = (k % 2u) == 0u ? spread : -spread;
        const HIKARI::GameProjectFloat3 local =
            BossMath::Mul({s.fireCollar.x * side, s.fireCollar.y, s.fireCollar.z + along}, pose.torso.scale);
        EmitTongue(world, object, boss, kFireTongueBase + k,
                   BossMath::Add(pose.torso.position, BossMath::Rotate(pose.torso.rotation, local)), 1.0f);
    }
    EmitTongue(world, object, boss, kFireTongueBase + 4u, CuffMouth(s, pose.rightCuff, s.fireCuffAlong),
               s.fireCuffShare);
    EmitTongue(world, object, boss, kFireTongueBase + 5u, CuffMouth(s, pose.leftCuff, s.fireCuffAlong),
               s.fireCuffShare);
}

// 待機の構えの全身（霞分身・編集器のプレビュー）。入力は物体の変換と時計だけ：振れは持たず、大袖・草摺は揺らぎだけ。
// 胴の捻りは本体の待機と同じく刀の横の向きから。
BossBodyPose IdlePose(const BossComponent& boss, const HIKARI::GameProjectTransform& body, float clock) {
    HIKARI::GameProjectFloat3 blade{}, grip{};
    BossBodyMath::IdleKamae(boss.body.motion, BossBodyMath::PhaseTwoLook(boss), clock, blade, grip);
    grip = BossBodyMath::Mul(grip, boss.body.bodyScale);
    const BossBodyRigInput input{boss.body,
                                 body,
                                 boss.bodyHalfHeight,
                                 clock,
                                 {},
                                 BossBodyMath::TwistTarget(boss.body.motion, blade),
                                 0.0f,
                                 boss.lastPhaseNumber >= 2,
                                 true,
                                 BossBodyMath::SwordScale(boss),
                                 BossBodyMath::HandScale(boss)};
    return BossBodyRig::SolveIdle(input);
}

} // namespace

void BossBodyVisuals::NoteTick(BossComponent& boss, HIKARI::GameProjectFloat3 position) {
    BossBodyState& state = boss.bodyState;
    // 瞬間移動した tick（霞隠れの現れる・分身との入れ替え）は補間しない。
    const bool jumped = boss.teleportCount != state.tickTeleports;
    state.tickTeleports = boss.teleportCount;
    state.tickFrom = state.ticksSeen > 0 && !jumped ? state.tickTo : position;
    state.tickTo = position;
    state.ticksSeen = std::min(state.ticksSeen + 1, 2);
}

void BossBodyVisuals::Update(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss, float seconds,
                             float alpha) {
    // 撃破の後に体が縮んで消える間は、外形も出さない。
    const bool vanishing = boss.state == BossState::Dead && boss.stateFrame > boss.deadVanishDelayFrames;
    HIKARI::GameProjectTransform body{};
    if (!boss.body.visible || vanishing || !object.GetWorldTransform(body)) {
        Stop(world, boss);
        return;
    }
    // 体の位置は tick ごとに変わるので、描画のフレームでは前の tick との間を補間した所に置く（ナレッジ k-c2a8a4。
    // エンジンが体を描くのと同じ）。向きは tick の値のまま（1 tick に変わる角度は小さい）。
    const BossBodyState& state = boss.bodyState;
    boss.bodyState.presentShift = {};
    if (boss.body.interpolate && state.ticksSeen >= 2)
        boss.bodyState.presentShift =
            BossMath::Mul(BossMath::Sub(state.tickFrom, state.tickTo), 1.0f - std::clamp(alpha, 0.0f, 1.0f));
    body.position = BossMath::Add(body.position, boss.bodyState.presentShift);
    BossIntroVisuals::Advance(world, boss, seconds);
    // 体に付く効果（胴の煙・気配・衝撃の輪）は見た目の体に付ける（登場の間は演技の体の根と向き。c-e0c500）。
    const HIKARI::GameProjectTransform shown = BossIntroVisuals::BodyTransform(boss, body);
    BossPhaseShiftVisuals::Advance(boss, seconds);
    BossBodyAccents::StepSmoke(boss, seconds);
    StepFire(boss, seconds);
    UpdateSmoke(world, boss, shown);
    if (boss.body.model.empty())
        return;
    BossBodyRig::StepSway(boss, body, seconds);
    BossBodySword::Step(boss, seconds);
    BossBodyPose pose = BossBodyRig::Solve(boss, body);
    BossBodySword::Trace(boss, body, pose);
    // 登場の間は、体に付く効果の置き場所（兜・刀・胴）を、演じている役者の骨のものに直す。
    BossIntroVisuals::ApplyPose(boss, pose, seconds);
    // フェーズの変わり目の演出の間は、膝をついて刀を突き立て、刀身を拭い、斬り上げて抜く姿に直す。
    BossPhaseShiftVisuals::ApplyPose(boss, body, pose);
    // 登場の役者が出ている間は、戦いの体の外形を出さない（役者が体を演じる。c-e0c500）。骨格の体がクリップを流して
    // いる間も同じ（c-7b1a06）。登場の間は骨格の体を出さない。
    const bool animated = !BossIntroVisuals::Showing(boss) && BossAnimBody::Update(world, boss, body, alpha, seconds);
    // 骨格の体が出ている間は、体に付く効果（刀光・刀の霞・手・胴・兜・変わり目の火と煙）を見えている骨格の骨に付ける
    // （c-7b1a09・c-bc5908）。
    if (animated)
        BossAnimBody::ApplyPose(world, boss, alpha, seconds, pose);
    if (BossIntroVisuals::Showing(boss))
        BossAnimBody::Stop(world, boss);
    if (!BossIntroVisuals::Showing(boss) && !animated)
        SubmitPose(world, object, boss, pose);
    BossPhaseShiftVisuals::Render(world, object, boss, pose, seconds);
    UpdateFireTongues(world, object, boss, pose);
    UpdateBladeShader(world, object, boss, pose.bladeHot, seconds);
    BossBodyAccents::Update(world, boss, shown, pose, seconds);
}

void BossBodyVisuals::UpdateClone(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                                  BossCloneComponent& clone, float seconds) {
    // 出ていない分身は地面の下にしまってあり、何も頼まない（頼まなかったフレームは消える）。
    HIKARI::GameProjectTransform body{};
    if (!clone.active || !boss.body.visible || boss.body.model.empty() || !object.GetWorldTransform(body))
        return;
    clone.bodyClock += std::max(0.0f, seconds);
    SubmitPose(world, object, boss, IdlePose(boss, body, clone.bodyClock));
}

void BossBodyVisuals::UpdatePreviews(HIKARI::WorldView& world) {
    // 演出ワークスペースがボスの登場の資産を流している間だけ、役者をその時刻の姿にする（流していない時は、触った
    // ものを元へ戻す＝ふだんの編集の見た目）。胴の煙・効果は出さない。
    world.ForEachComponent<BossComponent>(
        HIKARI::MakeComponentTypeId(kBossType),
        [&](HIKARI::ObjectView, BossComponent& boss) { BossIntroVisuals::Preview(world, boss); });
}

void BossBodyVisuals::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    BossVfx::StopHandle(world, boss.bodyState.smokeVfx);
    BossBodyAccents::Stop(world, boss);
    BossPhaseShiftVisuals::Stop(world, boss);
    BossAnimBody::Stop(world, boss);
}

} // namespace Project_TD2_2
