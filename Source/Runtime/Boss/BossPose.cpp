#include "BossPose.h"

#include <algorithm>
#include <cmath>

#include "BossMath.h"

namespace Project_TD2_2 {
namespace {

using namespace BossMath;

// 角度で書いた剣の向き（度）。x は前後の傾き（正で下）、y は左右（正でボスの左）。試作の POSE と同じ約束。
// 両手の型（BossSwingShape）にしていない技の構えは、肩の付け根（Boss.swordShoulder）からこの向きへ剣を出す。
struct Angles {
    float x = 0.0f;
    float y = 0.0f;
};

constexpr Angles kRest{70.0f, -10.0f};
constexpr Angles kUp{-110.0f, 0.0f};
constexpr Angles kStunned{105.0f, -25.0f};
// 投げナイフ：左手で投げる間、刀は右の外で後ろ上から前下へ小さく振る（体の前を横切らない）。
constexpr Angles kKnifeStart{5.0f, -130.0f};
constexpr Angles kKnifeEnd{20.0f, -60.0f};
// 回し蹴りの間、剣を横に構えたまま回る。分身を出す間は剣をまっすぐ前へ（試作の held と [0, 0]）。
constexpr Angles kHeld{20.0f, -80.0f};
constexpr Angles kForward{0.0f, 0.0f};
// 天降り斬りで落ちる・着地した剣：切っ先を真下へ向けて突き立てる。落ちる進みのこの倍で向け終える（落ちる前半で下を向く）。
constexpr Angles kPlunge{90.0f, 0.0f};
constexpr float kPlungeTurnSpeed = 2.0f;
// 炎走り：前隙は剣を右後ろへ引きずる（切っ先が地面すれすれ。左手で刀身をなぞる間は片手）。前線を放つたびに両手で前の
// 高い所へ斬り上げ（縦の型を逆に回す）、放つ間隔のこの割合で上げ切って、残りで低い構えへ戻す。
constexpr Angles kDrag{60.0f, -160.0f};
constexpr float kFlameRunUpShare = 0.6f;
// 炎走りの前隙のこの割合から、引きずった構えを両手に持ち替えて斬り上げの低い構えへ寄せる（左手で刀身をなぞり終えた後）。
constexpr float kFlameRunGripShare = 0.8f;
// フェイントで見せる振り出しの進み（振りの 0〜1）、引き戻す長さ (F)、溜めの震え（角度 度・1 F あたりの位相 rad）。
constexpr float kFeintShowShare = 0.16f;
constexpr int kFeintPullbackFrames = 7;
constexpr float kFeintTremorDegrees = 2.0f;
constexpr float kFeintTremorRate = 0.8f;
// 剣の根元（剣の長さに対する割合）と、霞の内側の始まり。
constexpr float kSwordBaseShare = 0.14f;
constexpr float kSwordInnerShare = 0.65f;
// 混ぜた刃の向きがこれより短ければ、刃の向きは決まっていないとする。
constexpr float kEdgeMinLength = 0.1f;

Vec3 DirectionOf(Angles a) {
    const float px = a.x * kRadiansPerDegree, py = a.y * kRadiansPerDegree;
    return {std::cos(px) * std::sin(py), -std::sin(px), std::cos(px) * std::cos(py)};
}

// 角度で書いた構え：肩の付け根から、その向きへ片手で剣を出す（捻り・腰の落としは見た目が刀の向きから決める）。
BossSwordPose FromAngles(const BossComponent& boss, Angles a) {
    BossSwordPose pose;
    pose.grip = boss.swordShoulder;
    pose.direction = DirectionOf(a);
    return pose;
}

// 構えを混ぜる：握りは体のまわり、刀の向きは大円、刃の向きは決まっている方へ寄せる。
BossSwordPose Blend(const BossSwordPose& a, const BossSwordPose& b, float k) {
    BossSwordPose pose;
    pose.grip = LerpAroundY(a.grip, b.grip, k);
    pose.direction = Slerp(Normalize(a.direction), Normalize(b.direction), k);
    const Vec3 edge = ProjectOut(Lerp(a.edge, b.edge, k), pose.direction);
    pose.edge = Length(edge) > kEdgeMinLength ? Normalize(edge) : Vec3{};
    pose.crouch = Lerp(a.crouch, b.crouch, k);
    pose.lean = Lerp(a.lean, b.lean, k);
    pose.twist = Lerp(a.twist, b.twist, k);
    pose.twoHand = Lerp(a.twoHand, b.twoHand, k);
    return pose;
}

BossSwordPose Sweep(const BossSwordPose& a, const BossSwordPose& c, float k) { return Blend(a, c, Smooth(k)); }

// 両手の型の、軸のまわり degrees 度の構え。mirror は左右に映した型（返しの振り）。sense は振る向き（角度が増える向き
// へ振る時 1、減る向き＝逆に回す時 -1）で、斬る向き（刃を向ける先）を決める。
BossSwordPose ShapePose(const BossSwingShape& shape, bool mirror, float degrees, float sense) {
    const auto flip = [mirror](Vec3 v) { return mirror ? Vec3{-v.x, v.y, v.z} : v; };
    // 映した回転の軸は、x の成分を残して y・z を反転する（向きを持つ軸は鏡で裏返る）。
    const Vec3 axis =
        Normalize(mirror ? Vec3{shape.axis.x, -shape.axis.y, -shape.axis.z} : shape.axis, {0.0f, 1.0f, 0.0f});
    const Vec3 pivot = flip(shape.pivot);
    const float span = shape.toDegrees - shape.fromDegrees;
    const float share = std::abs(span) > 1e-3f ? (degrees - shape.fromDegrees) / span : 0.0f;
    const float wrist = Lerp(shape.wristFromDegrees, shape.wristToDegrees, share);
    BossSwordPose pose;
    pose.grip = Add(pivot, RotateAbout(Sub(flip(shape.grip), pivot), axis, degrees * kRadiansPerDegree));
    pose.direction =
        Normalize(RotateAbout(Normalize(flip(shape.direction)), axis, (degrees + wrist) * kRadiansPerDegree));
    // 斬る向き：構えから振り終わりへ回る向き。
    const Vec3 sweep = Mul(Cross(axis, pose.direction), sense);
    pose.edge = Normalize(ProjectOut(sweep, pose.direction), {0.0f, -1.0f, 0.0f});
    // 振り込むほど沈み、前へ傾く（構えの値から振り終わりの値へ、振りの進みで寄せる）。
    const float weight = Smooth(share);
    pose.crouch = Lerp(shape.crouch, shape.crouchTo, weight);
    pose.lean = Lerp(shape.lean, shape.leanTo, weight);
    pose.twist = (mirror ? -1.0f : 1.0f) * Lerp(shape.twistFromDegrees, shape.twistToDegrees, share);
    pose.twoHand = 1.0f;
    return pose;
}

// 一撃の振り方：両手の型を（mirror で左右に映して）from 度から to 度まで回す。
struct Strike {
    const BossSwingShape* shape = nullptr;
    bool mirror = false;
    float from = 0.0f;
    float to = 0.0f;
};

Strike ShapeStrike(const BossSwingShape& shape, bool mirror) {
    return {&shape, mirror, shape.fromDegrees, shape.toDegrees};
}

// 炎走りの斬り上げ：縦の型を逆に回す。
Strike RiseStrike(const BossComponent& boss) {
    return {&boss.swing.vertical, false, boss.swing.riseFromDegrees, boss.swing.riseToDegrees};
}

// 振りの進み e（0＝構え、1＝振り終わり）の構え。
BossSwordPose StrikeAt(const Strike& strike, float e) {
    return ShapePose(*strike.shape, strike.mirror, Lerp(strike.from, strike.to, e),
                     strike.to >= strike.from ? 1.0f : -1.0f);
}

BossSwordPose StrikeCocked(const Strike& strike) { return StrikeAt(strike, 0.0f); }
BossSwordPose StrikeEnd(const Strike& strike) { return StrikeAt(strike, 1.0f); }

// 振りの進み：ゆっくり動き始めて加速し、swingPeakShare で最も速く、振り終わりへ減速して止まる。速さは途中で途切れない
// （進みを peak で速さが最大になるように曲げてから、両端で速さ 0 の 5 次の曲線に通す）。
float SwingProgress(const BossComponent& boss, int tIn, int length) {
    const float u = std::clamp(static_cast<float>(tIn) / static_cast<float>(std::max(1, length)), 0.0f, 1.0f);
    const float peak = std::clamp(boss.swingPeakShare, 0.5f, 0.9f);
    const float x = std::pow(u, std::log(0.5f) / std::log(peak));
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

BossSwordPose Swing(const BossComponent& boss, const Strike& strike, int tIn, int length) {
    return StrikeAt(strike, SwingProgress(boss, tIn, length));
}

// 振り上げ：今の構え previous から一撃の構えへ（k は 0〜1）。型の raiseVia が 0 より大きければ、前半で型の途中の構えへ
// 寄せ、後半で型を逆にたどって構えへ上げる（縦の振りかぶりで、刀の向きが真逆へ変わる時に体の中を通らない）。
BossSwordPose Raise(const BossSwordPose& previous, const Strike& strike, float k) {
    const float via = std::clamp(strike.shape->raiseVia, 0.0f, 1.0f);
    if (via <= 0.0f)
        return Sweep(previous, StrikeCocked(strike), k);
    const float s = Smooth(k);
    if (s < 0.5f)
        return Blend(previous, StrikeAt(strike, via), s * 2.0f);
    return StrikeAt(strike, via * (1.0f - (s - 0.5f) * 2.0f));
}

// 溜めの震え：刀の向きを、刀と上の向きに直交する軸のまわりに小さく揺らす。
BossSwordPose Tremble(BossSwordPose pose, int t) {
    const Vec3 axis = Cross(pose.direction, {0.0f, 1.0f, 0.0f});
    const float angle = std::sin(static_cast<float>(t) * kFeintTremorRate) * kFeintTremorDegrees * kRadiansPerDegree;
    pose.direction = Normalize(RotateAbout(pose.direction, axis, angle), pose.direction);
    return pose;
}

// 振り上げ→構え。剣を振る方向と反対へ大きく引いて、構えの間は止める。
// フェイント（遅）は、いつものテンポで「振り出し」まで見せてから引き戻し、溜めて（小さく震えて）待つ。
BossSwordPose Windup(const BossComponent& boss, const BossSwordPose& previous, const Strike& strike, int tIn,
                     int length, bool feint) {
    const BossSwordPose cocked = StrikeCocked(strike);
    const int base = feint ? std::max(1, length - boss.feintFrames) : length;
    const int kamae = std::min(boss.kamaeFrames, base - 1);
    const int raiseEnd = std::max(1, base - kamae);
    const int raiseStart = std::max(0, raiseEnd - boss.raiseFrames);
    if (tIn < base || !feint) {
        if (tIn < raiseStart)
            return previous;
        if (tIn < raiseEnd)
            return Raise(previous, strike,
                         static_cast<float>(tIn - raiseStart) / static_cast<float>(std::max(1, raiseEnd - raiseStart)));
        return cocked;
    }
    const int t = tIn - base;
    const int swingIn = std::max(1, boss.swingInFrames);
    if (t <= swingIn)
        return StrikeAt(strike, kFeintShowShare * Smooth(static_cast<float>(t) / static_cast<float>(swingIn)));
    const BossSwordPose fake = StrikeAt(strike, kFeintShowShare);
    if (t <= swingIn + kFeintPullbackFrames)
        return Sweep(fake, cocked, static_cast<float>(t - swingIn) / static_cast<float>(kFeintPullbackFrames));
    return Tremble(cocked, t);
}

// 連撃の index 番目（0 から）の一撃の振り方。横の一撃は左右を往復するので、手前の横の数で向きが決まる。
// 一撃の無い行は、横の一撃 1 つとして扱う。呼ぶたびに配列を作らない（毎 tick 呼ばれる）。
Strike StrikeOf(const BossComponent& boss, const BossMove& move, size_t index) {
    bool side = false;
    for (size_t i = 0; i < move.strikes.size(); ++i) {
        Strike strike{};
        switch (move.strikes[i].direction) {
        case BossStrikeDirection::Horizontal:
            strike = ShapeStrike(boss.swing.horizontal, side);
            side = !side;
            break;
        case BossStrikeDirection::Vertical: strike = ShapeStrike(boss.swing.vertical, false); break;
        case BossStrikeDirection::Kesa: strike = ShapeStrike(boss.swing.kesa, false); break;
        case BossStrikeDirection::ReverseKesa: strike = ShapeStrike(boss.swing.kesa, true); break;
        }
        if (i == index)
            return strike;
    }
    return ShapeStrike(boss.swing.horizontal, false);
}

int SegmentLength(const BossComponent& boss, const BossMove& move, size_t index) {
    if (BossMoveTable::IsMoveType(move.type) && index == 0)
        return std::max(1, boss.actFrames);
    return move.segments[index].length;
}

} // namespace

BossSegmentProgress BossPose::Progress(const BossComponent& boss, const BossMove& move) {
    BossSegmentProgress result;
    int accumulated = 0, hitIndex = 0;
    for (size_t k = 0; k < move.segments.size(); ++k) {
        const BossSegment& segment = move.segments[k];
        const int length = SegmentLength(boss, move, k);
        if (boss.stateFrame <= accumulated + length) {
            result.segment = static_cast<int>(k);
            result.hitIndex = hitIndex;
            result.tIn = boss.stateFrame - accumulated;
            result.swingLength = std::max(1, std::min(length, boss.swingInFrames + boss.swingFrames));
            result.hit = segment.hit;
            result.progress = segment.hit ? static_cast<float>(result.tIn) / static_cast<float>(result.swingLength)
                                          : static_cast<float>(result.tIn) / static_cast<float>(length);
            return result;
        }
        if (segment.hit)
            ++hitIndex;
        accumulated += length;
    }
    result.segment = std::max(0, static_cast<int>(move.segments.size()) - 1);
    result.hitIndex = hitIndex;
    result.progress = 1.0f;
    return result;
}

namespace {

// 後隙・待機の戻り：from から下ろした構えへ、frame F 目（全体 frames F）までに戻す。
BossSwordPose BackToRest(const BossComponent& boss, const BossSwordPose& from, int frame, int frames) {
    return Sweep(from, BossPose::Rest(boss), Share(frame, frames));
}

// 待機：技・気絶の終わりの構え（lastEndPose）を保ったまま入った時、poseRecoverFrames F かけて下ろした構えへ戻る。
// 0 は待機の最初の tick で切り替える（これまでの動き）。
BossSwordPose IdlePose(const BossComponent& boss) {
    return boss.poseRecoverFrames > 0 ? BackToRest(boss, boss.lastEndPose, boss.idleFrames, boss.poseRecoverFrames)
                                      : BossPose::Rest(boss);
}

// 気絶：技を打ち切った時の構え（lastEndPose）から、poseRecoverFrames F かけて崩れた構え（kStunned）へ。0 は一気に
// 切り替える（これまでの動き）。
BossSwordPose StunPose(const BossComponent& boss) {
    const BossSwordPose stunned = FromAngles(boss, kStunned);
    return boss.poseRecoverFrames > 0 ? Sweep(boss.lastEndPose, stunned, Share(boss.stateFrame, boss.poseRecoverFrames))
                                      : stunned;
}

// 剣を決まった向きに保つ技（回し蹴り・分身）：前隙で、前の技の構えからその向きへ移す。
BossSwordPose HeldPose(const BossComponent& boss, const BossMove& move, Angles held) {
    const BossSwordPose pose = FromAngles(boss, held);
    return boss.phase == BossActPhase::Pre ? Sweep(boss.moveStartPose, pose, Share(boss.stateFrame, move.pre)) : pose;
}

// 後隙：振り終わりを保ち、最後の poseRecoverFrames F で下ろした構えへ戻す（腰の落とし・前傾・両手も一緒に戻るので、
// 見える刀が待機の構えへ戻る間に体だけ屈んだまま残らない）。
BossSwordPose HoldThenRest(const BossComponent& boss, const BossSwordPose& end, int post) {
    const int recover = std::min(boss.poseRecoverFrames, post);
    if (recover <= 0)
        return end;
    return Sweep(end, BossPose::Rest(boss), Share(boss.stateFrame - (post - recover), recover));
}

// ひとつの振りの技（前隙で構え、攻撃中に振り、後隙は振り終わりを保ってから下ろす）。
BossSwordPose SingleStrikePose(const BossComponent& boss, const BossMove& move, const Strike& strike) {
    if (boss.phase == BossActPhase::Pre)
        return Windup(boss, boss.moveStartPose, strike, boss.stateFrame, move.pre, false);
    if (boss.phase == BossActPhase::Post)
        return HoldThenRest(boss, StrikeEnd(strike), move.post);
    const auto g = BossPose::Progress(boss, move);
    return g.hit ? Swing(boss, strike, g.tIn, g.swingLength) : StrikeEnd(strike);
}

// 突進横なぎ：ひとつの振りと同じで、後隙だけ下ろした構えへ戻す。
BossSwordPose DashPose(const BossComponent& boss, const BossMove& move) {
    if (boss.phase == BossActPhase::Post)
        return BackToRest(boss, boss.lastEndPose, boss.stateFrame, move.post);
    return SingleStrikePose(boss, move, ShapeStrike(boss.swing.horizontal, false));
}

// 退く間は剣を下げたまま。降りたら振り上げて、溜めの間ずっと振りかぶったまま構える（長い溜めを剣で見せる）。
BossSwordPose RetreatPose(const BossComponent& boss, const BossMove& move) {
    const Strike strike = ShapeStrike(boss.swing.vertical, false);
    if (boss.phase != BossActPhase::Pre)
        return SingleStrikePose(boss, move, strike);
    if (!boss.retreat.charging)
        return Sweep(boss.moveStartPose, BossPose::Rest(boss),
                     Share(boss.stateFrame, std::clamp(boss.retreatStepFrames, 1, std::max(1, move.pre - 1))));
    return Raise(BossPose::Rest(boss), strike,
                 static_cast<float>(boss.retreat.chargeElapsed) / static_cast<float>(std::max(1, boss.raiseFrames)));
}

// 1 撃目は左上→右下、2 撃目は右上→左下（X 字）。間の当たりの無い区間で、1 撃目の振り終わりから 2 撃目の構えへ
// 振り上げる（前は間の区間も 2 撃目の振りで、間に入る tick に構えが跳んでいた）。
BossSwordPose JumpPose(const BossComponent& boss, const BossMove& move) {
    const Strike first = ShapeStrike(boss.swing.kesa, false);
    const Strike second = ShapeStrike(boss.swing.kesa, true);
    if (boss.phase == BossActPhase::Pre)
        return Windup(boss, boss.moveStartPose, first, boss.stateFrame, move.pre, false);
    if (boss.phase == BossActPhase::Post)
        return BackToRest(boss, boss.lastEndPose, boss.stateFrame, move.post);
    const auto g = BossPose::Progress(boss, move);
    if (g.hit)
        return Swing(boss, g.hitIndex % 2 == 0 ? first : second, g.tIn, g.swingLength);
    return Raise(StrikeEnd(first), second, g.progress);
}

// 剣を頭上へ掲げ、切っ先を下へ向けて地面へ突き立て、溜めの間・爆ぜる間は突き立てたまま。後隙で抜いて戻す。
BossSwordPose DetonatePose(const BossComponent& boss, const BossMove& move) {
    const int t = boss.stateFrame;
    const BossSwordPose plunge = FromAngles(boss, kPlunge);
    if (boss.phase == BossActPhase::Post)
        return Sweep(plunge, BossPose::Rest(boss), static_cast<float>(t) / static_cast<float>(std::max(1, move.post)));
    if (boss.phase != BossActPhase::Pre)
        return plunge;
    const float progress = static_cast<float>(t) / static_cast<float>(std::max(1, move.pre));
    const float raise = std::clamp(boss.detonateRaiseShare, 0.05f, 0.9f);
    const float plant = std::max(raise + 0.01f, boss.detonatePlantShare);
    const BossSwordPose up = FromAngles(boss, kUp);
    return progress < raise ? Sweep(boss.moveStartPose, up, progress / raise)
                            : Sweep(up, plunge, (progress - raise) / (plant - raise));
}

BossSwordPose FlameRunPose(const BossComponent& boss, const BossMove& move) {
    const int t = boss.stateFrame;
    const BossSwordPose drag = FromAngles(boss, kDrag);
    const Strike rise = RiseStrike(boss);
    const BossSwordPose low = StrikeCocked(rise), up = StrikeEnd(rise);
    if (boss.phase == BossActPhase::Pre) {
        const float share = Share(t, move.pre);
        if (share >= kFlameRunGripShare)
            return Sweep(drag, low, (share - kFlameRunGripShare) / (1.0f - kFlameRunGripShare));
        return Sweep(boss.moveStartPose, drag,
                     static_cast<float>(t) / static_cast<float>(std::max(1, boss.raiseFrames)));
    }
    if (boss.phase == BossActPhase::Post)
        return HoldThenRest(boss, up, move.post);
    // 攻撃中：いちばん新しく放った前線を放った tick（stateFrame 1＋間隔×番号）からの F で斬り上げる。
    // 最後の 1 本は斬り上げたまま後隙へ。
    const int interval = std::max(1, boss.flameRunIntervalFrames);
    const int since = std::max(0, t - 1 - interval * std::max(0, boss.flameRun.released - 1));
    const int span = std::max(1, static_cast<int>(static_cast<float>(interval) * kFlameRunUpShare));
    if (since <= span)
        return StrikeAt(rise, Smooth(static_cast<float>(since) / static_cast<float>(span)));
    if (boss.flameRun.released >= boss.flameRun.count)
        return up;
    return Sweep(up, low, static_cast<float>(since - span) / static_cast<float>(std::max(1, interval - span)));
}

// 屈みながら剣を頭上へ掲げ、跳ぶ・空中で溜める間は掲げたまま。落ちる間に切っ先を下へ向け、着地で突き立てる。
BossSwordPose SkyfallPose(const BossComponent& boss, const BossMove& move) {
    const BossSwordPose up = FromAngles(boss, kUp);
    const BossSwordPose plunge = FromAngles(boss, kPlunge);
    switch (boss.skyfall.phase) {
    case BossSkyfallPhase::Crouch: return Sweep(boss.moveStartPose, up, boss.crouch);
    case BossSkyfallPhase::Rise:
    case BossSkyfallPhase::Hang: return up;
    case BossSkyfallPhase::Dive: return Sweep(up, plunge, boss.skyfall.dive * kPlungeTurnSpeed);
    case BossSkyfallPhase::Landed:
        return boss.phase == BossActPhase::Post
                   ? Sweep(plunge, BossPose::Rest(boss),
                           static_cast<float>(boss.stateFrame) / static_cast<float>(std::max(1, move.post)))
                   : plunge;
    default: return BossPose::Rest(boss);
    }
}

// 霞の大剣の間：構えは大剣の柄と向き（見える刀が大剣に重なる。大剣の当たりは BossGreatsword が決める）。両手で握り、
// 大剣の向きへ胴を捻る。薙ぐ・叩きつける・刺さっている間は腰を落として前へ傾く（薙ぎ始めへ下ろす間・叩きつける間・
// ほどける間はその進みで寄せる）。
BossSwordPose GreatswordPose(const BossComponent& boss) {
    const BossGreatswordState& sword = boss.greatsword;
    const BossSwingSettings& s = boss.swing;
    const Vec3 root{boss.ground.x, boss.ground.y + BossPose::RootHeight(boss), boss.ground.z};
    BossSwordPose pose;
    pose.grip = RotateY(Sub(sword.hilt, root), -boss.yaw);
    pose.direction = Normalize(RotateY(sword.direction, -boss.yaw), {0.0f, 1.0f, 0.0f});
    pose.twoHand = 1.0f;
    float low = 0.0f, turn = 1.0f;
    switch (sword.phase) {
    case BossGreatswordPhase::Conjure:
        low = sword.wind;
        turn = sword.wind;
        break;
    case BossGreatswordPhase::Sweep:
    case BossGreatswordPhase::Stuck: low = 1.0f; break;
    case BossGreatswordPhase::Slam: low = sword.slam; break;
    case BossGreatswordPhase::Dissolve: low = 1.0f - sword.dissolve; break;
    default: break;
    }
    pose.crouch = s.greatswordCrouch * low;
    pose.lean = s.greatswordLean * low;
    // 大剣の向きの横の角度（縦に立てている時は決まらないので捻らない）。
    const float flat = std::hypot(pose.direction.x, pose.direction.z);
    const float yaw = flat > 0.2f ? std::atan2(pose.direction.x, pose.direction.z) / kRadiansPerDegree : 0.0f;
    pose.twist =
        std::clamp(yaw * s.greatswordTwistShare, -s.greatswordTwistMaxDegrees, s.greatswordTwistMaxDegrees) * turn;
    return pose;
}

// 霞隠れ：前隙で前の技の構えから下ろした構えへ。隠れている間は下ろした構えで、現れる時の振り上げもそこから始める
// （隠れている間は剣が見えないので、前の技の構えへは戻さない）。
BossSwordPose AmbushPose(const BossComponent& boss, const BossMove& move) {
    const int t = boss.stateFrame;
    const BossSwordPose rest = BossPose::Rest(boss);
    if (boss.phase == BossActPhase::Pre)
        return Sweep(boss.moveStartPose, rest, Share(t, move.pre));
    if (boss.phase == BossActPhase::Post)
        return BackToRest(boss, boss.lastEndPose, t, move.post);
    const auto g = BossPose::Progress(boss, move);
    const BossSegment& segment = move.segments[static_cast<size_t>(g.segment)];
    const Strike strike = ShapeStrike(boss.swing.horizontal, false);
    if (g.hit)
        return Swing(boss, strike, g.tIn, g.swingLength);
    if (segment.kind != BossSegmentKind::Appear)
        return rest;
    // 現れて振り上げ→構え（最後の 1 回はフェイント）。
    return Windup(boss, rest, strike, g.tIn, segment.length, segment.feint);
}

// 投げナイフ：前隙で前の技の構えから投げの構えへ、攻撃中で投げ切り、後隙で下ろした構えへ戻る。
BossSwordPose KnifePose(const BossComponent& boss, const BossMove& move) {
    const BossSwordPose start = FromAngles(boss, kKnifeStart);
    if (boss.phase == BossActPhase::Pre)
        return Sweep(boss.moveStartPose, start, Share(boss.stateFrame, move.pre));
    if (boss.phase == BossActPhase::Act)
        return Sweep(start, FromAngles(boss, kKnifeEnd), BossPose::Progress(boss, move).progress * 2.0f);
    return BackToRest(boss, boss.lastEndPose, boss.stateFrame, move.post);
}

// 連撃：一撃ごとの構えと振りは StrikeOf。後隙は最後の一撃の振り終わりを保ってから下ろす。
BossSwordPose StringPose(const BossComponent& boss, const BossMove& move) {
    const size_t last = std::max<size_t>(1, move.strikes.size()) - 1;
    if (boss.phase == BossActPhase::Pre)
        return Windup(boss, boss.moveStartPose, StrikeOf(boss, move, 0), boss.stateFrame, move.pre,
                      !move.strikes.empty() && move.strikes[0].feint);
    if (boss.phase == BossActPhase::Post)
        return HoldThenRest(boss, StrikeEnd(StrikeOf(boss, move, last)), move.post);
    const auto g = BossPose::Progress(boss, move);
    const BossSegment& segment = move.segments[static_cast<size_t>(g.segment)];
    const size_t k = std::min(static_cast<size_t>(std::max(0, segment.strike)), last);
    const Strike now = StrikeOf(boss, move, k);
    if (g.hit)
        return Swing(boss, now, g.tIn, g.swingLength);
    return Windup(boss, StrikeEnd(StrikeOf(boss, move, k > 0 ? k - 1 : 0)), now, g.tIn, segment.length, segment.feint);
}

} // namespace

BossSwordPose BossPose::Rest(const BossComponent& boss) { return FromAngles(boss, kRest); }

BossSwordPose BossPose::Target(const BossComponent& boss) {
    if (boss.state == BossState::Dead)
        return FromAngles(boss, kStunned);
    if (boss.state == BossState::Stunned)
        return StunPose(boss);
    if (boss.state == BossState::Idle)
        return IdlePose(boss);
    const auto& moves = boss.table.GetMoves();
    if (boss.state != BossState::Act || boss.move < 0 || static_cast<size_t>(boss.move) >= moves.size())
        return Rest(boss);
    const BossMove& move = moves[static_cast<size_t>(boss.move)];
    if (boss.greatsword.phase != BossGreatswordPhase::None)
        return GreatswordPose(boss);
    switch (move.type) {
    case BossMoveType::Dash: return DashPose(boss, move);
    case BossMoveType::SlashR: return SingleStrikePose(boss, move, ShapeStrike(boss.swing.kesa, false));
    case BossMoveType::SlashL: return SingleStrikePose(boss, move, ShapeStrike(boss.swing.kesa, true));
    case BossMoveType::SlashH: return SingleStrikePose(boss, move, ShapeStrike(boss.swing.horizontal, false));
    case BossMoveType::Overhead:
    case BossMoveType::SlashV: return SingleStrikePose(boss, move, ShapeStrike(boss.swing.vertical, false));
    case BossMoveType::Retreat: return RetreatPose(boss, move);
    case BossMoveType::Jump: return JumpPose(boss, move);
    case BossMoveType::Kick: return HeldPose(boss, move, kHeld);
    case BossMoveType::Detonate: return DetonatePose(boss, move);
    case BossMoveType::Clone: return HeldPose(boss, move, kForward);
    case BossMoveType::FlameRun: return FlameRunPose(boss, move);
    case BossMoveType::Skyfall: return SkyfallPose(boss, move);
    case BossMoveType::Ambush: return AmbushPose(boss, move);
    case BossMoveType::Knife: return KnifePose(boss, move);
    case BossMoveType::String: return StringPose(boss, move);
    default:
        if (boss.phase == BossActPhase::Pre)
            return Sweep(boss.moveStartPose, Rest(boss), Share(boss.stateFrame, move.pre));
        return boss.phase == BossActPhase::Post ? BackToRest(boss, boss.lastEndPose, boss.stateFrame, move.post)
                                                : Rest(boss);
    }
}

float BossPose::RootHeight(const BossComponent& boss) { return boss.height + boss.floatHeight - boss.sink; }

BossSwordLine BossPose::SwordLine(const BossComponent& boss, const BossSwordPose& pose) {
    const Vec3 root{boss.ground.x, boss.ground.y + RootHeight(boss), boss.ground.z};
    return {Add(root, RotateY(pose.grip, boss.yaw)), RotateY(Normalize(pose.direction, {0.0f, -1.0f, 0.0f}), boss.yaw)};
}

BossSwordPoints BossPose::SwordPoints(const BossComponent& boss, const BossSwordPose& pose) {
    const BossSwordLine line = SwordLine(boss, pose);
    BossSwordPoints points;
    points.base = Add(line.root, Mul(line.direction, boss.swordLength * kSwordBaseShare));
    points.inner = Add(line.root, Mul(line.direction, boss.swordLength * kSwordInnerShare));
    points.outer = Add(line.root, Mul(line.direction, boss.swordLength + boss.swordHazeLength));
    return points;
}

} // namespace Project_TD2_2
