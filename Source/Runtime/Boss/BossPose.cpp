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
    // 刃の面：斬る向きを刀身の軸のまわりに回す（左右に映した型では逆へ）。
    if (shape.edgeRollDegrees != 0.0f)
        pose.edge = Normalize(
            RotateAbout(pose.edge, pose.direction, (mirror ? -1.0f : 1.0f) * shape.edgeRollDegrees * kRadiansPerDegree),
            pose.edge);
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

// 振りの進み e（0＝構え、1＝振り終わり）の構え。
BossSwordPose StrikeAt(const Strike& strike, float e) {
    return ShapePose(*strike.shape, strike.mirror, Lerp(strike.from, strike.to, e),
                     strike.to >= strike.from ? 1.0f : -1.0f);
}

// 0〜1 の進み u を、peak で速さが最も大きくなる 0〜1 の進みへ。速さは途中で途切れない（u を peak で速さが最大になる
// ように曲げてから、両端で速さ 0 の 5 次の曲線に通す）。
float PeakCurve(float u, float peak) {
    const float x = std::pow(std::clamp(u, 0.0f, 1.0f), std::log(0.5f) / std::log(std::clamp(peak, 0.1f, 0.9f)));
    return x * x * x * (x * (x * 6.0f - 15.0f) + 10.0f);
}

// 振りの進み：ゆっくり動き始めて加速し、swingPeakShare で最も速く、振り終わりへ減速して止まる。
float SwingProgress(const BossComponent& boss, int tIn, int length) {
    return PeakCurve(static_cast<float>(tIn) / static_cast<float>(std::max(1, length)),
                     std::clamp(boss.swingPeakShare, 0.5f, 0.9f));
}

// 振り終わりの後：振り終わりの行き過ぎ（1＋swingOvershoot）から、振り終わりへ swingSettleFrames F で戻る（勢いで流れた
// 刀を引き戻して止める＝残心）。
BossSwordPose Follow(const BossComponent& boss, const Strike& strike, int t) {
    const float settle = Smooth(Share(t, std::max(1, boss.swingSettleFrames)));
    return StrikeAt(strike, 1.0f + boss.swingOvershoot * (1.0f - settle));
}

// 振り上げ：今の構え previous から一撃の構え（振りの進み end。行き過ぎは負）へ（k は 0〜1）。型の raiseVia が 0 より
// 大きければ、前半で型の途中の構えへ寄せ、後半で型を逆にたどって構えへ上げる（縦の振りかぶりで、刀の向きが真逆へ変わる
// 時に体の中を通らない）。
BossSwordPose Raise(const BossSwordPose& previous, const Strike& strike, float k, float end = 0.0f) {
    const float via = std::clamp(strike.shape->raiseVia, 0.0f, 1.0f);
    if (via <= 0.0f)
        return Sweep(previous, StrikeAt(strike, end), k);
    const float s = Smooth(k);
    if (s < 0.5f)
        return Blend(previous, StrikeAt(strike, via), s * 2.0f);
    return StrikeAt(strike, Lerp(via, end, (s - 0.5f) * 2.0f));
}

// 溜めの震え：刀の向きを、刀と上の向きに直交する軸のまわりに小さく揺らす（degrees は揺れの大きさ）。
BossSwordPose Tremble(BossSwordPose pose, int t, float degrees = kFeintTremorDegrees) {
    const Vec3 axis = Cross(pose.direction, {0.0f, 1.0f, 0.0f});
    const float angle = std::sin(static_cast<float>(t) * kFeintTremorRate) * degrees * kRadiansPerDegree;
    pose.direction = Normalize(RotateAbout(pose.direction, axis, angle), pose.direction);
    return pose;
}

// 構えの間の振りの進み（t は構えに入ってからの F、kamae は構えの長さ）：振り上げの行き過ぎ（-swingOverwind）から
// swingOverwindFrames F で戻りながら、構えの終わりへ向けて swingCoil まで引き絞る。
float CockedShare(const BossComponent& boss, int t, int kamae) {
    const float settle = Smooth(Share(t, std::max(1, boss.swingOverwindFrames)));
    const float coil = Smooth(Share(t, std::max(1, kamae)));
    return -boss.swingOverwind * (1.0f - settle) - boss.swingCoil * coil;
}

