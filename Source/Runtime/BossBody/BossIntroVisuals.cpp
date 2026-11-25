#include "BossIntroVisuals.h"

#include <algorithm>
#include <cmath>

#include "HIKARI/Project/SequenceView.h"
#include "BossBodyMath.h"
#include "BossIntroActor.h"
#include "BossIntroEffects.h"
#include "BossIntroSmoke.h"
#include "../Boss/BossFrame.h"
#include "../Boss/BossIntro.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;

// 流れているはずの演出の資産の時刻が、ゲームの時間でこれだけ進まなければ、自分の時計に切り替える (秒)。
constexpr float kSequenceStallSeconds = 0.25f;

// 演技の時計を演出の資産の時刻に合わせる（合わせたら true）。流れ終わった・止まったまま進まない時は false（ゲームの
// 時間で進める）。資産は受け付けられても、演出の機能が切ってあると時刻が進まないので、流れているのに
// kSequenceStallSeconds の間進まなければ、それからは自分の時計で流す（一時停止は待つ）。
bool FollowSequence(HIKARI::WorldView& world, BossIntroComponent& intro, float dt) {
    BossIntroState& state = intro.state;
    HIKARI::GameProjectSequencePlaybackSnapshot snapshot{};
    if (state.sequenceStalled || !state.sequence.IsValid() ||
        !world.GetSequence().GetSnapshot(state.sequence, snapshot))
        return false;
    if (snapshot.state == HIKARI::GameProjectSequencePlaybackState::Paused)
        return true;
    if (snapshot.state != HIKARI::GameProjectSequencePlaybackState::Playing)
        return false;
    if (snapshot.timeSeconds != state.sequenceSeen) {
        state.sequenceSeen = snapshot.timeSeconds;
        state.sequenceStall = 0.0f;
        // 資産は、このフレームの systems の後に、このフレームのゲームの時間だけ進み、カメラもその時刻で決まる。
        // 読める時刻より 1 歩先で演技を進める：役者の姿とカットの切り替わりが、同じフレームに来る（1 フレーム遅れると、
        // 片段が替わるカットの最初のフレームに、前の片段の最後の姿が写る）。
        state.showSeconds = snapshot.timeSeconds + dt * std::clamp(intro.settings.playbackRate, 0.01f, 8.0f);
        return true;
    }
    state.sequenceStall += dt;
    if (state.sequenceStall < kSequenceStallSeconds)
        return true;
    state.sequenceStalled = true;
    BossLog::Warn("[BossIntro] 演出の資産の時刻が進みません（演出の機能が切ってある？）。ゲームの時間で演技を流します");
    return false;
}

// 一息の段：戦いの体に替わった直後、肩に担いだ刀（演技の最後の置き場所）から、待機の構えへ下ろす。刀を握る右手と
// 籠手の筒は、刀に付けたまま一緒に動かす。
void SettleSword(BossIntroComponent& intro, BossBodyPose& pose, float seconds) {
    BossIntroState& state = intro.state;
    if (!state.carryValid || state.skipped)
        return;
    state.settleClock += std::max(0.0f, seconds);
    // 役者から替わった時から、一息の段の終わりまでに下ろし切る。
    const float length = std::max(0.2f, intro.settings.settleSeconds - intro.settings.settleSwapSeconds);
    const float w = length > kTiny ? Smooth(state.settleClock / length) : 1.0f;
    if (w >= 1.0f)
        return;
    const BossBodyPlacement solved = pose.sword;
    const Vec3 position = Lerp(state.carrySword.position, solved.position, w);
    const Quat rotation = Nlerp(state.carrySword.rotation, solved.rotation, Clamp01(w));
    const Quat turn =
        Multiply(rotation, Quat{-solved.rotation.x, -solved.rotation.y, -solved.rotation.z, solved.rotation.w});
    const auto carry = [&](BossBodyPlacement& part) {
        part.position = Add(position, Rotate(turn, Sub(part.position, solved.position)));
        part.rotation = Multiply(turn, part.rotation);
    };
    carry(pose.rightHand);
    carry(pose.rightCuff);
    pose.sword.position = position;
    pose.sword.rotation = rotation;
}

// 事件段を読み直した後に 1 回、役者が使う段を見分ける。
void EnsureRoles(BossIntroComponent& intro) {
    if (intro.state.cueRolesRead)
        return;
    intro.state.cueRolesRead = true;
    BossIntroActor::ReadCues(intro);
}

} // namespace

bool BossIntroVisuals::Showing(const BossIntroComponent* intro) {
    if (intro == nullptr)
        return false;
    const BossIntroPhase phase = intro->state.phase;
    return phase == BossIntroPhase::Scattered || phase == BossIntroPhase::Show || BossIntro::ActorLingers(intro);
}

