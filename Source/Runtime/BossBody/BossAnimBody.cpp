#include "BossAnimBody.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "BossBodyMath.h"
#include "BossBodyRig.h"
#include "../Boss/BossComponent.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossMath.h"
#include "../Boss/BossRules.h"

namespace Project_TD2_2 {

using BossMath::YawLeanSway;

namespace {

using BossMath::Quat;
using BossMath::Vec3;

// 刀の骨の空間から、外形の刀の置き場所の約束（BossBodyRig.h：+Z が剣先）への、変わらない回転。登場の役者
// （BossIntroActor.cpp の kSwordOffset）と同じ模型・同じ骨なので同じ値（骨格を作り直したら一緒に直す）。
constexpr Quat kSwordAxes{0.0f, 0.707107f, 0.707107f, 0.0f};
// 兜・胴の骨から、外形の兜・胴の置き場所の約束への変換（BossIntroActor.cpp の kHelmOffset・kTorsoOffset と同じ値。
// 体に付く効果の置き場所を、見えている骨格の骨から出す。c-bc5908）。
constexpr Vec3 kHelmOffset{0.0f, 0.45625f, 0.05063f};
constexpr Quat kHelmTurn{0.104528f, 0.0f, 0.0f, 0.994522f};
constexpr Vec3 kTorsoOffset{0.0f, -0.30125f, 0.0675f};
// 袖口は、前腕の骨から手の骨へこの割合の所。
constexpr float kCuffAlong = 0.75f;
// 草摺の節点（模型の節点。登場の役者と同じ）。
constexpr std::array<std::string_view, 7> kTassetNodes{"Kusazuri0", "Kusazuri1", "Kusazuri2", "Kusazuri3",
                                                       "Kusazuri4", "Kusazuri5", "Kusazuri6"};
// 規則の時計の 1 F（60 Hz）の秒。クリップは秒で読む。
constexpr float kSecondsPerFrame = 1.0f / 60.0f;

// 骨格の体の模型の根の節点（Assets/Models/BossIntroActor。登場の役者と同じ模型）。隠す時はこの節点ごと隠す。
constexpr std::string_view kRootNode = "Boss_Rig";
// 技のクリップを重ねる層の根（骨格の一番上の骨。ここから下の骨全部にクリップの姿を重ねる）。
constexpr std::string_view kSkeletonRoot = "Hips";

// 技の規則の時計：技を始めてからの経過 F（前隙・攻撃中・後隙を通して）。
float MoveElapsed(const BossComponent& boss, const BossMove& move) {
    const float t = static_cast<float>(boss.stateFrame);
    switch (boss.phase) {
    case BossActPhase::Pre: return t;
    case BossActPhase::Act: return static_cast<float>(move.pre) + t;
    default: return static_cast<float>(move.pre + move.act) + t;
    }
}

void Show(HIKARI::ObjectView object, BossAnimBodyState& state) {
    if (state.shown == 1)
        return;
    (void)object.GetModel().ClearAllNodeVisibility();
    state.shown = 1;
}

Quat Conjugate(Quat q) { return {-q.x, -q.y, -q.z, q.w}; }

// 2 つの回転の間（短い方の向きで混ぜて長さ 1 に戻す。隣り合う F の間なので十分）。
Quat Nlerp(Quat a, Quat b, float t) {
    const float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    const float sign = dot < 0.0f ? -1.0f : 1.0f;
    Quat q{a.x + (b.x * sign - a.x) * t, a.y + (b.y * sign - a.y) * t, a.z + (b.z * sign - a.z) * t,
           a.w + (b.w * sign - a.w) * t};
    const float length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (length < 1e-6f)
        return a;
    return {q.x / length, q.y / length, q.z / length, q.w / length};
}

const BossClipSwordTrack* TrackOf(const BossComponent& boss, const std::string& clip) {
    for (const BossClipSwordTrack& track : boss.animBodyState.swordTracks) {
        if (track.clip == clip)
            return &track;
    }
    return nullptr;
}

// いま出している技（技の最中でなければ nullptr）。
const BossMove* CurrentMove(const BossComponent& boss) {
    return boss.state == BossState::Act ? BossRules::GetMove(boss, boss.move) : nullptr;
}

// 待機のクリップ（刀を肩に担ぐ姿）を出してよい間（c-40f6c1）：待機、移動の技（横歩き・寄る・下がる）、霞歩（回避の
// 横っ飛び・怯み跳び）、クリップのある技。気絶・撃破・コードで姿を作るほかの技の間は、骨格の体を隠して今までの外形で動く。
bool IdleAllowed(const BossComponent& boss, const BossMove* move) {
    if (boss.state == BossState::Idle)
        return true;
    return move != nullptr &&
           (BossMoveTable::IsMoveType(move->type) || move->type == BossMoveType::KasumiStep || !move->clip.empty());
}

// クリップの刀の骨（模型の空間）の、規則の時計の frame F の所（F の間は補間）。
struct ClipSword {
    Vec3 position{};
    Quat rotation{0.0f, 0.0f, 0.0f, 1.0f};
    Vec3 leftHand{};
};

ClipSword SampleTrack(const BossClipSwordTrack& track, float frame) {
    const size_t count = track.swordPosition.size();
    const float f = std::clamp(frame, 0.0f, static_cast<float>(count - 1));
    const size_t i = std::min(count - 1, static_cast<size_t>(f));
    const size_t j = std::min(count - 1, i + 1);
    const float t = f - static_cast<float>(i);
    return {BossMath::Lerp(track.swordPosition[i], track.swordPosition[j], t),
            Nlerp(track.swordRotation[i], track.swordRotation[j], t),
            BossMath::Lerp(track.leftHand[i], track.leftHand[j], t)};
}

// 刀身の向き（模型の空間。剣先へ。長さ 1）。
Vec3 BladeDirection(Quat rotation) {
    return BossMath::Normalize(BossMath::Rotate(BossMath::Multiply(rotation, kSwordAxes), {0.0f, 0.0f, 1.0f}));
}

// 見えている骨格の骨（世界）。骨は模型の空間で読み（1 フレーム前に描いた姿）、いまの置き場所（足元・向き・大きさ）を
// 掛ける：体が動く・回る時に 1 フレーム遅れて跳ばない（c-bc5908）。
struct Bones {
    HIKARI::GameProjectTransform chest, head, sword, rightArm, rightFore, rightHand, leftArm, leftFore, leftHand;
    std::array<HIKARI::GameProjectTransform, kTassetNodes.size()> tassets{};
    bool tassetsRead = false;
};

bool ReadBones(const HIKARI::AnimationView& animation, uint64_t handle, const BossAnimBodySettings& settings,
               const HIKARI::GameProjectTransform& placed, Bones& out) {
    const auto read = [&](std::string_view name, HIKARI::GameProjectTransform& bone) {
        HIKARI::GameProjectTransform local{};
        if (!animation.GetSocketTransform(handle, name, HIKARI::GameProjectAnimationSocketSpace::Local, local))
            return false;
        bone.position = BossMath::Add(placed.position,
                                      BossMath::Rotate(placed.rotation, BossMath::Mul(local.position, placed.scale.x)));
        bone.rotation = BossMath::Multiply(placed.rotation, local.rotation);
        bone.scale = placed.scale;
        return true;
    };
    if (!(read("Spine2", out.chest) && read("Head", out.head) && read(settings.swordNode, out.sword) &&
          read("RightArm", out.rightArm) && read("RightForeArm", out.rightFore) && read("RightHand", out.rightHand) &&
          read("LeftArm", out.leftArm) && read("LeftForeArm", out.leftFore) &&
          read(settings.leftHandNode, out.leftHand)))
        return false;
    out.tassetsRead = true;
    for (size_t k = 0; k < kTassetNodes.size(); ++k)
        out.tassetsRead = read(kTassetNodes[k], out.tassets[k]) && out.tassetsRead;
    return true;
}

void Put(BossBodyPlacement& placement, Vec3 position, Quat rotation) {
    placement.position = position;
    placement.rotation = rotation;
}

// 体に付く効果の置き場所を骨から（大きさは外形の体のまま。効果の大きさが変わらない）。
void PoseFromBones(const Bones& b, BossBodyPose& pose) {
    Put(pose.sword, b.sword.position, BossMath::Multiply(b.sword.rotation, kSwordAxes));
    Put(pose.head, BossMath::Add(b.head.position, BossMath::Rotate(b.head.rotation, kHelmOffset)),
        BossMath::Multiply(b.head.rotation, kHelmTurn));
    Put(pose.torso, BossMath::Add(b.chest.position, BossMath::Rotate(b.chest.rotation, kTorsoOffset)),
        b.chest.rotation);
    Put(pose.rightHand, b.rightHand.position, b.rightHand.rotation);
    Put(pose.leftHand, b.leftHand.position, b.leftHand.rotation);
    Put(pose.rightCuff, BossMath::Lerp(b.rightFore.position, b.rightHand.position, kCuffAlong), b.rightFore.rotation);
    Put(pose.leftCuff, BossMath::Lerp(b.leftFore.position, b.leftHand.position, kCuffAlong), b.leftFore.rotation);
    Put(pose.sode[0], b.rightArm.position, b.rightArm.rotation);
    Put(pose.sode[1], b.leftArm.position, b.leftArm.rotation);
    if (b.tassetsRead) {
        for (int k = 0; k < pose.tassetCount && static_cast<size_t>(k) < b.tassets.size(); ++k)
            Put(pose.tassets[static_cast<size_t>(k)], b.tassets[static_cast<size_t>(k)].position,
                b.tassets[static_cast<size_t>(k)].rotation);
    }
}

void Hide(HIKARI::ObjectView object, BossAnimBodyState& state) {
    if (state.shown == 0)
        return;
    (void)object.GetModel().ClearAllNodeVisibility();
    if (object.GetModel().SetNodeVisibility(kRootNode, false).Succeeded())
        state.shown = 0;
}

} // namespace

void BossAnimBodySettings::Describe(HIKARI::PropertyFields& fields) {
    if (!fields.Section("骨格の体（技をモーションで見せる体。登場の役者と同じ骨格）", false))
        return;
    fields.Note(
        "流すクリップがある間は、部品ごとに置く戦いの体の外形を隠し、この物体（模型と動画の部品を持つ）をボスの"
        "足元と向きに置いて流します。技のクリップは技の表の「アニメーション」の列、長さはクリップの長さの表と区間の"
        "表で決まります。クリップが無い間は隠して、今までどおりの外形で動きます。「クリップの刀で当てる」を入れると、"
        "刀の当たり・斬撃の面・刀の煙・刀光がクリップの刀の骨に付いて行きます（当たりの窓は区間の表どおり）。");
    fields.SceneObject("animBodyObject", "骨格の体の物体（模型と動画の部品を持つ。無ければ使わない）", object);
    fields.Float("animBodyScale", "骨格の体の大きさの倍率", scale, 0.1f, 10.0f);
    fields.Text("animBodyIdleClip", "技を出していない間に繰り返すクリップ（空なら技のクリップの間だけ出す）", idleClip);
    fields.Float("animBodyBlendSeconds", "クリップを替える時に混ぜる長さ (秒)", blendSeconds, 0.0f, 2.0f);
    fields.Bool("animBodyClipSword", "クリップの刀で当てる（当たり・斬撃・刀の煙・刀光をクリップの刀の骨に合わせる）",
                clipSword);
    fields.Text("animBodySwordNode", "刀の骨の名前（模型の節点）", swordNode);
    fields.Text("animBodyLeftHandNode", "左手の骨の名前（模型の節点。左手の技の弾を出す所）", leftHandNode);
    fields.Float("animBodyTwoHandNear", "両手の霧の刀：左手が柄からこの距離 (m) 以下で両手で握った", twoHandNear, 0.0f,
                 10.0f);
    fields.Float("animBodyTwoHandFar", "…この距離 (m) 以上で片手（間はなめらかに）", twoHandFar, 0.0f, 10.0f);
    fields.Float("animBodyTwoHandReach", "…両手で握った刀の当たりを剣先の先へ伸ばす長さ (m)", twoHandReach, 0.0f,
                 20.0f);
    fields.Float("animBodyTwoHandMist", "…刀身に沿って場の霧へ入れる霧（毎秒。剣の煙と同じ単位。0 は出さない）",
                 twoHandMist, 0.0f, 500.0f);
    fields.Float("animBodyTwoHandMistRadius", "…霧を入れる半径 (m)", twoHandMistRadius, 0.1f, 10.0f);
    fields.Float("animBodyLeftHandMist", "左手にまとう霧（毎秒。剣の煙と同じ単位。0 は出さない）", leftHandMist, 0.0f,
                 500.0f);
    fields.Float("animBodyLeftHandMistRadius", "…霧を入れる半径 (m)", leftHandMistRadius, 0.1f, 10.0f);
    fields.Float("animBodyLeftHandMistSwirl", "…霧が左手の周りを回る速さ (m/秒)", leftHandMistSwirl, 0.0f, 50.0f);
}

bool BossAnimBody::Update(HIKARI::WorldView& world, BossComponent& boss, const HIKARI::GameProjectTransform& body,
                          float alpha, float seconds) {
    const BossAnimBodySettings& settings = boss.animBody;
    BossAnimBodyState& state = boss.animBodyState;
    state.active = false;
    if (settings.object == 0u)
        return false;
    const HIKARI::ObjectView object = world.FindSceneObject(settings.object);
    const HIKARI::AnimationView animation = world.GetAnimation();
    if (!object.IsValid() || !animation.IsValid()) {
        if (!state.warned) {
            state.warned = true;
            BossLog::Warn("[AnimBody] 骨格の体の物体が無いか、動画の機能（Animation）が切ってあります。骨格の体を使いません");
        }
        return false;
    }
    const uint64_t handle = object.GetHandle().value;
    // 技のクリップ：技を出している間、表のアニメーションの列のクリップ。
    const BossMove* move = boss.state == BossState::Act ? BossRules::GetMove(boss, boss.move) : nullptr;
    const std::string* moveClip = move != nullptr && !move->clip.empty() && move->clip != state.failedClip ? &move->clip
                                                                                                             : nullptr;
    const bool idle = !settings.idleClip.empty() && settings.idleClip != state.failedClip && IdleAllowed(boss, move);
    if (moveClip == nullptr && !state.layerClip.empty()) {
        (void)animation.ClearMaskedPoseLayer(handle, settings.blendSeconds);
        state.layerClip.clear();
        state.layerOffSeconds = 0.0f;
    }
    if (moveClip == nullptr) {
        state.layerWeight = 0.0f;
        state.layerOffSeconds += std::max(0.0f, seconds);
    }
    if (moveClip == nullptr && !idle) {
        Hide(object, state);
        state.playing.clear();
        return false;
    }
    if (state.shown != 1)
        (void)animation.SetEvaluationEnabled(handle, 1u);
    const auto fail = [&](const std::string& clip) {
        // 流せないクリップは、骨格の体を出さずに今までどおりの外形で動く（初期の姿の体を見せない）。
        if (state.failedClip != clip) {
            state.failedClip = clip;
            char message[256]{};
            std::snprintf(message, sizeof(message), "[AnimBody] クリップ「%s」を流せません（模型にその名前のクリップが無い）",
                          clip.c_str());
            BossLog::Warn(message);
        }
        Hide(object, state);
        return false;
    };
    // 下の層：待機のクリップを繰り返す（無ければ骨格の初期の姿のまま、技のクリップの層が全身を覆う）。
    if (idle && state.playing != settings.idleClip) {
        if (!animation.CrossFade(handle, settings.idleClip, settings.blendSeconds, 1u, 1u))
            return fail(settings.idleClip);
        state.playing = settings.idleClip;
    }
    // 上の層：技のクリップを、技の規則の時計の時刻の姿で重ねる。長さはクリップの長さ（表から決めた時）か技の長さ。
    if (moveClip != nullptr) {
        // 下の層に流せるクリップが無いと、動画の部品は骨格の初期の姿のままで、上の層も効かない（T ポーズになった）。
        // 待機のクリップが無い時は、技のクリップを下の層に置いて止めておく（姿は上の層が時刻で決める）。
        if (!idle && state.playing != *moveClip) {
            if (!animation.Play(handle, *moveClip, 0u, 1u))
                return fail(*moveClip);
            (void)animation.Pause(handle);
            state.playing = *moveClip;
        }
        const float length = static_cast<float>(move->clipFrames > 0 ? move->clipFrames : move->pre + move->act + move->post);
        const float elapsed = MoveElapsed(boss, *move) + std::clamp(alpha, 0.0f, 1.0f);
        const float normalized = std::clamp(elapsed / std::max(1.0f, length), 0.0f, 1.0f);
        // 待機の姿から重ね始める時（層を外し切った後）は、重みを混ぜる長さで 0 から上げる（技の始めの姿へ跳ばない）。
        // 技から技へ続く時・外している途中で次の技が始まった時は 1 のまま（1 つの層しか無いので、混ぜる元が無い）。
        if (state.layerClip != *moveClip)
            state.layerFromIdle =
                idle && state.layerClip.empty() && state.layerOffSeconds >= std::max(0.0f, settings.blendSeconds);
        const float blendFrames = std::max(1.0f, settings.blendSeconds * 60.0f);
        const float weight = state.layerFromIdle ? BossMath::Smooth(elapsed / blendFrames) : 1.0f;
        state.layerWeight = weight;
        // 時刻を指定する層は毎フレーム置き直すので、混ぜる長さは 0（0 より大きいと置き直すたびに混ぜ始めから
        // やり直し、重みが上がらず骨格の初期の姿＝T ポーズになる。登場の役者と同じ）。重みは毎フレームここで決める。
        if (!animation.SetMaskedPoseLayer(handle, *moveClip, kSkeletonRoot, normalized, weight, 0.0f))
            return fail(*moveClip);
        if (state.layerClip != *moveClip) {
            HIKARI::GameProjectAnimationState animator{};
            const bool read = animation.GetState(handle, animator);
            char message[256]{};
            std::snprintf(message, sizeof(message),
                          "[AnimBody] MoveClip %s frames=%d move=%s state=%d valid=%d duration=%.2f weight=%.2f base=%s",
                          moveClip->c_str(), move->clipFrames, move->name.c_str(), static_cast<int>(read),
                          static_cast<int>(animator.valid), static_cast<double>(animator.durationSeconds),
                          static_cast<double>(animator.transitionWeight), state.playing.c_str());
            BossLog::Write(message);
        }
        state.layerClip = *moveClip;
    }
    Show(object, state);
    // ボスの足元（体の中心から体の高さの半分だけ下）と向きに置く。傾き・揺れは骨格の動画が受け持つ。
    HIKARI::GameProjectTransform placed{};
    placed.position = {body.position.x, body.position.y - boss.bodyHalfHeight * body.scale.y, body.position.z};
    placed.rotation = YawLeanSway(boss.yaw, 0.0f, 0.0f);
    placed.scale = {settings.scale, settings.scale, settings.scale};
    (void)object.SetWorldTransform(placed);
    state.placed = placed;
    state.active = true;
    return true;
}

void BossAnimBody::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    BossAnimBodyState& state = boss.animBodyState;
    state.active = false;
    state.playing.clear();
    if (boss.animBody.object == 0u)
        return;
    if (const HIKARI::ObjectView object = world.FindSceneObject(boss.animBody.object); object.IsValid()) {
        if (!state.layerClip.empty() && world.GetAnimation().IsValid())
            (void)world.GetAnimation().ClearMaskedPoseLayer(object.GetHandle().value, 0.0f);
        state.layerClip.clear();
        Hide(object, state);
    }
}

