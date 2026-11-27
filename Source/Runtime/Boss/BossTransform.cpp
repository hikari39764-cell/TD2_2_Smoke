#include "BossTransform.h"

#include <algorithm>
#include <cmath>

#include "BossIntro.h"
#include "BossPhaseShift.h"
#include "BossMath.h"
#include "BossPose.h"
#include "../BossBody/BossBodyVisuals.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 撃破の後に体を縮める下限の倍率（0 にすると変換が潰れるので、見えない大きさで止める）。
constexpr float kVanishMinScale = 0.001f;
// 体のゆれの周期（rad/秒）。
constexpr float kBobRate = 2.1f;

} // namespace

// 体の物体の置き場所：足元の高さ（跳躍・浮遊）に浮遊のゆれを足し、向き（回し蹴りの体の回転を含む）だけで回す。
// 体の姿勢（傾き・屈み）は骨格の体の動画が見せる（c-43f1a5。前は物体ごと傾けたので、骨格の体の足が滑った）。
void BossTransform::Apply(HIKARI::ObjectView object, BossComponent& boss, const BossIntroComponent* intro,
                          const BossPhaseShiftComponent* shift) {
    const float bob = std::sin(boss.floatClock * kBobRate) * boss.floatBob;
    HIKARI::GameProjectTransform transform{};
    transform.rotation = YawRotation(BossPose::BodyYaw(boss));
    transform.position = {boss.ground.x, boss.ground.y + BossPose::RootHeight(boss) + bob + boss.bodyHalfHeight,
                          boss.ground.z};
    // 撃破の後は、待ってから体を縮めて見えなくする（撃破の演出は別に作る）。
    float vanish = 1.0f;
    if (boss.state == BossState::Dead && boss.stateFrame > boss.deadVanishDelayFrames) {
        const float progress = boss.deadVanishFrames > 0
                                   ? static_cast<float>(boss.stateFrame - boss.deadVanishDelayFrames) /
                                         static_cast<float>(boss.deadVanishFrames)
                                   : 1.0f;
        vanish = std::max(kVanishMinScale, 1.0f - progress);
    }
    transform.scale = {vanish, vanish, vanish};
    // 登場の演技の前と間は、体の物体を舞台に止める（演出の資産のカメラは物体に付く。浮遊のゆれ・傾きを入れない。
    // c-e0c500）。上の傾き・構えの値は、いつも通り進めておく（演技の後の戦いの見た目を変えない）。
    if (BossIntro::Pinned(intro))
        transform = BossIntro::StageTransform(boss, *intro);
    // フェーズの変わり目の演出の間も同じ（資産のカメラは骨格の体の物体に付き、骨格の体はこの物体の足元に置かれる）。
    if (BossPhaseShift::Pinned(shift))
        transform = BossPhaseShift::StageTransform(boss, *shift);
    object.SetWorldTransform(transform);
    // 体は描画のフレームで、前の tick とこの tick の間を補間して置く。
    BossBodyVisuals::NoteTick(boss, transform.position);
}

void BossTransform::Describe(HIKARI::PropertyFields& fields, BossComponent& boss) {
    if (fields.Section("撃破の後")) {
        fields.Int("deadVanishDelayFrames", "撃破されてから体が縮み始めるまで (F)", boss.deadVanishDelayFrames, 0,
                   3600);
        fields.Int("deadVanishFrames", "体が縮んで消えるまでの長さ (F。0 で待ちの後すぐ消える)", boss.deadVanishFrames,
                   0, 3600);
        fields.Note("体の姿勢（傾き・屈み）は骨格の体の動画が見せます。地面や画面に予告は出しません（ソウルライク）。");
    }
}

} // namespace Project_TD2_2