void BossIntroVisuals::Advance(HIKARI::WorldView& world, const BossBodySettings& body, BossIntroComponent& intro,
                               float seconds) {
    BossIntroState& state = intro.state;
    state.seenPhase = state.phase;
    const float dt = std::max(0.0f, seconds);
    const HIKARI::GameProjectTransform stage = BossIntro::GroundTransform(intro);
    switch (state.phase) {
    case BossIntroPhase::Scattered:
        state.showSeconds = 0.0f;
        BossIntroActor::Drive(world, body, intro, &stage, 0.0f, false);
        break;
    case BossIntroPhase::Show:
        if (!FollowSequence(world, intro, dt))
            state.showSeconds += dt * std::max(0.0f, intro.settings.playbackRate);
        EnsureRoles(intro);
        // 役者の姿（姿の層・節点の偏移）は、エンジンの動画の更新が次のフレームに読む（こちらは LateUpdate、動画の更新は
        // Update）。もう 1 歩先の時刻を頼んでおくと、描かれる姿がカメラと同じ時刻になる。
        BossIntroActor::Drive(world, body, intro, &stage,
                              state.showSeconds + dt * std::clamp(intro.settings.playbackRate, 0.01f, 8.0f), true);
        // 演出の灯が置かれている所を控える（演技の後、役者が出ている間は、ここに保つ）。
        if (const HIKARI::ObjectView light = world.FindSceneObject(intro.settings.keyLightObject);
            intro.settings.keyLightObject != 0u && light.IsValid()) {
            state.actor.keyLightSeen = light.GetWorldTransform(state.actor.keyLightAt);
            HIKARI::GameProjectLightState lit{};
            if (light.GetLight().TryGetState(lit)) {
                state.actor.keyLightLevelSeen = true;
                state.actor.keyLightLevel = lit.enabled != 0u ? lit.intensity : 0.0f;
            }
        }
        break;
    default:
        // 一息の段の初め：役者を、演技の最後の姿のまま出しておく（カメラが引いてから、戦いの体に替える）。
        if (BossIntro::ActorLingers(&intro)) {
            BossIntroActor::Drive(world, body, intro, &stage, state.showSeconds, true);
            BossIntroActor::HoldKeyLight(world, intro);
            break;
        }
        // 演技が済んだ（飛ばした）：役者と落ちている部品を隠す（戦いの体に替わる）。
        if (!state.actorHidden) {
            state.actorHidden = true;
            BossIntroActor::Hide(world, intro);
        }
        break;
    }
}

HIKARI::GameProjectTransform BossIntroVisuals::BodyTransform(const BossComponent& boss, const BossIntroComponent* intro,
                                                             const HIKARI::GameProjectTransform& body) {
    HIKARI::GameProjectTransform shown = body;
    if (Showing(intro))
        (void)BossIntroActor::BodyTransform(boss, *intro, shown);
    return shown;
}

void BossIntroVisuals::ApplyPose(BossComponent& boss, BossIntroComponent& intro, BossBodyPose& pose, float seconds) {
    BossIntroState& state = intro.state;
    BossIntroEffects::Update(boss, intro, state.showSeconds, seconds);
    if (state.phase == BossIntroPhase::Settle && !BossIntro::ActorLingers(&intro)) {
        SettleSword(intro, pose, seconds);
        return;
    }
    if (!Showing(&intro))
        return;
    // 体に付く効果（目の光・刀身の光）の置き場所を、役者の骨から。目は Eyes の拍から灯り、兜が頭に付く前は出さない。
    const bool lit = pose.eyesLit;
    if (BossIntroActor::Pose(intro, pose) && state.phase == BossIntroPhase::Show) {
        // 演技の最後の刀の置き場所を覚えておく（一息の段で、ここから構えへ下ろす）。
        state.carryValid = true;
        state.carrySword = {pose.sword.position, pose.sword.rotation, {1.0f, 1.0f, 1.0f}};
        state.settleClock = 0.0f;
    }
    pose.eyesLit = lit && state.phase != BossIntroPhase::Scattered && state.eyesLit &&
                   state.actor.attached[static_cast<size_t>(BossIntroPart::Helm)];
    // 演技の間は、燃える刀身を使わない（フェーズ 1 の刀）。
    pose.bladeHot = false;
}

float BossIntroVisuals::SmokeScale(const BossIntroComponent* intro) {
    // 散らばって待つ間は、まだ体が無いので出さない。演技の間は、地を走る煙が鎧に届く拍から満ちる。登場の部品が無ければ
    // いつも満ちている。
    if (intro == nullptr)
        return 1.0f;
    return intro->state.phase == BossIntroPhase::Scattered ? 0.0f : BossIntroSmoke::BodyAmount(*intro);
}

void BossIntroVisuals::Preview(HIKARI::WorldView& world, const BossBodySettings& body, BossIntroComponent& intro) {
    BossIntroState& state = intro.state;
    HIKARI::GameProjectSequencePlaybackSnapshot snapshot{};
    const bool shown = !intro.settings.sequence.empty() && world.GetSequence().GetEditorPreview(snapshot) &&
                       intro.settings.sequence == snapshot.assetGuid;
    if (!shown) {
        // プレビューをやめた：触った見せ方・偏移・姿の層を元へ戻す。
        if (state.previewing) {
            state.previewing = false;
            BossIntroActor::Restore(world, intro);
        }
        return;
    }
    if (!state.previewing) {
        state.previewing = true;
        state.previewHandle = {};
        if (!BossIntroActor::Find(world, intro))
            return;
    }
    // 流し直した（手綱が変わった）時に、事件段を読み直す。
    if (snapshot.handle != state.previewHandle) {
        state.previewHandle = snapshot.handle;
        BossIntro::ReadCues(world, snapshot.handle, state);
    }
    EnsureRoles(intro);
    state.showSeconds = snapshot.timeSeconds;
    // 役者は、シーンに置いてある所のまま（舞台へ置き直さない）。
    BossIntroActor::Drive(world, body, intro, nullptr, state.showSeconds, true);
}

} // namespace Project_TD2_2