bool BossAnimBody::Showing(const BossComponent& boss) { return boss.animBodyState.active; }

void BossAnimBody::PrepareSword(HIKARI::WorldView& world, BossComponent& boss) {
    const BossAnimBodySettings& settings = boss.animBody;
    BossAnimBodyState& state = boss.animBodyState;
    if (!settings.clipSword || settings.object == 0u)
        return;
    // 読んでいないクリップ（表を読み直した時に増えたものも）。
    std::vector<const BossMove*> wanted;
    for (const BossMove& move : boss.table.GetMoves()) {
        if (move.clip.empty() || move.clipFrames <= 0 || TrackOf(boss, move.clip) != nullptr)
            continue;
        if (std::find(state.swordTrackFailed.begin(), state.swordTrackFailed.end(), move.clip) !=
            state.swordTrackFailed.end())
            continue;
        bool listed = false;
        for (const BossMove* other : wanted)
            listed = listed || other->clip == move.clip;
        if (!listed)
            wanted.push_back(&move);
    }
    if (wanted.empty())
        return;
    const HIKARI::ObjectView object = world.FindSceneObject(settings.object);
    const HIKARI::AnimationView animation = world.GetAnimation();
    if (!object.IsValid() || !animation.IsValid())
        return;
    const uint64_t handle = object.GetHandle().value;
    const std::array<std::string_view, 2> names{settings.swordNode, settings.leftHandNode};
    for (const BossMove* move : wanted) {
        BossClipSwordTrack track;
        track.clip = move->clip;
        const size_t count = static_cast<size_t>(move->clipFrames) + 1u;
        track.swordPosition.reserve(count);
        track.swordRotation.reserve(count);
        track.leftHand.reserve(count);
        track.leftHandRotation.reserve(count);
        bool ok = true;
        for (size_t f = 0; f < count && ok; ++f) {
            // 模型の空間（Local。節点の全体の行列）で読む。体の置き場所（足元・向き・大きさ）は使う時に掛ける。
            std::array<HIKARI::GameProjectTransform, 2> out{};
            ok = animation.SampleSocketTransformsAtTime(handle, move->clip, static_cast<float>(f) * kSecondsPerFrame,
                                                        0u, names.data(), static_cast<uint32_t>(names.size()),
                                                        HIKARI::GameProjectAnimationSocketSpace::Local, out.data());
            track.swordPosition.push_back(out[0].position);
            track.swordRotation.push_back(out[0].rotation);
            track.leftHand.push_back(out[1].position);
            track.leftHandRotation.push_back(out[1].rotation);
        }
        char message[320]{};
        if (!ok) {
            state.swordTrackFailed.push_back(move->clip);
            std::snprintf(message, sizeof(message),
                          "[AnimBody] クリップ「%s」の刀の骨（%s・%s）を読めません。この技は今までの構えの刀で当てます",
                          move->clip.c_str(), settings.swordNode.c_str(), settings.leftHandNode.c_str());
            BossLog::Warn(message);
            continue;
        }
        const Vec3 tip0 = BossMath::Add(track.swordPosition.front(),
                                        BossMath::Mul(BladeDirection(track.swordRotation.front()), boss.swordLength));
        std::snprintf(message, sizeof(message),
                      "[AnimBody] SwordTrack %s frames=%zu grip0=(%.2f,%.2f,%.2f) tip0=(%.2f,%.2f,%.2f)",
                      move->clip.c_str(), count, static_cast<double>(track.swordPosition.front().x),
                      static_cast<double>(track.swordPosition.front().y),
                      static_cast<double>(track.swordPosition.front().z), static_cast<double>(tip0.x),
                      static_cast<double>(tip0.y), static_cast<double>(tip0.z));
        BossLog::Write(message);
        state.swordTracks.push_back(std::move(track));
    }
}

