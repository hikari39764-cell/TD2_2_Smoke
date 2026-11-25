#include "BossIntroSmoke.h"

#include <algorithm>
#include <array>
#include <cmath>

#include "HIKARI/Project/FluidTypes.h"
#include "BossBodyMath.h"
#include "BossHelmGlowData.h"
#include "BossIntroActor.h"
#include "BossIntroSmokeFeed.h"
#include "BossIntroSmokeShape.h"
#include "../Boss/BossVfx.h"
#include "../Common/FluidChannels.h"
#include "../Common/VfxParameters.h"

namespace Project_TD2_2 {
namespace {

using namespace BossBodyMath;
using namespace BossIntroSmokeFeed;
using namespace BossIntroSmokeShape;
using Kind = BossIntroCueKind;

// 流れている拍 kind の「段の中の秒」（流れていなければ負）と、その段の長さ。
float CueLocal(const BossIntroState& state, Kind kind, float& duration) {
    for (int k = 0; k < state.cueCount; ++k) {
        const BossIntroCue& cue = state.cues[static_cast<size_t>(k)];
        if (cue.kind != kind || !cue.entered || cue.left)
            continue;
        duration = std::max(kTiny, cue.clip.durationSeconds);
        return std::max(0.0f, state.showSeconds - cue.clip.startSeconds);
    }
    return -1.0f;
}

// 拍 kind が始まってからの秒（まだ始まっていなければ負。流れ終わった後も数える）。
float Since(const BossIntroState& state, Kind kind) {
    for (int k = 0; k < state.cueCount; ++k) {
        const BossIntroCue& cue = state.cues[static_cast<size_t>(k)];
        if (cue.kind == kind && cue.entered)
            return state.showSeconds - cue.clip.startSeconds;
    }
    return -1.0f;
}

// 拍 kind がもう流れ終わったか。
bool Passed(const BossIntroState& state, Kind kind) {
    for (int k = 0; k < state.cueCount; ++k)
        if (state.cues[static_cast<size_t>(k)].kind == kind && state.cues[static_cast<size_t>(k)].left)
            return true;
    return false;
}

Vec3 Ground(const BossIntroState& state, Vec3 at) { return {at.x, state.origin.y, at.z}; }

// 地を走る煙の頭：進む向き travel に細長い箱の中へ、点を散らして入れる。量を細かく不揃いに脈打たせる（尾が一様な筒に
// ならず、塊の連なりになる）。頭は場の霧をかき分けて走る。
void StreakHead(const Feed& feed, const BossIntroSettings& s, Vec3 head, Vec3 travel, Vec3 side, float speed,
                float local) {
    // 量は、ゆっくり揺らすだけ（細かく脈打たせると、尾が跳ぶように伸びて見える）。
    const float pulse = 0.85f + 0.15f * std::sin(local * 2.0f);
    const float mass = s.streakAmount * kStreakPerMeter * std::max(kStreakSlowest, speed) * pulse;
    feed.Burst(kStreakId + 6, head, kStreakFogRadius, kStreakFogPush, FluidChannels::kFog);
    for (int k = 0; k < kStreakWisps; ++k) {
        const uint64_t id = kStreakId + static_cast<uint64_t>(k);
        const Vec3 j = feed.Jitter(id, 3u);
        Wisp wisp{};
        wisp.at = Add(head, Add(Add(Mul(travel, 0.4f * j.x), Mul(side, 0.45f * j.z)), Up(0.38f + 0.26f * j.y)));
        wisp.radius = 0.25f;
        wisp.velocity =
            Add(Add(Mul(travel, kStreakCarry), Mul(side, 2.0f * j.z)), Up(0.6f + 1.4f * std::max(0.0f, j.y)));
        wisp.gust = 2.0f;
        wisp.heat = kStreakHeat;
        feed.Smoke(id, wisp, mass / static_cast<float>(kStreakWisps), FluidChannels::kIntroStreak);
    }
}

// 地を走る煙が鎧に着いた後：鎧の周りを回りながら、内へ巻き込む（鎧を包む渦になる）。
void Wrap(const Feed& feed, const BossIntroSettings& s, Vec3 armor, float since) {
    const float w = 1.0f - since / kWrapSeconds;
    for (int k = 0; k < kWrapWisps; ++k) {
        const uint64_t id = kWrapId + static_cast<uint64_t>(k);
        const Vec3 out =
            Around(since * kWrapTurn + 2.0f * kPi * static_cast<float>(k) / static_cast<float>(kWrapWisps));
        Wisp wisp{};
        wisp.at = Add(armor, Add(Mul(out, 1.3f), Up(0.3f)));
        wisp.spread = 0.2f;
        wisp.radius = 0.26f;
        wisp.velocity = Add(Add(Mul(Tangent(out), 5.0f), Mul(out, -2.0f)), Up(0.8f));
        wisp.gust = 1.5f;
        wisp.heat = 0.8f;
        feed.Smoke(id, wisp, s.wrapAmount * w / static_cast<float>(kWrapWisps));
    }
}

// 地を走って鎧に着き、鎧を包む煙。刀の向こう（カメラの後ろ）から来て、刀の横を抜ける。道に沿って置いた細長い格子の域
// （BossIntroStreak。局所 +X が道の向き。動かさない）へ、走る頭の所から煙を入れる：入れた煙はその場に残って渦を巻き、
// 頭の後ろに乱れた尾を引く。着いたら、舞台の域に鎧を包む煙を足す。
void Streak(HIKARI::WorldView& world, const Feed& feed, BossComponent& boss, BossIntroComponent& intro, float seconds) {
    const BossIntroSettings& s = intro.settings;
    BossIntroState& state = intro.state;
    const BossIntroActorState& a = state.actor;
    float duration = 0.0f;
    const float local = state.phase == BossIntroPhase::Show && !state.skipped && a.socketsValid
                            ? CueLocal(state, Kind::SmokeStreak, duration)
                            : -1.0f;
    if (local >= 0.0f) {
        if (!state.streakSet) {
            // 道は拍の始めに決めて、域を動かさない。
            state.streakSet = true;
            state.streakTo = Ground(state, Lerp(a.hips.position, a.chest.position, 0.5f));
            const Vec3 sword = BossIntroActor::SwordEntry(intro);
            state.streakFrom = Add(sword, Mul(FlatNormalize(Sub(sword, state.streakTo)), s.streakLead));
        }
        const Vec3 armor = state.streakTo;
        const Vec3 from = state.streakFrom;
        const Vec3 along = FlatNormalize(Sub(from, armor));
        const Vec3 side = Tangent(along);
        const float run = duration * kStreakRunShare;
        const float u = std::min(1.0f, local / std::max(kTiny, run));
        const auto path = [&](float v) {
            return Add(Lerp(from, armor, EaseOut(v)),
                       Mul(side, s.streakSwerve * std::sin(v * kPi * kStreakWeave) * (1.0f - v)));
        };
        const Vec3 head = path(u);
        const Vec3 step = Sub(path(std::min(1.0f, u + 0.01f)), head);
        // 頭の速さ (m/秒)。速い所でも道の 1 m あたりに置く煙の濃さが変わらないよう、量に掛ける。
        const float speed = Length(step) / std::max(kTiny, 0.01f * run);
        state.streakFade = 0.0f;
        const float since = local - run;
        if (since < 0.0f)
            StreakHead(feed, s, head, Normalize(step, Mul(along, -1.0f)), side, speed, local);
        else if (since < kWrapSeconds)
            Wrap(feed, s, armor, since);
    } else {
        // 拍が終わった後：残った尾が薄れるまで域を置いておき、それから止める（飛ばした時はすぐ止める）。
        if (!state.streakHandle.IsValid())
            return;
        state.streakFade += std::max(0.0f, seconds);
        if (state.skipped || state.streakFade >= kStreakFadeSeconds) {
            BossVfx::StopHandle(world, state.streakHandle);
            return;
        }
    }
    if (s.streakEffect.IsEmpty() || !state.streakSet)
        return;
    // 域は道の真ん中に、道の向き（局所 +X）へ向けて置く。
    const Vec3 way = Sub(state.streakTo, state.streakFrom);
    BossVfx::StartOrSend(
        world, boss, state.streakHandle, s.streakEffect,
        BossVfx::Placed(Lerp(state.streakFrom, state.streakTo, 0.5f), 1.0f, YawRotation(std::atan2(-way.z, way.x))),
        {});
}

// 鎧を引き起こす煙（Smoke.Rise）：倒れた鎧の軸（足元 → 胸の上）の周りを回りながら、ねじれた柱になって上へ抜ける。
// 鎧は、上へ抜ける煙に吊られて起きる。初めに強く噴いて、起き上がるにつれて細る。
void Rise(const Feed& feed, const BossIntroComponent& intro) {
    const BossIntroState& state = intro.state;
    const BossIntroActorState& a = state.actor;
    float duration = 0.0f;
    const float local = CueLocal(state, Kind::SmokeRise, duration);
    if (local < 0.0f || !a.socketsValid)
        return;
    const float u = local / duration;
    const float mass = intro.settings.riseAmount * (1.0f - u) * (1.0f - u) / static_cast<float>(kRiseWisps);
    const Vec3 ground = Ground(state, a.hips.position);
    const Vec3 top = Add(a.chest.position, Up(kRiseAbove));
    for (int k = 0; k < kRiseWisps; ++k) {
        const uint64_t id = kRiseId + static_cast<uint64_t>(k);
        const float height = feed.Roll(id, 5u);
        const Vec3 out = Around(state.showSeconds * kRiseTwist + feed.Roll(id, 6u) +
                                2.0f * kPi * static_cast<float>(k) / static_cast<float>(kRiseWisps));
        Wisp wisp{};
        // 低い所ほど多く（高さの二乗で配る）。
        wisp.at = Add(Lerp(Add(ground, Up(0.25f)), top, height * height), Mul(out, kRiseRing));
        wisp.spread = 0.22f;
        wisp.radius = 0.27f;
        wisp.velocity =
            Add(Add(Up(kRiseSpeed * (0.6f + 0.5f * height)), Mul(Tangent(out), kRiseSwirl)), Mul(out, -1.0f));
        wisp.gust = 2.0f;
        wisp.heat = kRiseHeat;
        feed.Smoke(id, wisp, mass);
    }
    // 柱を引き上げる力（鎧の軸の真ん中あたりで、上へ。細るのに合わせて弱める）。
    feed.Blow(kRiseId + 6, Lerp(ground, top, 0.4f), kRisePushRadius, {0.0f, 1.0f, 0.0f}, kRisePush * (1.0f - u),
              FluidChannels::kIntroSmoke);
    feed.Warm(kWarmId + 4, Add(top, Up(kRiseChimneyRadius * 0.6f)), kRiseChimneyRadius, kRiseChimneyHeat * (1.0f - u));
}

// 腕の煙 1 本：肩 → 肘 → 手首の折れ線の上の、reach（0〜1）までの所へ点を散らす（伸びていく腕）。mass＝片腕の総量。
void Arm(const Feed& feed, uint64_t id, Vec3 shoulder, Vec3 elbow, Vec3 wrist, float reach, int count, float mass,
         float radius) {
    if (mass <= 0.0f || reach <= 0.0f)
        return;
    const float upper = Length(Sub(elbow, shoulder));
    const float total = std::max(kTiny, upper + Length(Sub(wrist, elbow)));
    for (int k = 0; k < count; ++k) {
        const uint64_t point = id + static_cast<uint64_t>(k);
        const float along = (static_cast<float>(k) + feed.Roll(point, 4u)) / static_cast<float>(count) * reach * total;
        const bool first = along < upper;
        Wisp wisp{};
        wisp.at = first ? Lerp(shoulder, elbow, along / std::max(kTiny, upper))
                        : Lerp(elbow, wrist, (along - upper) / std::max(kTiny, total - upper));
        wisp.spread = 0.06f;
        wisp.radius = radius;
        wisp.velocity = Mul(Normalize(first ? Sub(elbow, shoulder) : Sub(wrist, elbow)), kArmFlow);
        wisp.gust = 0.8f;
        wisp.heat = kArmHeat;
        feed.Smoke(point, wisp, mass / static_cast<float>(count));
    }
}

// 両腕の煙：手甲が付く前は、肩から手首へ伸びていく濃い煙の腕（右は Smoke.Tendril、左は Smoke.TendrilLeft の拍）。
// 付いた後は、鎧と手甲をつなぐ細い煙。
void Arms(const Feed& feed, const BossIntroComponent& intro) {
    const BossIntroSettings& s = intro.settings;
    const BossIntroState& state = intro.state;
    const BossIntroActorState& a = state.actor;
    if (!a.socketsValid)
        return;
    float duration = 0.0f;
    const bool rightOn = a.attached[static_cast<size_t>(BossIntroPart::GauntletRight)];
    const bool leftOn = a.attached[static_cast<size_t>(BossIntroPart::GauntletLeft)];
    const float right = CueLocal(state, Kind::SmokeTendril, duration);
    const float left = CueLocal(state, Kind::SmokeTendrilLeft, duration);
    if (rightOn)
        Arm(feed, kArmId, a.rightShoulder.position, a.rightElbow.position, a.rightHand.position, 1.0f,
            kArmWispsAttached, s.armAmount, 0.22f);
    else if (right >= 0.0f)
        Arm(feed, kArmId, a.rightShoulder.position, a.rightElbow.position, a.rightHand.position,
            Smooth(right / kArmReachSeconds), kArmWisps, s.tendrilAmount, 0.25f);
    if (leftOn)
        Arm(feed, kArmId + 4, a.leftShoulder.position, a.leftElbow.position, a.leftHand.position, 1.0f,
            kArmWispsAttached, s.armAmount, 0.22f);
    else if (left >= 0.0f)
        Arm(feed, kArmId + 4, a.leftShoulder.position, a.leftElbow.position, a.leftHand.position,
            Smooth(left / kArmReachSeconds), kArmWisps, s.tendrilAmount, 0.25f);
}

// 落ちている手甲を呼ぶ煙：痙攣の段の間、肩の下の地面から手甲へ、地を這う細い煙が伸びて行く（届くと手甲が震える）。
// 飛んでいる間は、手甲が煙を引く。
void Call(const Feed& feed, const BossIntroComponent& intro, BossIntroPart part, Vec3 shoulder, uint64_t id) {
    const BossIntroState& state = intro.state;
    const BossIntroActorState& a = state.actor;
    const size_t index = static_cast<size_t>(part);
    if (a.attached[index] || !a.socketsValid)
        return;
    const Vec3 at = a.partAt[index];
    const float amount = intro.settings.tendrilAmount;
    // 震えている間・飛び立つ間は、場の霧を手甲の周りから押しのける（低い霧の中でも手甲が読める）。
    if (a.fly[index] > 0.0f || a.twitch[index] > 0.0f)
        feed.Burst(id + 7, Ground(state, at), kCallFogRadius, kCallFogPush, FluidChannels::kFog);
    if (a.fly[index] > 0.0f) {
        for (uint64_t k = 0; k < 2; ++k) {
            Wisp wisp{};
            wisp.at = at;
            wisp.spread = 0.14f;
            wisp.radius = 0.22f;
            wisp.gust = 1.5f;
            wisp.heat = 0.5f;
            feed.Smoke(id + k, wisp, amount * 0.25f);
        }
        return;
    }
    if (a.twitch[index] <= 0.0f)
        return;
    const Vec3 from = Ground(state, shoulder);
    const Vec3 to = Ground(state, at);
    const float reach = Smooth(a.twitch[index] * kCallReachShare);
    for (int k = 0; k < kCallWisps; ++k) {
        const uint64_t point = id + 2 + static_cast<uint64_t>(k);
        const float along = (static_cast<float>(k) + feed.Roll(point, 4u)) / static_cast<float>(kCallWisps) * reach;
        Wisp wisp{};
        wisp.at = Add(Lerp(from, to, along), Up(0.2f));
        wisp.spread = 0.08f;
        wisp.radius = 0.22f;
        wisp.velocity = Mul(FlatNormalize(Sub(to, from)), 2.0f);
        wisp.gust = 0.6f;
        feed.Smoke(point, wisp, amount * 0.5f / static_cast<float>(kCallWisps));
    }
}

// 手甲が腕に付いた瞬間：手首の継ぎ目から、前腕の周りへ輪に噴く（叩き込まれて、中の煙が継ぎ目から押し出される）。
void Puff(const Feed& feed, const BossIntroComponent& intro, Kind kind, Vec3 wrist, Vec3 elbow, uint64_t id) {
    float duration = 0.0f;
    const float local = CueLocal(intro.state, kind, duration);
    if (local < 0.0f || local >= kPuffSeconds)
        return;
    const Vec3 axis = Normalize(Sub(wrist, elbow), {0.0f, -1.0f, 0.0f});
    const Vec3 side = Perpendicular({1.0f, 0.0f, 0.0f}, axis);
    const Vec3 over = Cross(axis, side);
    for (int k = 0; k < kPuffWisps; ++k) {
        const uint64_t point = id + static_cast<uint64_t>(k);
        // 短い噴きの間に、点は輪の自分の持ち分を端から端まで掃く（止まった点は、小さな球の粒に見える）。
        const float angle =
            2.0f * kPi * (static_cast<float>(k) + local / kPuffSeconds) / static_cast<float>(kPuffWisps);
        const Vec3 out = Add(Mul(side, std::cos(angle)), Mul(over, std::sin(angle)));
        Wisp wisp{};
        wisp.at = Add(Sub(wrist, Mul(axis, 0.15f)), Mul(out, 0.22f));
        wisp.spread = 0.05f;
        wisp.radius = 0.22f;
        wisp.velocity = Add(Mul(out, kPuffSpeed), Mul(axis, -1.5f));
        wisp.gust = 1.5f;
        wisp.heat = 1.0f;
        feed.Smoke(point, wisp, intro.settings.puffAmount / static_cast<float>(kPuffWisps));
    }
}

// 兜を手の高さまで持ち上げる煙の柱（Smoke.HelmLift）：兜の下の地面から、ねじれながら立って兜を下から支える。
void Lift(const Feed& feed, const BossIntroComponent& intro) {
    const BossIntroState& state = intro.state;
    const BossIntroActorState& a = state.actor;
    float duration = 0.0f;
    const float local = CueLocal(state, Kind::HelmLift, duration);
    if (local < 0.0f)
        return;
    const size_t part = static_cast<size_t>(BossIntroPart::Helm);
    const Vec3 helm = a.attached[part] && a.socketsValid ? a.head.position : a.partAt[part];
    const Vec3 ground = Ground(state, helm);
    const float fade = Smooth(local / 0.6f) * Smooth((duration - local) / 0.8f);
    const float height = std::max(0.3f, helm.y - 0.4f - ground.y);
    for (int k = 0; k < kLiftWisps; ++k) {
        const uint64_t id = kLiftId + static_cast<uint64_t>(k);
        const float share = (static_cast<float>(k) + feed.Roll(id, 4u)) / static_cast<float>(kLiftWisps);
        const Vec3 out = Around(state.showSeconds * kLiftTwist + kPi * static_cast<float>(k));
        Wisp wisp{};
        wisp.at = Add(ground, Add(Up(0.15f + share * height), Mul(out, 0.16f)));
        wisp.spread = 0.05f;
        wisp.radius = 0.22f;
        wisp.velocity = Add(Up(2.2f + 1.2f * share), Mul(Tangent(out), 1.6f));
        wisp.gust = 0.6f;
        wisp.heat = 1.5f;
        feed.Smoke(id, wisp, intro.settings.liftAmount * fade / static_cast<float>(kLiftWisps));
    }
}

// 兜が頭に収まった瞬間の襟：兜の後ろ半分の弧から外へ噴く。冷たい煙なので、肩を越えて鎧を伝い落ちる。顔の前へは
// 噴かない（目の灯りと兜を煙で隠さない）。
void SeatCollar(const Feed& feed, const BossIntroComponent& intro, Vec3 collar, Vec3 face, float local) {
    const float arc = 2.0f * kPi - 2.0f * kSeatFrontGapDegrees * kRadiansPerDegree;
    const float back = YawOf(face) + kPi;
    for (int k = 0; k < kSeatRing; ++k) {
        const uint64_t id = kSeatId + static_cast<uint64_t>(k);
        // 噴きの間に、点は弧の自分の持ち分を端から端まで掃く。
        const float share = (static_cast<float>(k) + local / kSeatBurstSeconds) / static_cast<float>(kSeatRing);
        const Vec3 out = Around(back + (share - 0.5f) * arc);
        Wisp wisp{};
        wisp.at = Add(collar, Mul(out, 0.7f));
        wisp.spread = 0.08f;
        wisp.radius = 0.24f;
        wisp.velocity = Add(Mul(out, kSeatRingSpeed * (0.7f + 0.6f * feed.Roll(id, 5u))), Up(-1.0f));
        wisp.gust = 2.0f;
        feed.Smoke(id, wisp, intro.settings.seatBurstAmount / static_cast<float>(kSeatRing));
    }
}

// 兜が頭に収まった瞬間：襟から煙が噴き、面頬の隙間から前へ細く噴き出し、周りが外へ押される。
void Seat(const Feed& feed, BossIntroComponent& intro) {
    BossIntroState& state = intro.state;
    const BossIntroActorState& a = state.actor;
    float duration = 0.0f;
    const float local = CueLocal(state, Kind::HelmSeat, duration);
    if (local < 0.0f || !a.socketsValid)
        return;
    const Vec3 collar = Add(a.head.position, Up(-kSeatCollarDrop));
    if (local < kSeatBurstSeconds)
        SeatCollar(feed, intro, collar, FlatNormalize(Rotate(a.head.rotation, {0.0f, 0.0f, 1.0f})), local);
    // 面頬の隙間から前へ：兜が叩き込まれて、中の煙が隙間から細く噴き出す（顔の前を塞がない細さと短さ）。
    if (local < kSeatJetSeconds) {
        const Vec3 forward = Rotate(a.head.rotation, {0.0f, 0.0f, 1.0f});
        const Vec3 side = Rotate(a.head.rotation, {1.0f, 0.0f, 0.0f});
        const Vec3 visor = Add(a.head.position, Add(Mul(forward, kSeatJetForward), Up(kSeatJetHeight)));
        for (int k = 0; k < kSeatJets; ++k) {
            const float across = (static_cast<float>(k) / static_cast<float>(kSeatJets - 1) - 0.5f) * kSeatJetWidth;
            Wisp wisp{};
            wisp.at = Add(visor, Mul(side, across));
            wisp.spread = 0.03f;
            wisp.radius = 0.2f;
            wisp.velocity = Add(Mul(forward, kSeatJetSpeed), Mul(side, across * 4.0f));
            wisp.gust = 1.0f;
            wisp.heat = 0.8f;
            feed.Smoke(kSeatId + 8 + static_cast<uint64_t>(k), wisp,
                       intro.settings.seatBurstAmount * kSeatJetShare / static_cast<float>(kSeatJets));
        }
    }
    if (state.smokeSeatPush) {
        state.smokeSeatPush = false;
        feed.Burst(kSeatId + 11, collar, 5.0f, 7.0f, FluidChannels::kIntroSmoke);
        // 足元の場の霧を、輪に押しのける（叩き込んだ重さが地面まで伝わる）。
        feed.Burst(kSeatId + 12, BossIntroSmoke::FeetWorld(intro), kSeatFogRadius, kSeatFogPush, FluidChannels::kFog);
    }
}

// 刀へ歩く一歩（拍 Step）：足元から黒い煙が滲み出て、自分でゆっくり立ち上がる（一歩ごとに噴き出させない）。場の霧は
// 少し押しのける。
void Step(const Feed& feed, const BossIntroComponent& intro) {
    float duration = 0.0f;
    const float local = CueLocal(intro.state, Kind::Step, duration);
    if (local < 0.0f || local > kStepSeconds)
        return;
    const Vec3 feet = BossIntroSmoke::FeetWorld(intro);
    for (int k = 0; k < kStepWisps; ++k) {
        const uint64_t id = kStepId + static_cast<uint64_t>(k);
        const Vec3 out =
            Around(2.0f * kPi * (static_cast<float>(k) + local / kStepSeconds) / static_cast<float>(kStepWisps));
        Wisp wisp{};
        wisp.at = Add(feet, Add(Mul(out, 0.5f), Up(0.18f)));
        wisp.spread = 0.1f;
        wisp.radius = 0.22f;
        wisp.velocity = Add(Mul(out, kStepSpeed), Up(kStepRise));
        wisp.gust = 0.8f;
        wisp.heat = kStepHeat;
        feed.Smoke(id, wisp, intro.settings.stepAmount / static_cast<float>(kStepWisps));
    }
    if (local < kStepPushSeconds)
        feed.Burst(kStepId + 5, feet, kStepFogRadius, kStepFogPush, FluidChannels::kFog);
}

// 刀を握っている間、刀の根元へ地面の煙がゆっくり寄り、刀身に沿って自分から立ち上がる（噴かない。握るほど濃くなる）。
void Seep(const Feed& feed, const BossIntroComponent& intro) {
    const BossIntroState& state = intro.state;
    float duration = 0.0f;
    const float local = CueLocal(state, Kind::SwordGrab, duration);
    if (local < 0.0f)
        return;
    const Vec3 entry = BossIntroActor::SwordEntry(intro);
    const float u = local / duration;
    const float mass = intro.settings.seepAmount * u / static_cast<float>(kSeepGround + kSeepBlade);
    for (int k = 0; k < kSeepGround; ++k) {
        const uint64_t id = kSeepId + static_cast<uint64_t>(k);
        const Vec3 out = Around(state.showSeconds * 1.7f + 2.0f * kPi * (static_cast<float>(k) + feed.Roll(id, 4u)) /
                                                               static_cast<float>(kSeepGround));
        Wisp wisp{};
        wisp.at = Add(entry, Add(Mul(out, 0.75f), Up(0.18f)));
        wisp.spread = 0.12f;
        wisp.radius = 0.22f;
        wisp.velocity = Add(Add(Mul(Tangent(out), kSeepSwirl), Mul(out, -kSeepInward)), Up(kSeepRise));
        wisp.gust = 0.8f;
        wisp.heat = kSeepHeat;
        feed.Smoke(id, wisp, mass);
    }
    for (int k = 0; k < kSeepBlade; ++k) {
        const uint64_t id = kSeepId + static_cast<uint64_t>(kSeepGround + k);
        Wisp wisp{};
        wisp.at = Add(entry, Up(0.3f + 1.3f * feed.Roll(id, 4u)));
        wisp.spread = 0.08f;
        wisp.radius = 0.2f;
        wisp.velocity = Up(kSeepRise);
        wisp.gust = 0.8f;
        wisp.heat = kSeepHeat + 0.6f;
        feed.Smoke(id, wisp, mass * u);
    }
}

#include "BossIntroSmokeFlow.inl"
#include "BossIntroSmokePrologue.inl"

// 刀が抜けた噴き：根元から柱が立ち上がり、地面の煙が扇へ転がる。煙は細かい点で入れ、流れは力で運ぶ。
// root＝柱の根元、drift＝煙を流して行く向き（水平。柱はそちらへ倒れ、扇もそちらへ開く）、local＝噴き始めてからの秒。
void DrawBurst(const Feed& feed, const BossIntroSettings& s, Vec3 root, Vec3 drift, float local) {
    const float u = local / std::max(kTiny, s.drawBurstSeconds);
    const float w = 1.0f - 0.5f * u;
    // 柱：根元から伸びて行く高さの中へ、点を高さに沿って散らす（上ほど広く、流れる向きへ倒す。軸の周りを回る）。
    const float front = kBurstColumnHeight * Smooth(local / kBurstColumnGrow);
    // 柱の点は、抜けてから少し置いて入れ始める。抜けた直後（スローの寄り）は、握っている間に根元へ溜めた煙を、柱の力が
    // 噴き上げる：何も無い所へ点から入れた煙は、巻く前の丸い粒のまま見えてしまう。
    const float jet =
        s.drawBurstAmount * w * Smooth((local - kBurstColumnDelay) / kBurstColumnFill) / static_cast<float>(kBurstJet);
    for (int k = 0; k < kBurstJet; ++k) {
        const uint64_t id = kBurstId + static_cast<uint64_t>(k);
        const float height = (static_cast<float>(k) + feed.Roll(id, 4u)) / static_cast<float>(kBurstJet) * front;
        const Vec3 out = Around(local * kBurstColumnTwist + 2.4f * static_cast<float>(k));
        Wisp wisp{};
        wisp.at = Add(Add(root, Mul(drift, kBurstColumnLean * height)),
                      Add(Up(0.3f + height), Mul(out, 0.25f + 0.07f * height)));
        wisp.spread = 0.25f + 0.05f * height;
        wisp.radius = 0.42f;
        wisp.velocity = Add(Up(s.drawJetSpeed * 0.6f), Mul(Tangent(out), 2.5f));
        wisp.gust = 4.0f;
        wisp.heat = kBurstHeat;
        feed.Smoke(id, wisp, jet);
    }
    // 柱を噴き上げる力：根元から kBurstJetRun の高さで、柱の速さまで加速する。終わりへ向けて弱める（先の煙は上へ
    // 抜けて行き、後の煙が柱として立ち残る）。
    feed.Blow(kBurstId + 12, Add(root, Up(kBurstJetRun * 0.5f)), kBurstJetRadius,
              Normalize(Add(Up(1.0f), Mul(drift, 0.15f))),
              s.drawJetSpeed * s.drawJetSpeed / (2.0f * kBurstJetRun) * (1.0f - u * u), FluidChannels::kIntroSmoke);
}

// 刀身が引く煙の幕：刀身の上の点を、速さを抑えて追いかける点から煙を入れる。刀身そのものは振りの間 60 m/秒を超え、
// エンジンが「瞬間移動」とみなして掃かなくなる（煙が帯にならず、1 フレームごとの粒が弧に並ぶ）。追う点は遅れて付いて
// 行くので、刀身の後ろに帯が残る。
void Trail(const Feed& feed, const BossComponent& boss, const BossBodySettings& bodyLook, BossIntroComponent& intro,
           float local) {
    BossIntroState& state = intro.state;
    BossBodyPose pose{};
    (void)BossIntroActor::Pose(intro, pose);
    const Vec3 tip = Rotate(pose.sword.rotation, {0.0f, 0.0f, 1.0f});
    const float reach = VisibleBladeReach(boss, bodyLook);
    const float fade = 1.0f - local / kTrailSeconds;
    // 追う点が進める長さは、演技の時刻の進みで決める（スローの間は、流体の時間と同じだけ遅くなる）。
    const float step = std::max(0.0f, state.showSeconds - state.smokeTrailAt);
    state.smokeTrailAt = state.showSeconds;
    for (int k = 0; k < kTrailWisps; ++k) {
        const uint64_t id = kTrailId + static_cast<uint64_t>(k);
        const float along = kTrailFrom + (kTrailTo - kTrailFrom) * (static_cast<float>(k) + feed.Roll(id, 4u)) /
                                             static_cast<float>(kTrailWisps);
        const Vec3 target = Add(pose.sword.position, Mul(tip, reach * along));
        Vec3& chaser = state.smokeTrail[static_cast<size_t>(k)];
        chaser = state.smokeTrailSet ? Add(chaser, ClampLength(Sub(target, chaser), kTrailChase * step)) : target;
        Wisp wisp{};
        wisp.at = chaser;
        wisp.radius = 0.32f;
        wisp.gust = 2.5f;
        wisp.heat = kTrailHeat;
        feed.Smoke(id, wisp, intro.settings.drawTrailAmount * fade * fade / static_cast<float>(kTrailWisps));
    }
    state.smokeTrailSet = true;
}

// 刀が地から抜けた瞬間：根元から柱が噴き上がり、地面を煙が外へ転がる。刀身は、地から引きずり出した煙の幕を頭の上へ
// 引いて行く。ボスの体の側へは噴かない：噴いた煙は体の向こうの背景になり、主役を隠さない。
void Draw(const Feed& feed, const BossComponent& boss, const BossBodySettings& bodyLook, BossIntroComponent& intro) {
    const BossIntroSettings& s = intro.settings;
    BossIntroState& state = intro.state;
    const BossIntroActorState& a = state.actor;
    float duration = 0.0f;
    const float local = CueLocal(state, Kind::SwordOut, duration);
    if (local < 0.0f)
        return;
    const Vec3 entry = BossIntroActor::SwordEntry(intro);
    const Vec3 body = FlatNormalize(Sub(BossIntroSmoke::FeetWorld(intro), entry), Mul(Forward(state.yaw), -1.0f));
    // 煙を流して行く向き：刀のある側の外へ、後ろへ。抜刀は正面のカメラで見せるので、根元（主役の斜め前）から噴いた煙が
    // カメラの方へ来ると画を塞ぐ。主役の真後ろへも行かせない（黒い鎧の後ろに黒い煙が来ると、輪郭が読めなくなる）。
    const Vec3 forward = Forward(state.yaw);
    const Vec3 outside = FlatNormalize(ProjectOut(Mul(body, -1.0f), forward), Tangent(forward));
    const Vec3 drift = FlatNormalize(Add(outside, Mul(forward, -kBurstDriftBack)));
    const Vec3 root = Add(entry, Mul(body, -0.5f));
    if (local < s.drawBurstSeconds)
        DrawBurst(feed, s, root, drift, local);
    DrawStreamers(feed, s, root, outside, forward, local);
    // 風：柱と、ボスの周りへ広がった煙を、流す向きへ押し続ける（噴き終わった後もしばらく）。向きはゆっくり左右へ振れ、
    // 少し上へ持ち上げる（煙が一つ所に立ち続けず、漂って散って行く）。
    if (local < s.drawBurstSeconds + kBurstWindSeconds) {
        const float sway = kBurstWindSway * std::sin(local * kBurstWindSwaySpeed + 0.7f);
        const Vec3 wind =
            Normalize(Add(Add(Mul(drift, std::cos(sway)), Mul(Tangent(drift), std::sin(sway))), Up(0.25f)));
        feed.Blow(kWarmId + 2, Add(root, Up(kBurstWindHeight)), kBurstWindRadius, wind, kBurstWind,
                  FluidChannels::kIntroSmoke);
    }
    // 域の縁の手前で薄める（流した先と、上）：煙が域の端まで届くと、そこで断ち切られて見える。
    feed.Absorb(kClearId + 3, Add(root, Add(Mul(drift, kBurstEdgeAhead), Up(4.0f))), kBurstEdgeRadius, kBurstEdgeRate);
    feed.Absorb(kClearId + 4, Add(root, Up(kBurstEdgeAbove)), kBurstEdgeRadius, kBurstEdgeRate);
    // 噴き終わったら、柱を薄めて行く（最後のカットは、刀の根元の向こうから主役を見る。柱が残ると主役の前を塞ぐ）。
    if (local >= s.drawBurstSeconds) {
        feed.Absorb(kClearId + 1, Add(root, Up(2.0f)), kBurstFadeRadius, kBurstFadeRate);
        feed.Absorb(kClearId + 2, Add(root, Up(7.0f)), kBurstFadeRadius, kBurstFadeRate);
    }
    if (local < s.drawBurstSeconds + kBurstChimneySeconds) {
        // 柱の通り道を温め、地面を転がる煙の通り道を開けておく（流す向き）。
        for (int k = 0; k < kBurstChimney; ++k)
            feed.Warm(kWarmId + static_cast<uint64_t>(k),
                      Add(root, Up(kBurstChimneyRadius + kBurstChimneyStep * static_cast<float>(k))),
                      kBurstChimneyRadius, kBurstChimneyHeat);
        feed.Warm(kWarmId + 3, Add(root, Add(Mul(drift, kBurstFloorRadius * 0.8f), Up(0.6f))), kBurstFloorRadius, 0.0f);
    }
    if (state.smokeDrawPush) {
        state.smokeDrawPush = false;
        state.smokeTrailSet = false;
        // 握っている間に根元へ溜めた煙を、1 回突き上げる（抜けた瞬間に、溜まっていた煙が塊のまま噴き上がる）。
        feed.Kick(kWarmId + 5, Add(root, Up(kBurstKickHeight)), kBurstKickRadius, {0.0f, 1.0f, 0.0f}, kBurstKick,
                  FluidChannels::kIntroSmoke);
        // 同時に、根元に集まっていた霧を輪に押し開く（弱く：強く押すと、霧が流体の解いていない所まで飛んで消える）。
        feed.Burst(kBurstId + 14, root, kBurstRingRadius, kBurstRingSpeed, FluidChannels::kIntroSmoke);
        // 場の霧を外へ押す（地面を輪が広がる）。舞台の煙は押さない：噴いたばかりの煙まで域の外へ飛ばしてしまう。
        feed.Burst(kBurstId + 15, entry, s.drawPushRadius, s.drawPush, FluidChannels::kFog);
    }
    if (local < kTrailSeconds && a.socketsValid)
        Trail(feed, boss, bodyLook, intro, local);
}

// 目の炎：目が灯ってから演技の終わりまで、面頬の左右の隙間から青白い炎が細く立つ。兜の前に付いて動く細かい流体の域
// （BossEyeFire。格 3 cm。空気は世界に残るので、頭が動くと炎が後ろへ流れる）へ、熱い煙を細く入れ続ける。目が灯る拍
// （Eyes）の間は強く噴いて、引いて行く。光そのものは兜の中の灯（BossBodyAccents）。
void EyeFire(HIKARI::WorldView& world, const Feed& feed, BossComponent& boss, BossIntroComponent& intro) {
    const BossIntroSettings& s = intro.settings;
    BossIntroState& state = intro.state;
    const BossIntroActorState& a = state.actor;
    if (state.phase != BossIntroPhase::Show || state.skipped || !state.eyesLit || !a.socketsValid ||
        s.eyeFireEffect.IsEmpty() || s.eyeFireAmount <= 0.0f) {
        if (state.eyeFireHandle.IsValid())
            BossVfx::StopHandle(world, state.eyeFireHandle);
        return;
    }
    // 置き場所は、兜そのものの置き場所（兜の原点と傾き）と、模型から取った隙間の帯（BossHelmGlowData）で決める。
    BossBodyPose pose{};
    (void)BossIntroActor::Pose(intro, pose);
    const BossBodyPlacement& helm = pose.head;
    const auto point = [&helm](Vec3 local) {
        return Add(helm.position, Rotate(helm.rotation, Mul(local, BossHelmGlowData::kActorHelmScale)));
    };
    const Vec3 forward = Rotate(helm.rotation, {0.0f, 0.0f, 1.0f});
    const Vec3 side = Rotate(helm.rotation, {1.0f, 0.0f, 0.0f});
    BossVfx::StartOrSend(world, boss, state.eyeFireHandle, s.eyeFireEffect,
                         BossVfx::Placed(point(BossHelmGlowData::kVisorCentre), 1.0f, helm.rotation), {});
    float duration = 0.0f;
    const float local = CueLocal(state, Kind::Eyes, duration);
    const float burst = local >= 0.0f ? (1.0f - local / duration) * (1.0f - local / duration) : 0.0f;
    const float strength = std::min(kEmitStrengthMax, s.eyeFireAmount * (1.0f + kEyeBurst * burst) /
                                                          (kEmitVolume * kEyeRadius * kEyeRadius * kEyeRadius));
    for (int k = 0; k < 2; ++k) {
        const uint64_t id = kEyeId + static_cast<uint64_t>(k);
        const float across = k == 0 ? -kEyeApart : kEyeApart;
        // 目尻から外の上へ流れる。置き場所は隙間の中で少し揺れる。
        const Vec3 at =
            Add(point({across, BossHelmGlowData::kVisorCentre.y, BossHelmGlowData::kVisorCentre.z + kEyeForward}),
                Mul(feed.Jitter(id, 21u), 0.015f));
        const Vec3 velocity = Add(Add(Mul(forward, 0.9f), Up(2.4f + 2.6f * burst)), Mul(side, across * 3.0f));
        feed.Send(id, Fluid::Emit, at, {kEyeRadius, kEyeHeat, 0.0f}, velocity, strength, 0.0f, FluidChannels::kEyeFire);
    }
}

// 舞台の煙を消す（刀の根元・体の周り）。吸収は中心から離れるほど弱いので、何か所かに置く。
void Clear(const Feed& feed, const BossIntroComponent& intro, float rate) {
    const BossIntroState& state = intro.state;
    const Vec3 entry = BossIntroActor::SwordEntry(intro);
    const Vec3 feet = BossIntroSmoke::FeetWorld(intro);
    feed.Absorb(kClearId, Add(entry, Vec3{0.0f, 4.0f, 0.0f}), 9.0f, rate);
    feed.Absorb(kClearId + 1, Add(feet, Vec3{0.0f, 3.0f, 0.0f}), 8.0f, rate);
    const Vec3 corners[4] = {{7.0f, 2.0f, 0.0f}, {-7.0f, 2.0f, 0.0f}, {0.0f, 2.0f, 7.0f}, {0.0f, 2.0f, -7.0f}};
    for (int k = 0; k < 4; ++k)
        feed.Absorb(kClearId + 2 + static_cast<uint64_t>(k), Add(state.origin, corners[k]), 7.0f, rate);
}

} // namespace

void BossIntroSmoke::Render(HIKARI::WorldView& world, HIKARI::ObjectView object, BossComponent& boss,
                            const BossBodySettings& body, BossIntroComponent& intro, float seconds) {
    const BossIntroSettings& s = intro.settings;
    BossIntroState& state = intro.state;
    if (s.smokeEffect.IsEmpty())
        return;
    // 煙の点の行き先（演技の時刻から決める：時間が止まっている間は動かない）。
    const float clock = std::max(0.0f, state.showSeconds) * kWispRate;
    const Feed feed{world, object.GetHandle(), clock};
    Streak(world, feed, boss, intro, seconds);
    EyeFire(world, feed, boss, intro);
    const HIKARI::GameProjectTransform placed = BossVfx::Placed(state.origin, 1.0f, YawRotation(state.yaw));
    // 序幕の霧の点は、演技の前から進む時計で、ゆっくり巡らせる。
    const Feed ambient{world, object.GetHandle(), state.ambientClock * kAmbientWispRate};
    if (state.phase == BossIntroPhase::Scattered) {
        // 演技の前（プレイヤーを待つ間）：部品の周りに黒い霧を溜めておく（最初のカットが始まる時には、もう漂っている）。
        // 舞台の向きが決まる（プレイヤーが見えた tick）までは、域を置かない。
        if (!state.faced && boss.tick <= kAmbientWaitTicks)
            return;
        state.ambientClock += std::max(0.0f, seconds);
        BossVfx::StartOrSend(world, boss, state.smokeHandle, s.smokeEffect, placed, {});
        Ambient(ambient, intro);
        return;
    }
    if (state.phase != BossIntroPhase::Show || state.skipped) {
        // 演技の後：残った煙を消し切ってから域を止める。飛ばした時はすぐ止める（戦いに残さない）。
        if (!state.smokeHandle.IsValid())
            return;
        state.smokeLinger += std::max(0.0f, seconds);
        if (state.skipped || state.smokeLinger >= s.smokeLingerSeconds) {
            BossVfx::StopHandle(world, state.smokeHandle);
            return;
        }
        BossVfx::StartOrSend(world, boss, state.smokeHandle, s.smokeEffect, placed, {});
        // ゲームのカメラへ渡す間：外へ流し続けながら、初めは弱く、だんだん強く消す（カメラが引いて行く間に晴れて行き、
        // 域を止める時には、何も残っていない）。
        const float gone = state.smokeLinger / std::max(kTiny, s.smokeLingerSeconds);
        DisperseFlow(feed, intro, 1.0f - gone);
        Clear(feed, intro, s.clearAbsorption * gone * (1.0f + kLingerClearGain * gone));
        return;
    }
    state.smokeLinger = 0.0f;
    BossVfx::StartOrSend(world, boss, state.smokeHandle, s.smokeEffect, placed, {});
    state.ambientClock += std::max(0.0f, seconds);
    Ambient(ambient, intro);
    const BossIntroActorState& a = state.actor;
    Rise(feed, intro);
    Arms(feed, intro);
    Call(feed, intro, BossIntroPart::GauntletRight, a.rightShoulder.position, kCallId);
    Call(feed, intro, BossIntroPart::GauntletLeft, a.leftShoulder.position, kCallId + 8);
    if (a.socketsValid) {
        Puff(feed, intro, Kind::HandRight, a.rightHand.position, a.rightElbow.position, kPuffId);
        Puff(feed, intro, Kind::HandLeft, a.leftHand.position, a.leftElbow.position, kPuffId + kPuffWisps);
    }
    Lift(feed, intro);
    Seat(feed, intro);
    Step(feed, intro);
    Seep(feed, intro);
    Gather(feed, intro);
    Draw(feed, boss, body, intro);
    Backdrop(feed, intro);
    Disperse(feed, intro);
    float duration = 0.0f;
    if (CueLocal(state, Kind::SmokeClear, duration) >= 0.0f)
        Clear(feed, intro, s.clearAbsorption);
    // 鎧が起きた後、抜刀の柱が立っている間のほかは、体の周りに漂う薄い煙を消し続ける。鎧が起きる前（序幕）は消さない：
    // 部品の周りに溜めている霧まで消してしまう。
    else if (a.socketsValid && Passed(state, Kind::SmokeRise) && CueLocal(state, Kind::SwordOut, duration) < 0.0f)
        feed.Absorb(kClearId, Lerp(a.chest.position, a.head.position, kHazeHead), kHazeRadius, kHazeRate);
}

float BossIntroSmoke::BodyAmount(const BossIntroComponent& intro) {
    const BossIntroState& state = intro.state;
    if (state.phase != BossIntroPhase::Show)
        return 1.0f;
    for (int k = 0; k < state.cueCount; ++k) {
        const BossIntroCue& cue = state.cues[static_cast<size_t>(k)];
        // 拍の名前を見分ける前（演技の最初の 1 フレーム）は、まだ出さない。
        if (cue.kind == Kind::Unread)
            return 0.0f;
        if (cue.kind != Kind::SmokeRise)
            continue;
        if (!cue.entered)
            return 0.0f;
        if (cue.left)
            return 1.0f;
        return Smooth((state.showSeconds - cue.clip.startSeconds) / std::max(kTiny, cue.clip.durationSeconds));
    }
    return 1.0f;
}

HIKARI::GameProjectFloat3 BossIntroSmoke::FeetWorld(const BossIntroComponent& intro) {
    const BossIntroState& state = intro.state;
    if (!state.actor.socketsValid)
        return state.origin;
    return {state.actor.hips.position.x, state.origin.y, state.actor.hips.position.z};
}

HIKARI::GameProjectFloat3 BossIntroSmoke::SwordEntryWorld(const BossIntroComponent& intro) {
    return BossIntroActor::SwordEntry(intro);
}

} // namespace Project_TD2_2
