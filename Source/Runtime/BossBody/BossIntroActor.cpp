#include "BossIntroActor.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <iterator>
#include <string_view>

#include "HIKARI/Project/ActionViews.h"
#include "HIKARI/Project/LightTypes.h"
#include "BossHelmGlowData.h"
#include "BossPoseKit.h"
#include "../Boss/BossFrame.h"

namespace Project_TD2_2 {
namespace {

using namespace BossPoseKit;

// 役者の模型の節点：全部の親（隠す時はここを隠す）と、姿の層を掛ける骨の根。
constexpr std::string_view kActorRoot = "Boss_Rig";
// 役者の草摺の節点（戦いの体の草摺を外してある間は、これも隠す）。
constexpr std::array<std::string_view, 7> kTassetNodes{"Kusazuri0", "Kusazuri1", "Kusazuri2", "Kusazuri3",
                                                       "Kusazuri4", "Kusazuri5", "Kusazuri6"};
constexpr std::string_view kSkeletonRoot = "Hips";
// 事件段の名前の頭。
constexpr std::string_view kAnimPrefix = "Anim.";
constexpr std::string_view kPartPrefix = "Part.";
constexpr std::string_view kFlySuffix = ".Fly";
constexpr std::string_view kTwitchSuffix = ".Twitch";
constexpr std::string_view kTwitchClip = "Twitch";

// 部品 1 つ：事件段での名前、付く骨、役者の側の同じ部品の節点（付くまで隠す）、落ちている物体の模型の全部の親と、
// 偏移を足す根の骨（模型の空間＝この骨の空間に作ってあるので、根の骨を役者の骨の世界の変換に重ねると、ぴったり重なる）。
struct PartDef {
    std::string_view name;
    std::string_view bone;
    std::array<std::string_view, 2> skin;
    std::string_view root;
    bool twitch;
};
constexpr std::array<PartDef, kBossIntroPartCount> kParts{{
    {"HandR", "RightHand", {"HandR", "CuffRMesh"}, "IntroGauntletR_Rig", true},
    {"HandL", "LeftHand", {"HandL", "CuffLMesh"}, "IntroGauntletL_Rig", true},
    {"Helm", "Head", {"KnightHelm", ""}, "IntroHelm_Rig", false},
}};

// 骨の空間から、外形の置き場所の約束（BossBodyRig.h）への、変わらない変換。役者の模型を作る時の値
// （D:/GAME PJ/TD2_2/BossAnim/intro/work/sockets.py が骨の静止姿勢と部品の置き場所から出す。骨格を作り直したら直す）。
struct SocketOffset {
    Vec3 position;
    Quat rotation;
};
constexpr SocketOffset kHelmOffset{{0.0f, 0.45625f, 0.05063f}, {0.104528f, 0.0f, 0.0f, 0.994522f}};
constexpr SocketOffset kSwordOffset{{0.0f, 0.0f, 0.0f}, {0.0f, 0.707107f, 0.707107f, 0.0f}};
constexpr SocketOffset kTorsoOffset{{0.0f, -0.30125f, 0.0675f}, {0.0f, 0.0f, 0.0f, 1.0f}};

// 事件段の名前（終端が無くても、入れ物の外は読まない）。
std::string_view NameOf(const BossIntroCue& cue) {
    const char* begin = cue.clip.name;
    return {begin, static_cast<size_t>(std::find(begin, std::end(cue.clip.name), '\0') - begin)};
}

uint64_t ObjectId(const BossIntroSettings& s, size_t part) {
    switch (static_cast<BossIntroPart>(part)) {
    case BossIntroPart::GauntletRight: return s.gauntletRightObject;
    case BossIntroPart::GauntletLeft: return s.gauntletLeftObject;
    default: return s.helmObject;
    }
}

// 役者の側の部品の節点（1 つか 2 つ）を見せる・隠す（同じ頼みは出さない）。
void ShowSkin(HIKARI::ObjectView actor, int8_t& asked, const PartDef& def, bool shown) {
    const int8_t want = shown ? 1 : 0;
    if (asked == want)
        return;
    bool done = true;
    for (const std::string_view node : def.skin)
        done = (node.empty() || actor.GetModel().SetNodeVisibility(node, shown).Succeeded()) && done;
    if (done)
        asked = want;
}

// 模型をまるごと隠す・見せる。節点ごとの頼みと根の頼みを重ねないよう、先にほかの頼みを全部外す。
void HideAll(HIKARI::ObjectView object, int8_t& asked, std::string_view root) {
    if (asked == 0)
        return;
    (void)object.GetModel().ClearAllNodeVisibility();
    if (object.GetModel().SetNodeVisibility(root, false).Succeeded())
        asked = 0;
}

void ShowAll(HIKARI::ObjectView object, int8_t& asked) {
    if (asked == 1)
        return;
    (void)object.GetModel().ClearAllNodeVisibility();
    asked = 1;
}

bool ReadSocket(const HIKARI::AnimationView& animation, uint64_t actor, std::string_view bone,
                HIKARI::GameProjectTransform& out) {
    return animation.GetSocketTransform(actor, bone, HIKARI::GameProjectAnimationSocketSpace::World, out);
}

// 骨の変換（1 フレーム前に描いた姿。動画の評価は systems の後）。
void ReadSockets(const HIKARI::AnimationView& animation, uint64_t actor, BossIntroActorState& a) {
    a.socketsValid = ReadSocket(animation, actor, "Hips", a.hips) && ReadSocket(animation, actor, "Spine2", a.chest) &&
                     ReadSocket(animation, actor, "Head", a.head) && ReadSocket(animation, actor, "Sword", a.sword) &&
                     ReadSocket(animation, actor, "RightArm", a.rightShoulder) &&
                     ReadSocket(animation, actor, "RightForeArm", a.rightElbow) &&
                     ReadSocket(animation, actor, "RightHand", a.rightHand) &&
                     ReadSocket(animation, actor, "LeftArm", a.leftShoulder) &&
                     ReadSocket(animation, actor, "LeftForeArm", a.leftElbow) &&
                     ReadSocket(animation, actor, "LeftHand", a.leftHand);
}

const HIKARI::GameProjectTransform& BoneOf(const BossIntroActorState& a, size_t part) {
    switch (static_cast<BossIntroPart>(part)) {
    case BossIntroPart::GauntletRight: return a.rightHand;
    case BossIntroPart::GauntletLeft: return a.leftHand;
    default: return a.head;
    }
}

BossBodyPlacement Placed(const HIKARI::GameProjectTransform& bone, const SocketOffset& offset, BossBodyPlacement base) {
    base.position = Add(bone.position, Rotate(bone.rotation, offset.position));
    base.rotation = Multiply(bone.rotation, offset.rotation);
    return base;
}

// 段の中の進み（0〜1）。段より前は 0、後は 1。長さ 0 の段は、始めを過ぎたら 1。
float Progress(const BossIntroCue& cue, float t) {
    const float duration = cue.clip.durationSeconds;
    if (t <= cue.clip.startSeconds)
        return 0.0f;
    return duration > kTiny ? std::clamp((t - cue.clip.startSeconds) / duration, 0.0f, 1.0f) : 1.0f;
}

// 部品を骨へ寄せる偏移（落ちている物体の空間。進み u は 0〜1）。手甲は吸われる（最後に速い）＋途中で浮く。兜は煙に
// 持ち上げられる（ゆっくり動き出してゆっくり止まる）＋途中で横へふくらむ。
HIKARI::GameProjectTransform FlyOffset(const BossIntroSettings& s, size_t part,
                                       const HIKARI::GameProjectTransform& rest,
                                       const HIKARI::GameProjectTransform& bone, float u) {
    const bool helm = static_cast<BossIntroPart>(part) == BossIntroPart::Helm;
    const float e = helm ? Smooth(u) : std::pow(u, std::max(1.0f, s.gauntletFlyPower));
    const Vec3 delta = Sub(bone.position, rest.position);
    Vec3 world = Mul(delta, e);
    const float arc = std::sin(kPi * u);
    if (helm) {
        const Vec3 side = Normalize(Cross({0.0f, 1.0f, 0.0f}, delta), {1.0f, 0.0f, 0.0f});
        world = Add(world, Mul(side, s.helmFlyBulge * arc));
    } else {
        world.y += s.gauntletFlyLift * arc * (1.0f - u);
    }
    const Quat inverse = Conjugate(rest.rotation);
    const float scale = std::max(kTiny, rest.scale.x);
    HIKARI::GameProjectTransform offset{};
    offset.position = Mul(Rotate(inverse, world), 1.0f / scale);
    offset.rotation = Blend({0.0f, 0.0f, 0.0f, 1.0f}, Multiply(inverse, bone.rotation), helm ? Smooth(u) : e);
    offset.scale = {1.0f, 1.0f, 1.0f};
    return offset;
}

// 演出の灯を点ける・消す（同じ頼みは出さない）。明るさと置き場所は、演出の資産の物体の轨が書く。
void SwitchLight(HIKARI::WorldView& world, const BossComponent& boss, int8_t& asked, bool on) {
    const int8_t want = on ? 1 : 0;
    if (asked == want || boss.intro.keyLightObject == 0u)
        return;
    const HIKARI::ObjectView light = world.FindSceneObject(boss.intro.keyLightObject);
    if (!light.IsValid())
        return;
    HIKARI::GameProjectLightPatch patch{};
    patch.fieldMask = static_cast<uint32_t>(HIKARI::GameProjectLightPatchField::Enabled);
    patch.enabled = on ? 1u : 0u;
    if (light.GetLight().ApplyPatch(patch).Succeeded())
        asked = want;
}

void ClearOffset(const HIKARI::AnimationView& animation, uint64_t object, const PartDef& def, bool& set) {
    if (!set)
        return;
    (void)animation.ClearNodeLocalTransformOffset(object, def.bone);
    set = false;
}

// 部品 1 つ：落ちている物体と、役者の側の同じ部品の、どちらを見せるか。落ちている間は、痙攣の片段と、骨へ寄せる偏移。
void DrivePart(HIKARI::WorldView& world, BossComponent& boss, HIKARI::ObjectView actor, size_t part, float t,
               bool started) {
    const PartDef& def = kParts[part];
    BossIntroState& state = boss.introState;
    BossIntroActorState& a = state.actor;
    const BossIntroCue* fly = nullptr;
    const BossIntroCue* twitch = nullptr;
    for (int k = 0; k < state.cueCount; ++k) {
        const BossIntroCue& cue = state.cues[static_cast<size_t>(k)];
        if (cue.part != static_cast<int8_t>(part))
            continue;
        if (cue.role == BossIntroCueRole::PartFly && fly == nullptr)
            fly = &cue;
        if (cue.role == BossIntroCueRole::PartTwitch && twitch == nullptr)
            twitch = &cue;
    }
    // 飛ぶ段が無ければ、初めから付いている。
    const float u = !started ? 0.0f : fly != nullptr ? Progress(*fly, t) : 1.0f;
    const bool attached = started && (fly == nullptr || t >= fly->clip.startSeconds + fly->clip.durationSeconds);
    a.fly[part] = u;
    a.attached[part] = attached;
    a.twitch[part] = started && twitch != nullptr && t < twitch->clip.startSeconds + twitch->clip.durationSeconds
                         ? Progress(*twitch, t)
                         : 0.0f;
    ShowSkin(actor, a.skinShown[part], def, attached);
    const HIKARI::ObjectView object = world.FindSceneObject(ObjectId(boss.intro, part));
    if (!object.IsValid())
        return;
    const uint64_t handle = object.GetHandle().value;
    const HIKARI::AnimationView animation = world.GetAnimation();
    if (attached) {
        HideAll(object, a.partShown[part], def.root);
        ClearOffset(animation, handle, def, a.offsetSet[part]);
        return;
    }
    if (a.partShown[part] != 1) {
        ShowAll(object, a.partShown[part]);
        (void)animation.SetEvaluationEnabled(handle, 1u);
    }
    if (def.twitch) {
        const float progress = started && twitch != nullptr ? Progress(*twitch, t) : 0.0f;
        const bool set = animation.SetMaskedPoseLayer(handle, kTwitchClip, def.bone, progress, 1.0f, 0.0f);
        if (progress > 0.0f && !a.twitchLogged[part]) {
            a.twitchLogged[part] = true;
            char message[128]{};
            std::snprintf(message, sizeof(message), "IntroTwitch %.*s set=%d t=%.2f", static_cast<int>(def.name.size()),
                          def.name.data(), static_cast<int>(set), static_cast<double>(t));
            BossLog::Write(message);
        }
    }
    HIKARI::GameProjectTransform rest{};
    if (!object.GetWorldTransform(rest))
        return;
    a.partAt[part] = rest.position;
    if (u <= 0.0f || !a.socketsValid) {
        ClearOffset(animation, handle, def, a.offsetSet[part]);
        return;
    }
    const HIKARI::GameProjectTransform offset = FlyOffset(boss.intro, part, rest, BoneOf(a, part), u);
    if (animation.SetNodeParentLocalTransformOffset(handle, def.bone, offset))
        a.offsetSet[part] = true;
    a.partAt[part] = Add(rest.position, Rotate(rest.rotation, Mul(offset.position, rest.scale.x)));
}

} // namespace

bool BossIntroActor::Find(HIKARI::WorldView& world, BossComponent& boss) {
    BossIntroActorState& a = boss.introState.actor;
    a = {};
    a.searched = true;
    const HIKARI::ObjectView actor = world.FindSceneObject(boss.intro.actorObject);
    if (boss.intro.actorObject == 0u || !actor.IsValid() || !world.GetAnimation().IsValid()) {
        BossLog::Warn("[BossIntro] 役者の物体が無いか、動画の機能（Animation）が切ってあります。登場をしません");
        return false;
    }
    a.ready = true;
    return true;
}

void BossIntroActor::ReadCues(BossComponent& boss) {
    BossIntroState& state = boss.introState;
    for (int k = 0; k < state.cueCount; ++k) {
        BossIntroCue& cue = state.cues[static_cast<size_t>(k)];
        const std::string_view name = NameOf(cue);
        cue.role = BossIntroCueRole::None;
        cue.part = -1;
        if (name.starts_with(kAnimPrefix) && name.size() > kAnimPrefix.size()) {
            cue.role = BossIntroCueRole::Anim;
            continue;
        }
        if (!name.starts_with(kPartPrefix))
            continue;
        const std::string_view rest = name.substr(kPartPrefix.size());
        for (size_t part = 0; part < kParts.size(); ++part) {
            const std::string_view partName = kParts[part].name;
            if (!rest.starts_with(partName))
                continue;
            const std::string_view tail = rest.substr(partName.size());
            if (tail == kFlySuffix)
                cue.role = BossIntroCueRole::PartFly;
            else if (tail == kTwitchSuffix)
                cue.role = BossIntroCueRole::PartTwitch;
            if (cue.role != BossIntroCueRole::None)
                cue.part = static_cast<int8_t>(part);
        }
    }
}

void BossIntroActor::Drive(HIKARI::WorldView& world, BossComponent& boss, const HIKARI::GameProjectTransform* stage,
                           float t, bool started) {
    BossIntroState& state = boss.introState;
    BossIntroActorState& a = state.actor;
    const HIKARI::ObjectView actor = world.FindSceneObject(boss.intro.actorObject);
    if (!a.ready || !actor.IsValid())
        return;
    if (stage != nullptr)
        (void)actor.SetWorldTransform(*stage);
    const uint64_t handle = actor.GetHandle().value;
    const HIKARI::AnimationView animation = world.GetAnimation();
    SwitchLight(world, boss, a.lightOn, started);
    if (a.actorShown != 1) {
        ShowAll(actor, a.actorShown);
        a.skinShown = {-1, -1, -1};
        (void)animation.SetEvaluationEnabled(handle, 1u);
        // 草摺を外してある間（Boss.bodyTassetCount が 0）は、役者の草摺も出さない。
        if (boss.body.tassetCount <= 0)
            for (const std::string_view node : kTassetNodes)
                (void)actor.GetModel().SetNodeVisibility(node, false);
    }
    // 片段：始まっている Anim の段のうち、始めが一番遅いもの。まだ無ければ、動画の部品に書いてある片段の最初の姿のまま。
    const BossIntroCue* clip = nullptr;
    for (int k = 0; started && k < state.cueCount; ++k) {
        const BossIntroCue& cue = state.cues[static_cast<size_t>(k)];
        if (cue.role == BossIntroCueRole::Anim && cue.clip.startSeconds <= t &&
            (clip == nullptr || cue.clip.startSeconds >= clip->clip.startSeconds))
            clip = &cue;
    }
    if (clip != nullptr) {
        // 段の長さ＝流す長さ：片段を、段の中の進みの時刻の姿で重ねる（時計は演出の資産。巻き戻しにも付いて行く）。
        const std::string_view name = NameOf(*clip).substr(kAnimPrefix.size());
        const bool set = animation.SetMaskedPoseLayer(handle, name, kSkeletonRoot, Progress(*clip, t), 1.0f, 0.0f);
        a.layerSet = set || a.layerSet;
        if (clip != a.loggedClip) {
            a.loggedClip = clip;
            HIKARI::GameProjectAnimationState animator{};
            const bool read = animation.GetState(handle, animator);
            char message[200]{};
            std::snprintf(message, sizeof(message),
                          "IntroClip %.*s set=%d t=%.2f state=%d valid=%d playing=%d duration=%.2f",
                          static_cast<int>(name.size()), name.data(), static_cast<int>(set), static_cast<double>(t),
                          static_cast<int>(read), static_cast<int>(animator.valid), static_cast<int>(animator.playing),
                          static_cast<double>(animator.durationSeconds));
            BossLog::Write(message);
        }
    } else if (a.layerSet) {
        (void)animation.ClearMaskedPoseLayer(handle, 0.0f);
        a.layerSet = false;
    }
    ReadSockets(animation, handle, a);
    // 刀が地面に入る所は、刀がまだ地に立っている間（演技の前。舞台の向きが決まるまで）に決めておく。
    if (a.socketsValid && (!started || !a.swordEntryValid)) {
        const Vec3 tip = Rotate(Multiply(a.sword.rotation, kSwordOffset.rotation), {0.0f, 0.0f, 1.0f});
        const float drop = a.sword.position.y - state.origin.y;
        a.swordEntry = tip.y > -0.2f ? Vec3{a.sword.position.x, state.origin.y, a.sword.position.z}
                                     : Add(a.sword.position, Mul(tip, drop / -tip.y));
        a.swordEntryValid = true;
    }
    for (size_t part = 0; part < kParts.size(); ++part)
        DrivePart(world, boss, actor, part, t, started);
}

void BossIntroActor::HoldKeyLight(HIKARI::WorldView& world, BossComponent& boss) {
    const BossIntroActorState& a = boss.introState.actor;
    if (!a.keyLightSeen || boss.intro.keyLightObject == 0u)
        return;
    const HIKARI::ObjectView light = world.FindSceneObject(boss.intro.keyLightObject);
    if (!light.IsValid())
        return;
    (void)light.SetWorldTransform(a.keyLightAt);
    if (!a.keyLightLevelSeen)
        return;
    // 演技の最後の明るさから、なめらかに 0 へ（役者を替える時には 0 にそろえる。替える時に灯を消しても、光が跳ねない）。
    const BossIntroSettings& s = boss.intro;
    const float fade = std::min(s.settleLightFadeSeconds, s.settleSwapSeconds);
    constexpr float kTicksPerSecond = 60.0f; // 一息の段の frame は、固定の tick（1 秒 60）で数える
    const float seconds = static_cast<float>(boss.introState.frame) / kTicksPerSecond;
    const float u = fade > 0.0f ? std::clamp(seconds / fade, 0.0f, 1.0f) : 1.0f;
    HIKARI::GameProjectLightPatch patch{};
    patch.fieldMask = static_cast<uint32_t>(HIKARI::GameProjectLightPatchField::Intensity);
    patch.intensity = a.keyLightLevel * (1.0f - u * u * (3.0f - 2.0f * u));
    (void)light.GetLight().ApplyPatch(patch);
}

void BossIntroActor::Hide(HIKARI::WorldView& world, BossComponent& boss) {
    BossIntroActorState& a = boss.introState.actor;
    if (!a.ready)
        return;
    SwitchLight(world, boss, a.lightOn, false);
    const HIKARI::AnimationView animation = world.GetAnimation();
    if (const HIKARI::ObjectView actor = world.FindSceneObject(boss.intro.actorObject); actor.IsValid()) {
        HideAll(actor, a.actorShown, kActorRoot);
        // 見えない間は、姿を評価しない。
        (void)animation.SetEvaluationEnabled(actor.GetHandle().value, 0u);
    }
    for (size_t part = 0; part < kParts.size(); ++part) {
        const HIKARI::ObjectView object = world.FindSceneObject(ObjectId(boss.intro, part));
        if (!object.IsValid())
            continue;
        HideAll(object, a.partShown[part], kParts[part].root);
        ClearOffset(animation, object.GetHandle().value, kParts[part], a.offsetSet[part]);
        (void)animation.SetEvaluationEnabled(object.GetHandle().value, 0u);
    }
    a.socketsValid = false;
}

void BossIntroActor::Restore(HIKARI::WorldView& world, BossComponent& boss) {
    BossIntroActorState& a = boss.introState.actor;
    const HIKARI::AnimationView animation = world.GetAnimation();
    SwitchLight(world, boss, a.lightOn, false);
    if (const HIKARI::ObjectView actor = world.FindSceneObject(boss.intro.actorObject); actor.IsValid()) {
        (void)actor.GetModel().ClearAllNodeVisibility();
        if (a.layerSet)
            (void)animation.ClearMaskedPoseLayer(actor.GetHandle().value, 0.0f);
    }
    for (size_t part = 0; part < kParts.size(); ++part) {
        const HIKARI::ObjectView object = world.FindSceneObject(ObjectId(boss.intro, part));
        if (!object.IsValid())
            continue;
        (void)object.GetModel().ClearAllNodeVisibility();
        ClearOffset(animation, object.GetHandle().value, kParts[part], a.offsetSet[part]);
        if (kParts[part].twitch)
            (void)animation.ClearMaskedPoseLayer(object.GetHandle().value, 0.0f);
    }
    a = {};
}

void BossIntroActor::SetHelmGlow(HIKARI::WorldView& world, BossComponent& boss, HIKARI::GameProjectFloat3 color,
                                 float strength) {
    BossIntroActorState& a = boss.introState.actor;
    const HIKARI::ObjectView actor = world.FindSceneObject(boss.intro.actorObject);
    if (boss.intro.actorObject == 0u || !actor.IsValid())
        return;
    const bool lit = strength > 0.001f && a.attached[static_cast<size_t>(BossIntroPart::Helm)];
    const int8_t want = lit ? 1 : 0;
    if (a.helmGlowShown != want && actor.GetModel().SetNodeVisibility(BossHelmGlowData::kActorNode, lit).Succeeded())
        a.helmGlowShown = want;
    if (!lit)
        return;
    (void)actor.GetModel().SetMaterialColorOverride(BossHelmGlowData::kActorMaterial, {0.0f, 0.0f, 0.0f}, 1.0f, color,
                                                    std::min(strength, 64.0f));
    if (!a.helmSocketLogged) {
        // 模型に入れた場所の印（HelmVisor）を、エンジンが名前で引けるかを 1 度だけ記録する（エフェクトを結ぶ時の確かめ）。
        a.helmSocketLogged = true;
        HIKARI::GameProjectTransform visor{};
        const bool found =
            world.GetAnimation().GetSocketTransform(actor.GetHandle().value, BossHelmGlowData::kVisorSocket,
                                                    HIKARI::GameProjectAnimationSocketSpace::World, visor);
        char message[160]{};
        std::snprintf(message, sizeof(message),
                      "IntroHelmSocket HelmVisor found=%d at=(%.2f, %.2f, %.2f) head=(%.2f, %.2f, %.2f)",
                      static_cast<int>(found), static_cast<double>(visor.position.x),
                      static_cast<double>(visor.position.y), static_cast<double>(visor.position.z),
                      static_cast<double>(a.head.position.x), static_cast<double>(a.head.position.y),
                      static_cast<double>(a.head.position.z));
        BossLog::Write(message);
    }
}

void BossIntroActor::SetHelmPartGlint(HIKARI::WorldView& world, BossComponent& boss, HIKARI::GameProjectFloat3 color,
                                      float strength) {
    BossIntroActorState& a = boss.introState.actor;
    const bool lit = strength > 0.001f && a.ready && !a.attached[static_cast<size_t>(BossIntroPart::Helm)];
    if (!lit && !a.helmPartLit)
        return;
    const HIKARI::ObjectView helm = world.FindSceneObject(boss.intro.helmObject);
    if (boss.intro.helmObject == 0u || !helm.IsValid())
        return;
    if (lit) {
        (void)helm.GetModel().SetMaterialColorOverride(BossHelmGlowData::kPartMaterial, {0.0f, 0.0f, 0.0f}, 1.0f, color,
                                                       std::min(strength, 64.0f));
        a.helmPartLit = true;
    } else {
        (void)helm.GetModel().ClearMaterialColorOverride(BossHelmGlowData::kPartMaterial);
        a.helmPartLit = false;
    }
}

void BossIntroActor::SetBladeEdge(HIKARI::WorldView& world, BossComponent& boss, HIKARI::GameProjectFloat3 color,
                                  float strength) {
    BossIntroActorState& a = boss.introState.actor;
    const HIKARI::ObjectView actor = world.FindSceneObject(boss.intro.actorObject);
    if (boss.intro.actorObject == 0u || !actor.IsValid())
        return;
    if (strength > 0.001f) {
        (void)actor.GetModel().SetMaterialColorOverride(BossHelmGlowData::kActorBladeEdgeMaterial,
                                                        {0.46f, 0.48f, 0.52f}, 1.0f, color, std::min(strength, 64.0f));
        a.bladeEdgeLit = true;
    } else if (a.bladeEdgeLit) {
        (void)actor.GetModel().ClearMaterialColorOverride(BossHelmGlowData::kActorBladeEdgeMaterial);
        a.bladeEdgeLit = false;
    }
}

bool BossIntroActor::Pose(const BossComponent& boss, BossBodyPose& pose) {
    const BossIntroActorState& a = boss.introState.actor;
    if (!a.socketsValid)
        return false;
    pose.head = Placed(a.head, kHelmOffset, pose.head);
    pose.sword = Placed(a.sword, kSwordOffset, pose.sword);
    pose.torso = Placed(a.chest, kTorsoOffset, pose.torso);
    return true;
}

bool BossIntroActor::BodyTransform(const BossComponent& boss, HIKARI::GameProjectTransform& body) {
    const BossIntroActorState& a = boss.introState.actor;
    if (!a.socketsValid)
        return false;
    const float ground = boss.introState.origin.y;
    body.position = {a.hips.position.x, ground + boss.bodyHalfHeight * body.scale.y, a.hips.position.z};
    // 腰の骨の前（骨の空間の +Z。役者の模型の腰・胸・兜の骨は、体と同じ軸の約束）を水平に寝かせた向き。
    const Vec3 forward = FlatNormalize(Rotate(a.hips.rotation, {0.0f, 0.0f, 1.0f}), Forward(boss.introState.yaw));
    body.rotation = YawLeanSway(YawOf(forward), 0.0f, 0.0f);
    return true;
}

HIKARI::GameProjectFloat3 BossIntroActor::SwordEntry(const BossComponent& boss) {
    const BossIntroActorState& a = boss.introState.actor;
    return a.swordEntryValid ? a.swordEntry : boss.introState.origin;
}

} // namespace Project_TD2_2