bool BossAnimBody::ClipMove(const BossComponent& boss, const BossMove& move) {
    return boss.animBody.clipSword && boss.animBody.object != 0u && !move.clip.empty() && move.clipFrames > 0 &&
           TrackOf(boss, move.clip) != nullptr;
}

bool BossAnimBody::ClipSwordPose(const BossComponent& boss, BossSwordPose& out) {
    const BossMove* move = CurrentMove(boss);
    if (move == nullptr || !ClipMove(boss, *move))
        return false;
    const BossClipSwordTrack& track = *TrackOf(boss, move->clip);
    const float frame = MoveElapsed(boss, *move);
    const float scale = boss.animBody.scale;
    const ClipSword now = SampleTrack(track, frame);
    const ClipSword next = SampleTrack(track, frame + 1.0f);
    out = BossSwordPose{};
    out.grip = BossMath::Mul(now.position, scale);
    out.direction = BladeDirection(now.rotation);
    out.twoHand = 0.0f;
    // 両手で握った刀は霧をまとい、当たりが剣先の先へ伸びる（c-40f6c1）。
    out.reach = boss.animBody.twoHandReach * TwoHand(boss);
    // 斬る向き：剣先の動き（次の F まで）の、刀に直交する成分（刀光が刃から先に進む向き）。動いていなければ決めない。
    const Vec3 tipNow = BossMath::Add(now.position, BossMath::Mul(out.direction, boss.swordLength));
    const Vec3 tipNext = BossMath::Add(next.position, BossMath::Mul(BladeDirection(next.rotation), boss.swordLength));
    const Vec3 across = BossMath::ProjectOut(BossMath::Sub(tipNext, tipNow), out.direction);
    if (BossMath::Length(across) > 1e-4f)
        out.edge = BossMath::Normalize(across);
    return true;
}

