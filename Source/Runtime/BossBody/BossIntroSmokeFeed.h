#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

#include "HIKARI/Project/FluidTypes.h"
#include "HIKARI/Project/WorldView.h"
#include "../Boss/BossMath.h"
#include "../Common/FluidChannels.h"

namespace Project_TD2_2 {

// 登場の煙（c-e0c500）の、流体への出し方：煙の点 1 つの形（Wisp）と、1 件の提出（Feed）。拍ごとの形は BossIntroSmoke.cpp。
namespace BossIntroSmokeFeed {

using namespace BossMath;
using Fluid = HIKARI::GameProjectFluidInteractionKind;

// 煙の入れ方の決まり：大きな球を 1 つ置くと、球の形の塊がそのまま見える。小さな点を、決めた範囲の中で落ち着きなく
// 滑らせながら、速さも揺らして入れる。エンジンは点が動いた線分の上へ掃いて煙を入れるので、うねる細い帯の束になり、
// あとは流体が渦に巻く。点を毎フレーム別の所へ跳ばしてはいけない（掃かれずに、小さな球の粒が並んで見える。スローの
// 間は特に目立つ）。
// 点が行き先を変える速さ（1 秒の回数）。行き先から行き先へは滑らかに動く。
inline constexpr float kWispRate = 5.0f;
// 半径 r の点（中心から二乗に弱まる）へ毎秒 1 の強さで入れた時に、域へ入る煙の総量は (4π/30) r³。量を「1 秒に入れる
// 煙の総量」で決めるので、点を細かくしても全体の濃さは変わらない。強さの上限はエンジンの受け付ける範囲。
inline constexpr float kEmitVolume = 0.41888f;
inline constexpr float kEmitStrengthMax = 10000.0f;
// 通り道を温める時に足す煙の強さ（見えない薄さ。0 だとエンジンが受け付けない）。
inline constexpr float kTraceStrength = 1.0e-4f;
// 点の大きさの下限 (m)：格（15 cm）2 つより細いと、格の間に落ちて量が減る。
inline constexpr float kWispRadiusMin = 0.36f;

// 番号・刻み・用途から決まる 0〜1 の値（同じ時刻なら同じ値。演技を流し直しても同じ煙になる）。
inline float Hash(uint64_t id, uint32_t tick, uint32_t salt) {
    uint32_t h =
        static_cast<uint32_t>(id) * 0x9E3779B1u ^ (tick + 0x7F4A7C15u) * 0x85EBCA6Bu ^ (salt + 1u) * 0xC2B2AE35u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return static_cast<float>(h & 0xFFFFFFu) / 16777216.0f;
}

inline Vec3 Up(float height) { return {0.0f, height, 0.0f}; }
inline Vec3 Around(float angle) { return {std::sin(angle), 0.0f, std::cos(angle)}; }
// 水平の向き out を、上から見て 90 度回した向き（輪の接線）。
inline Vec3 Tangent(Vec3 out) { return {out.z, 0.0f, -out.x}; }

// 煙の点 1 つ。at の周り spread の箱の中を落ち着きなく滑り、向かう速さも gust だけ揺れる。
struct Wisp {
    Vec3 at{};
    float spread = 0.0f;
    float radius = 0.25f;
    Vec3 velocity{};
    float gust = 0.0f;
    // 温度：域の浮力の倍率が 1 なので、そのまま上向きの加速 (m/秒²)。0 の煙は重さで地面へ沈んで這う。
    float heat = 0.0f;
};

// 流体への 1 件。at・direction は世界。
struct Feed {
    HIKARI::WorldView& world;
    HIKARI::GameProjectObjectHandle owner;
    // 点をうねらせる時計（演技の時刻 × kWispRate）。
    float clock = 0.0f;

