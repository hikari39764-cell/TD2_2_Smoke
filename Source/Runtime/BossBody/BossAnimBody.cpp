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
}

bool BossAnimBody::Update(HIKARI::WorldView& world, BossComponent& boss, const HIKARI::GameProjectTransform& body,
                          float alpha) {
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
    const bool idle = !settings.idleClip.empty() && settings.idleClip != state.failedClip;
    if (moveClip == nullptr && !state.layerClip.empty()) {
        (void)animation.ClearMaskedPoseLayer(handle, settings.blendSeconds);
        state.layerClip.clear();
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
        // 時刻を指定する層は毎フレーム置き直すので、混ぜる長さは 0（0 より大きいと置き直すたびに混ぜ始めから
        // やり直し、重みが上がらず骨格の初期の姿＝T ポーズになる。登場の役者と同じ）。
        if (!animation.SetMaskedPoseLayer(handle, *moveClip, kSkeletonRoot, normalized, 1.0f, 0.0f))
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
    // 斬る向き：剣先の動き（次の F まで）の、刀に直交する成分（刀光が刃から先に進む向き）。動いていなければ決めない。
    const Vec3 tipNow = BossMath::Add(now.position, BossMath::Mul(out.direction, boss.swordLength));
    const Vec3 tipNext = BossMath::Add(next.position, BossMath::Mul(BladeDirection(next.rotation), boss.swordLength));
    const Vec3 across = BossMath::ProjectOut(BossMath::Sub(tipNext, tipNow), out.direction);
    if (BossMath::Length(across) > 1e-4f)
        out.edge = BossMath::Normalize(across);
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

void BossAnimBody::ApplyPose(BossComponent& boss, float alpha, BossBodyPose& pose) {
    BossAnimBodyState& state = boss.animBodyState;
    const BossMove* move = CurrentMove(boss);
    state.tipSpeed = 0.0f;
    if (!state.active || move == nullptr || !ClipMove(boss, *move))
        return;
    const BossClipSwordTrack& track = *TrackOf(boss, move->clip);
    const float frame = MoveElapsed(boss, *move) + std::clamp(alpha, 0.0f, 1.0f);
    const float reach = BossBodyMath::VisibleSwordLength(boss);
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
}

} // namespace Project_TD2_2