float BossAnimBody::TwoHand(const BossComponent& boss) {
    const BossMove* move = CurrentMove(boss);
    if (move == nullptr || !ClipMove(boss, *move))
        return 0.0f;
    const BossAnimBodySettings& s = boss.animBody;
    const ClipSword now = SampleTrack(*TrackOf(boss, move->clip), MoveElapsed(boss, *move));
    const float gap = BossMath::Length(BossMath::Sub(now.leftHand, now.position));
    const float u = (s.twoHandFar - gap) / std::max(1e-3f, s.twoHandFar - s.twoHandNear);
    return BossMath::Smooth(u);
}

bool BossAnimBody::BoneSwordPose(const BossComponent& boss, BossSwordPose& out) {
    const BossAnimBodyState& state = boss.animBodyState;
    const BossMove* move = CurrentMove(boss);
    if (!state.active || !state.swordSeen || !IdleAllowed(boss, move) || (move != nullptr && ClipMove(boss, *move)))
        return false;
    const Vec3 root{boss.ground.x, boss.ground.y + BossPose::RootHeight(boss), boss.ground.z};
    out = BossSwordPose{};
    out.grip = BossMath::RotateY(BossMath::Sub(state.swordGripWorld, root), -boss.yaw);
    out.direction = BossMath::RotateY(state.swordDirectionWorld, -boss.yaw);
    out.twoHand = 0.0f;
    return true;
}