// 振り上げ→構え。剣を振る方向と反対へ大きく引いて、構えの間は止める。
// フェイント（遅）は、いつものテンポで「振り出し」まで見せてから引き戻し、溜めて（小さく震えて）待つ。
// direct：前の振りの終わり・コンボの続きから、止まらずに振り上げ始める（振り上げを構えの前の間いっぱいに延ばす）。
BossSwordPose Windup(const BossComponent& boss, const BossSwordPose& previous, const Strike& strike, int tIn,
                     int length, bool feint, bool direct = false) {
    const BossSwordPose cocked = StrikeAt(strike, -boss.swingCoil);
    const int base = feint ? std::max(1, length - boss.feintFrames) : length;
    // 前隙が構え＋振り上げより短い時（変化で切り替えた技など）は比で分ける（構えへ 1 F で跳ばない）。
    int kamae = std::min(boss.kamaeFrames, base - 1);
    if (base < boss.kamaeFrames + boss.raiseFrames)
        kamae = std::max(1, base * boss.kamaeFrames / std::max(1, boss.kamaeFrames + boss.raiseFrames));
    const int raiseEnd = std::max(1, base - kamae);
    const int raiseStart = direct ? 0 : std::max(0, raiseEnd - boss.raiseFrames);
    if (tIn < base || !feint) {
        if (tIn < raiseStart)
            return previous;
        if (tIn < raiseEnd)
            return Raise(previous, strike,
                         static_cast<float>(tIn - raiseStart) / static_cast<float>(std::max(1, raiseEnd - raiseStart)),
                         -boss.swingOverwind);
        const int t = tIn - raiseEnd;
        return Tremble(StrikeAt(strike, CockedShare(boss, t, kamae)), tIn,
                       boss.swingCoilTremorDegrees * Share(t, std::max(1, kamae)));
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
            // クリップの刀で当てる技は、当たりの窓（区間の表）の長さ全部が振り（クリップの刀が窓の間ずっと動く）。
            result.swingLength = BossAnimBody::ClipMove(boss, move)
                                     ? std::max(1, length)
                                     : std::max(1, std::min(length, boss.swingInFrames + boss.swingFrames));
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

} // namespace

BossSwordPose BossPose::Rest(const BossComponent& boss) { return FromAngles(boss, kRest); }

BossSwordPose BossPose::Target(const BossComponent& boss) {
    // 規則の剣の構え（c-b4e76a）：クリップの刀で当たる技はクリップの刀（当たり・斬撃の面・刀の煙）、ほかの間は見えている
    // 骨格の体の刀。骨格の体をまだ読めていない間だけ休めの構え。技の型ごとの程序化の姿勢は持たない。
    if (BossSwordPose clip; BossAnimBody::ClipSwordPose(boss, clip))
        return clip;
    if (BossSwordPose bone; BossAnimBody::BoneSwordPose(boss, bone))
        return bone;
    return Rest(boss);
}

float BossPose::RootHeight(const BossComponent& boss) { return boss.height + boss.floatHeight - boss.sink; }

BossSwordLine BossPose::SwordLine(const BossComponent& boss, const BossSwordPose& pose) {
    const Vec3 root{boss.ground.x, boss.ground.y + RootHeight(boss), boss.ground.z};
    return {Add(root, RotateY(pose.grip, boss.yaw)), RotateY(Normalize(pose.direction, {0.0f, -1.0f, 0.0f}), boss.yaw)};
}

BossSwordPoints BossPose::SwordPoints(const BossComponent& boss, const BossSwordPose& pose) {
    BossSwordLine line = SwordLine(boss, pose);
    if (boss.spin != 0.0f) {
        const Vec3 center{boss.ground.x, line.root.y, boss.ground.z};
        line.root = Add(center, RotateY(Sub(line.root, center), boss.spin));
        line.direction = RotateY(line.direction, boss.spin);
    }
    BossSwordPoints points;
    points.base = Add(line.root, Mul(line.direction, boss.swordLength * kSwordBaseShare));
    points.inner = Add(line.root, Mul(line.direction, boss.swordLength * kSwordInnerShare));
    points.outer = Add(line.root, Mul(line.direction, boss.swordLength + boss.swordHazeLength + pose.reach));
    return points;
}

float BossPose::SwingShare(const BossComponent& boss, int tIn, int length) {
    return std::clamp(SwingProgress(boss, std::clamp(tIn, 0, std::max(1, length)), length), 0.0f, 1.0f);
}

} // namespace Project_TD2_2
