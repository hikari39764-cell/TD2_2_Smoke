#include "Include/Post/HIKARI_PostEffectInterface.hlsli"
#include "Include/Post/HIKARI_PostHash.hlsli"

Texture2D<float4> gSceneTex : register(t0);
// s0 は必ず使う（使わないとコンパイラが外し、ポスト効果の契約「b0・t0・s0 が要る」を満たさず、実行時に効果が断られる。
// 取り込みは通るので気付きにくい）。
SamplerState gLinearClamp : register(s0);

// 筋斗雲の速度感（Action Post。露出済みのシーン線形 HDR のまま返す。ブルームの前）。
// 進む向きを画面へ写した点（流れの中心）を澄ませたまま、そこから離れるほど縁を放射状に流す：
//   中心へ向かう放射状のぼかし（取り出しの位置を中心へ寄せて並べる）・外へ流れる細い筋（明るさを掛け算で少し上げる）・
//   ぼかしに乗るわずかな色ずれ（赤を外、青を内から取る）・画面の縁の減光。体と雲（守る楕円）の内は流さない。
// gUser の置き場所（FlightSpeed.hposteffect の params と同じ。0 と 1 は CloudMount が毎フレーム送る）：
// 0: x 強さ (0〜1)、y・z 流れの中心 UV（画面の外も取る）
// 1: x・y 守る楕円の中心 UV、z 半径（画面の高さを 1）、w 有効 (0/1)
// 2: x 澄んだ半径、y 流れが全部掛かる半径（どちらも流れの中心から、画面の高さを 1）、
//    z ぼかしの長さ（中心までの距離に対する割合、強さ 1 で）、w 守る楕円の縁のぼかし（半径に対する割合）
// 3: x 筋の明るさ（強さ 1 で足す割合）、y 筋の本数（一周）、z 筋の流れる速さ（刻み/秒）、w 筋のある本の割合
// 4: x 色ずれ（画面の隅での画素、強さ 1 で。ぼかしの取り出しの間隔を超えない）、y 縁の減光（強さ 1 の隅で）、
//    z 筋の太さ（1 本の幅に対する割合）、w 筋の明るい長さ（刻みに対する割合）
// 5: x 筋の刻みの細かさ（中心からの距離が 2 倍になる間の刻みの数）、y 減光の始まり（画面の中心 0〜隅 1）

// ぼかしの取り出しの数（重さと段の目立ち方の釣り合い。この 2 つ前後を足して 14 回読む）。
static const int kTaps = 12;
static const float kTwoPi = 6.28318531f;

// 守る楕円の外の割合（内 0、縁のぼかしを越えて 1）。
float OutsideGuard(float2 uv, float aspect)
{
    if (gUser[1].w < 0.5f || gUser[1].z <= 0.0f)
        return 1.0f;
    const float radius = gUser[1].z;
    const float feather = max(gUser[2].w, 1.0e-3f) * radius;
    return smoothstep(radius, radius + feather, length((uv - gUser[1].xy) * float2(aspect, 1.0f)));
}