bool BossAnimBody::LeftHandPoint(const BossComponent& boss, HIKARI::GameProjectFloat3& out) {
    const BossMove* move = CurrentMove(boss);
    if (move == nullptr || !ClipMove(boss, *move))
        return false;
    const ClipSword now = SampleTrack(*TrackOf(boss, move->clip), MoveElapsed(boss, *move));
    const Vec3 root{boss.ground.x, boss.ground.y + BossPose::RootHeight(boss), boss.ground.z};
    out = BossMath::Add(root, BossMath::RotateY(BossMath::Mul(now.leftHand, boss.animBody.scale), boss.yaw));
    return true;
}

void BossAnimBody::ApplyPose(HIKARI::WorldView& world, BossComponent& boss, float alpha, float seconds,
                             BossBodyPose& pose) {
    BossAnimBodyState& state = boss.animBodyState;
    const BossMove* move = CurrentMove(boss);
    state.tipSpeed = 0.0f;
    state.leftHandSeen = false;
    state.swordSeen = false;
    if (!state.active) {
        state.lastTipValid = false;
        return;
    }
    // 体に付く効果（刀光・刀の霞・手の光・まとう霧・胴の火と煙・目・変わり目の刀身の火）は、見えている骨格の骨の所に
    // 付ける（待機の姿の間も。c-40f6c1・c-bc5908）。
    const HIKARI::ObjectView object = world.FindSceneObject(boss.animBody.object);
    Bones bones;
    if (object.IsValid() && world.GetAnimation().IsValid() &&
        ReadBones(world.GetAnimation(), object.GetHandle().value, boss.animBody, state.placed, bones)) {
        PoseFromBones(bones, pose);
        state.leftHandWorld = pose.leftHand.position;
        state.leftHandSeen = true;
        state.swordGripWorld = pose.sword.position;
        state.swordDirectionWorld = BossMath::Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
        state.swordSeen = true;
        if (!state.bonesLogged) {
            state.bonesLogged = true;
            HIKARI::GameProjectTransform seen{};
            const bool read =
                world.GetAnimation().GetSocketTransform(object.GetHandle().value, boss.animBody.swordNode,
                                                        HIKARI::GameProjectAnimationSocketSpace::World, seen);
            char message[200]{};
            std::snprintf(message, sizeof(message),
                          "[AnimBody] Bones tassets=%d sword=(%.2f,%.2f,%.2f) world=%d (%.2f,%.2f,%.2f)",
                          static_cast<int>(bones.tassetsRead), static_cast<double>(pose.sword.position.x),
                          static_cast<double>(pose.sword.position.y), static_cast<double>(pose.sword.position.z),
                          static_cast<int>(read), static_cast<double>(seen.position.x),
                          static_cast<double>(seen.position.y), static_cast<double>(seen.position.z));
            BossLog::Write(message);
        }
    }
    const float reach = BossBodyMath::VisibleSwordLength(boss);
    // クリップの刀で当たる技（層が重なり切った後）：刀は規則の時計の時刻のクリップの刀（当たり・斬撃と同じ）。
    if (move == nullptr || !ClipMove(boss, *move) || state.layerWeight < 0.999f) {
        if (!state.swordSeen)
            return;
        const Vec3 tip = BossMath::Add(state.swordGripWorld, BossMath::Mul(state.swordDirectionWorld, reach));
        pose.swordTipClearance = tip.y - boss.ground.y;
        if (state.lastTipValid && seconds > 0.0f)
            state.tipSpeed = BossMath::Length(BossMath::Sub(tip, state.lastTip)) / seconds;
        state.lastTip = tip;
        state.lastTipValid = true;
        return;
    }
    const BossClipSwordTrack& track = *TrackOf(boss, move->clip);
    const float frame = MoveElapsed(boss, *move) + std::clamp(alpha, 0.0f, 1.0f);
    const Quat body = state.placed.rotation;
    const float scale = state.placed.scale.x;
    // 模型の空間の刀 → 見えている刀（骨格の体の足元・向き・大きさ）。剣先は刀の向き（+Z）へ見える刀の長さ。
    const auto place = [&](const ClipSword& sword, BossBodyPlacement& out) {
        out.position =
            BossMath::Add(state.placed.position, BossMath::Rotate(body, BossMath::Mul(sword.position, scale)));
        out.rotation = BossMath::Multiply(BossMath::Multiply(body, sword.rotation), kSwordAxes);
        return BossMath::Add(out.position, BossMath::Mul(BossMath::Rotate(out.rotation, {0.0f, 0.0f, 1.0f}), reach));
    };
    const Vec3 tip = place(SampleTrack(track, frame), pose.sword);
    BossBodyPlacement later = pose.sword;
    const Vec3 tipLater = place(SampleTrack(track, frame + 1.0f), later);
    pose.swordTipClearance = tip.y - boss.ground.y;
    state.tipSpeed = BossMath::Length(BossMath::Sub(tipLater, tip)) / kSecondsPerFrame;
    state.lastTip = tip;
    state.lastTipValid = true;
}

} // namespace Project_TD2_2
