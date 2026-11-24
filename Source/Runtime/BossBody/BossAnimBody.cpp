#include "BossAnimBody.h"

#include <cstdio>

#include "HIKARI/Project/ActionViews.h"
#include "../Boss/BossComponent.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossMath.h"

namespace Project_TD2_2 {

using BossMath::YawLeanSway;

namespace {

// 骨格の体の模型の根の節点（Assets/Models/BossIntroActor。登場の役者と同じ模型）。隠す時はこの節点ごと隠す。
constexpr std::string_view kRootNode = "Boss_Rig";

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
                "足元と向きに置いて流します。クリップが無い間は隠して、今までどおりの外形で動きます。当たりと技の長さは"
                "まだ今までどおりです。");
    fields.SceneObject("animBodyObject", "骨格の体の物体（模型と動画の部品を持つ。無ければ使わない）", object);
    fields.Float("animBodyScale", "骨格の体の大きさの倍率", scale, 0.1f, 10.0f);
    fields.Text("animBodyIdleClip", "技を出していない間に繰り返すクリップ（空なら技のクリップの間だけ出す）", idleClip);
    fields.Float("animBodyBlendSeconds", "クリップを替える時に混ぜる長さ (秒)", blendSeconds, 0.0f, 2.0f);
}

bool BossAnimBody::Update(HIKARI::WorldView& world, BossComponent& boss, const HIKARI::GameProjectTransform& body) {
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
    // 流すクリップ：技が頼んだもの、無ければ待機のクリップ。
    const bool fromMove = !state.moveClip.empty();
    const std::string& clip = fromMove ? state.moveClip : settings.idleClip;
    if (clip.empty() || clip == state.failedClip) {
        Hide(object, state);
        state.playing.clear();
        return false;
    }
    const uint64_t handle = object.GetHandle().value;
    if (state.shown != 1)
        (void)animation.SetEvaluationEnabled(handle, 1u);
    Show(object, state);
    // ボスの足元（体の中心から体の高さの半分だけ下）と向きに置く。傾き・揺れは骨格の動画が受け持つ。
    HIKARI::GameProjectTransform placed{};
    placed.position = {body.position.x, body.position.y - boss.bodyHalfHeight * body.scale.y, body.position.z};
    placed.rotation = YawLeanSway(boss.yaw, 0.0f, 0.0f);
    placed.scale = {settings.scale, settings.scale, settings.scale};
    (void)object.SetWorldTransform(placed);
    if (state.playing != clip) {
        const bool loop = fromMove ? state.moveLoop : true;
        if (!animation.CrossFade(handle, clip, settings.blendSeconds, loop ? 1u : 0u, 1u)) {
            // 流せないクリップは、骨格の体を出さずに今までどおりの外形で動く（初期の姿の体を見せない）。
            if (state.failedClip != clip) {
                state.failedClip = clip;
                char message[256]{};
                std::snprintf(message, sizeof(message),
                              "[AnimBody] クリップ「%s」を流せません（模型にその名前のクリップが無い）", clip.c_str());
                BossLog::Warn(message);
            }
            Hide(object, state);
            return false;
        }
        state.playing = clip;
    }
    state.active = true;
    return true;
}

void BossAnimBody::Stop(HIKARI::WorldView& world, BossComponent& boss) {
    BossAnimBodyState& state = boss.animBodyState;
    state.active = false;
    state.playing.clear();
    if (boss.animBody.object == 0u)
        return;
    if (const HIKARI::ObjectView object = world.FindSceneObject(boss.animBody.object); object.IsValid())
        Hide(object, state);
}

bool BossAnimBody::Showing(const BossComponent& boss) { return boss.animBodyState.active; }

} // namespace Project_TD2_2