float4 main(HikariPostEffectInput input) : SV_TARGET
{
    const float4 scene = gSceneTex.SampleLevel(gLinearClamp, input.uv, 0.0f);
    const float strength = saturate(gUser[0].x);
    if (strength <= 0.0f)
        return scene;

    const float2 resolution = max(float2(gResolutionX, gResolutionY), 1.0f.xx);
    const float aspect = resolution.x / resolution.y;
    const float2 center = gUser[0].yz;
    const float2 offset = (input.uv - center) * float2(aspect, 1.0f);
    const float dist = length(offset);
    const float sharpRadius = max(gUser[2].x, 0.0f);
    const float fullRadius = max(gUser[2].y, sharpRadius + 1.0e-3f);
    const float amount = strength * smoothstep(sharpRadius, fullRadius, dist) * OutsideGuard(input.uv, aspect);

    float3 color = scene.rgb;
    [branch]
    if (amount > 0.0f)
    {
        // 放射状のぼかし：画素から流れの中心へ向けて kTaps 個を並べて平均する。並びの長さは中心までの距離 × 割合 × 強さ
        // （中心へ近いほど短い）。守る楕円の内の取り出しは数えない（体の色を外へ引きずらない）。
        const float2 stepUv = (center - input.uv) * (max(gUser[2].z, 0.0f) * amount / float(kTaps));
        const float stepPixels = length(stepUv * resolution);
        const float cornerDistance = 0.5f * length(float2(aspect, 1.0f));
        const float shiftPixels = max(gUser[4].x, 0.0f) * amount * saturate(dist / cornerDistance);
        const float shift = stepPixels > 1.0e-4f ? saturate(shiftPixels / stepPixels) : 0.0f;
        // 画素ごとに決まった揺らぎ（時間では変えない。取り出しの段を目立たなくする）。
        const float jitter = HikariInterleavedGradientNoise(input.position.xy, 0u);
        float3 outer = gSceneTex.SampleLevel(gLinearClamp, input.uv + stepUv * (jitter - 1.0f), 0.0f).rgb;
        float2 uv = input.uv + stepUv * jitter;
        float3 current = gSceneTex.SampleLevel(gLinearClamp, uv, 0.0f).rgb;
        float3 sum = 0.0f.xxx;
        float weight = 0.0f;
        [unroll]
        for (int i = 0; i < kTaps; ++i)
        {
            const float2 nextUv = uv + stepUv;
            const float3 inner = gSceneTex.SampleLevel(gLinearClamp, nextUv, 0.0f).rgb;
            const float w = OutsideGuard(uv, aspect);
            // 赤は一つ外、青は一つ内の取り出しへ shift だけ寄せる（色ずれ。ぼかしの間隔を超えない）。
            sum += w * float3(lerp(current.r, outer.r, shift), current.g, lerp(current.b, inner.b, shift));
            weight += w;
            outer = current;
            current = inner;
            uv = nextUv;
        }
        color = weight > 1.0e-3f ? sum / weight : scene.rgb;

        // 外へ流れる細い筋：角度を本数に分け、本ごとに有る無し・刻みの位置・速さを決める。刻みは中心からの距離の対数
        // （遠いほど長い）に並べ、時間で外へ流す。明るさは掛け算（露出に寄らず、暗い所では目立たない）。
        const float lanes = max(floor(gUser[3].y), 1.0f);
        const float lane = atan2(offset.y, offset.x) / kTwoPi * lanes;
        const float cell = floor(lane);
        const float across = abs(frac(lane) - 0.5f);
        const float pixelInLanes = lanes / (kTwoPi * max(dist * resolution.y, 1.0f));
        const float halfWidth = 0.5f * saturate(gUser[4].z);
        const float streakLine = 1.0f - smoothstep(halfWidth, halfWidth + pixelInLanes, across);
        const float present = step(1.0f - saturate(gUser[3].w), HikariHash12(float2(cell, 3.7f)));
        const float pace = 0.75f + 0.5f * HikariHash12(float2(cell, 23.9f));
        const float phase = frac(log2(max(dist, 1.0e-3f)) * max(gUser[5].x, 0.0f) - gTime * gUser[3].z * pace +
                                 HikariHash12(float2(cell, 11.3f)));
        const float onLength = max(saturate(gUser[4].w), 1.0e-3f);
        const float dash = smoothstep(0.0f, 0.3f * onLength, phase) * (1.0f - smoothstep(0.6f * onLength, onLength, phase));
        color *= 1.0f + max(gUser[3].x, 0.0f) * amount * present * streakLine * dash;
    }

    // 画面の縁の減光（画面の中心から測る。強さに合わせる）。
    const float2 centered = (input.uv - 0.5f) * float2(aspect, 1.0f);
    const float edge = saturate(length(centered) / (0.5f * length(float2(aspect, 1.0f))));
    const float start = saturate(gUser[5].y);
    color *= 1.0f - saturate(gUser[4].y) * strength * smoothstep(start, max(start + 1.0e-3f, 1.0f), edge);
    return float4(max(color, 0.0f), scene.a);
}
