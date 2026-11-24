#include "BossAnimBody.h"

#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "../Boss/BossComponent.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossMath.h"
#include "../Boss/BossRules.h"

namespace Project_TD2_2 {

using BossMath::YawLeanSway;

namespace {

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
    fields.Note("流すクリップがある間は、部品ごとに置く戦いの体の外形を隠し、この物体（模型と動画の部品を持つ）をボスの"
                "足元と向きに置いて流します。技のクリップは技の表の「アニメーション」の列、長さはクリップの長さの表と区間の"
                "表で決まります。クリップが無い間は隠して、今までどおりの外形で動きます。当たりはまだ今までどおりの刀の線です。");
    fields.SceneObject("animBodyObject", "骨格の体の物体（模型と動画の部品を持つ。無ければ使わない）", object);
    fields.Float("animBodyScale", "骨格の体の大きさの倍率", scale, 0.1f, 10.0f);
    fields.Text("animBodyIdleClip", "技を出していない間に繰り返すクリップ（空なら技のクリップの間だけ出す）", idleClip);
    fields.Float("animBodyBlendSeconds", "クリップを替える時に混ぜる長さ (秒)", blendSeconds, 0.0f, 2.0f);
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

} // namespace Project_TD2_2