    void Send(uint64_t id, Fluid kind, Vec3 at, Vec3 extents, Vec3 direction, float strength, float absorption,
              uint32_t channels) const {
        if (extents.x <= 0.001f || (strength == 0.0f && absorption <= 0.0f))
            return;
        HIKARI::GameProjectFluidInteraction fluid{};
        fluid.kind = kind;
        fluid.position = at;
        fluid.extents = extents;
        fluid.direction = direction;
        fluid.strength = strength;
        fluid.absorption = absorption;
        fluid.channels = channels;
        world.GetFluid().Submit(owner, id, fluid);
    }
    // 今の 0〜1 の値（行き先から行き先へ滑らかに変わる）と、各成分 -1〜1 の向き。
    float Roll(uint64_t id, uint32_t salt) const {
        // 速さの違う 2 つの折れ線を、向きを変える時刻をずらして重ねる：行き先ごとに止まらず、うねり続ける
        // （止まっては動く形だと、煙が脈を打って出て、跳ぶように見える）。
        const float slow = clock * 0.61f + 0.5f;
        const uint32_t a = static_cast<uint32_t>(clock);
        const uint32_t b = static_cast<uint32_t>(slow);
        return 0.5f * Lerp(Hash(id, a, salt), Hash(id, a + 1u, salt), clock - static_cast<float>(a)) +
               0.5f * Lerp(Hash(id, b, salt + 97u), Hash(id, b + 1u, salt + 97u), slow - static_cast<float>(b));
    }
    Vec3 Jitter(uint64_t id, uint32_t salt) const {
        return {Roll(id, salt) * 2.0f - 1.0f, Roll(id, salt + 1u) * 2.0f - 1.0f, Roll(id, salt + 2u) * 2.0f - 1.0f};
    }
    // 煙を入れる。mass＝この点が 1 秒に入れる煙の総量。
    void Smoke(uint64_t id, const Wisp& wisp, float mass, uint32_t channels = FluidChannels::kIntroSmoke) const {
        if (mass <= 0.0f)
            return;
        const float radius = std::max(kWispRadiusMin, wisp.radius);
        const float strength = std::min(kEmitStrengthMax, mass / (kEmitVolume * radius * radius * radius));
        Send(id, Fluid::Emit, Add(wisp.at, Mul(Jitter(id, 11u), wisp.spread)), {radius, wisp.heat, 0.0f},
             Add(wisp.velocity, Mul(Jitter(id, 17u), wisp.gust)), strength, 0.0f, channels);
    }
    // 半径の中の空気を heat の温度へ寄せる（煙はほとんど足さない）。流体は「煙があり得る所」だけを解くので、速い煙は
    // まだ解いていない所へ入って消える。柱の通り道をこれで先に温めておくと、そこが解く所に入り、温められた空気が
    // 柱を引き上げる（煙突になる）。heat 0 なら、地面を這う煙の通り道（冷やして沈める）。
    void Warm(uint64_t id, Vec3 at, float radius, float heat) const {
        Send(id, Fluid::Emit, at, {radius, heat, 0.0f}, {}, kTraceStrength, 0.0f, FluidChannels::kIntroSmoke);
    }
    // その場から外へ 1 回押す（膨らむ）。
    void Burst(uint64_t id, Vec3 at, float radius, float speed, uint32_t channels) const {
        Send(id, Fluid::Impulse, at, {radius, 0.0f, 0.0f}, {}, speed, 0.0f, channels);
    }
    // 向き direction へ 1 回押す（その場の煙が塊のまま飛び出し、渦の輪を巻く）。
    void Kick(uint64_t id, Vec3 at, float radius, Vec3 direction, float speed, uint32_t channels) const {
        Send(id, Fluid::Impulse, at, {radius, 0.0f, 0.0f}, direction, speed, 0.0f, channels);
    }
    // 半径の中の空気を、向き direction へ押し続ける（出している間。加速 m/秒²）。煙の点が持たせる速さは点の中にしか
    // 効かないので、噴き上がる柱や地面を走る煙は、この力で運ぶ（周りの空気を巻き込んで、渦のある柱になる）。
    void Blow(uint64_t id, Vec3 at, float radius, Vec3 direction, float acceleration, uint32_t channels) const {
        Send(id, Fluid::Force, at, {radius, 0.0f, 0.0f}, direction, acceleration, 0.0f, channels);
    }
    // 半径の中の煙を消す。
    void Absorb(uint64_t id, Vec3 at, float radius, float rate) const {
        Send(id, Fluid::Force, at, {radius, 0.0f, 0.0f}, {}, 0.0f, rate, FluidChannels::kIntroSmoke);
    }
};

} // namespace BossIntroSmokeFeed
} // namespace Project_TD2_2
