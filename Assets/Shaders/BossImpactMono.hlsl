#include "Include/Post/HIKARI_PostEffectInterface.hlsli"

Texture2D<float4> gSceneTex : register(t0);
// s0 は必ず使う（使わないとコンパイラが外し、ポスト効果の契約「b0・t0・s0 が要る」を満たさず、実行時に効果が断られる）。
SamplerState gLinearClamp : register(s0);

// ボスの大きな瞬間の全画面の効果（Action Post。露出済みのシーン線形 HDR のまま返す。ブルームの前）。2 つを重ねる：
// (1) 衝撃：世界の 1 点（刀の根元）から輪が広がる。輪の所で画が外へ歪み、縁が光る。輪が通るまで、画は中心から外へ
//     流れ（中心へ向かうぼかし）、色がずれる。握っている間は「張りつめ」として、縁が暗く締まり、色がわずかにずれる。
// (2) 白黒の閃き：明るさで白と黒に分け（硬さを上げるとほぼ 2 値になる）、反転もできる。強さで元の色と混ぜる。
// gUser の置き場所（BossImpactMono.hposteffect の params と同じ。0・2・3 は Boss が毎フレーム送る）：
// 0: x 白黒の強さ (0〜1)、y 反転 (0〜1)
// 1: x 白と黒の境の明るさ（露出済みの線形）、y 硬さ（明るさが 2 倍になる間に進む量）、z 白の明るさ、w 黒の明るさ
// 2: x 衝撃の強さ (0〜1)、y 輪の半径（画面の高さを 1）、z・w 輪の中心 UV
// 3: x 張りつめ (0〜1)、y 暗さ (0〜1。前触れの間、画の全体が暗く、色が沈む)
// 5: x 剣先の星の強さ (0〜1)、y・z 星の中心 UV、w 星の大きさ（画面の高さを 1）
// 6: x 星の明るさ（露出済みの線形）
// 4: x 輪の幅（画面の高さを 1）、y 輪の歪みの大きさ（同）、z 中心へのぼかしの長さ（中心までの距離に対する割合）、
//    w 色ずれ（画素。画面の高さ 1080 の時）
static const int kShockTaps = 6;

// 衝撃と張りつめを掛けた色（どちらも 0 なら、そのままの画）。
float3 ShockColor(float2 uv)
{
    const float shock = saturate(gUser[2].x);
    const float tension = saturate(gUser[3].x);
    const float dim = saturate(gUser[3].y);
    if (shock <= 0.0f && tension <= 0.0f)
    {
        // 暗さだけの時：画の全体を暗くし、色を少し沈める（日が陰ったように）。
        const float3 plain = gSceneTex.SampleLevel(gLinearClamp, uv, 0.0f).rgb;
        const float plainGrey = dot(max(plain, 0.0f.xxx), float3(0.2126f, 0.7152f, 0.0722f));
        return lerp(plain, plainGrey.xxx * float3(0.92f, 0.97f, 1.06f), 0.4f * dim) * (1.0f - 0.5f * dim);
    }
    const float2 aspect = float2(gResolutionX / max(gResolutionY, 1.0f), 1.0f);
    const float2 center = gUser[2].zw;
    const float2 offset = (uv - center) * aspect;
    const float distance = length(offset);
    const float2 outward = distance > 1.0e-4f ? offset / distance : float2(0.0f, 0.0f);
    const float width = max(gUser[4].x, 1.0e-3f);
    const float band = (distance - gUser[2].y) / width;
    const float ring = exp(-band * band) * shock;
    // 輪の所の画を外へずらす（空気の歪み）。
    const float2 base = uv - outward / aspect * (ring * gUser[4].y);
    // 中心へ向かうぼかしと色ずれ（赤を外、青を内から取る）。
    const float blur = gUser[4].z * shock * saturate(distance * 2.0f);
    const float shift = gUser[4].w * (shock + 0.5f * tension) / 1080.0f;
    float3 sum = 0.0f.xxx;
    for (int i = 0; i < kShockTaps; ++i)
    {
        const float t = ((float)i + 0.5f) / (float)kShockTaps;
        const float2 at = base - (base - center) * (blur * t);
        const float2 split = outward / aspect * (shift * (0.5f + t));
        sum.r += gSceneTex.SampleLevel(gLinearClamp, at + split, 0.0f).r;
        sum.g += gSceneTex.SampleLevel(gLinearClamp, at, 0.0f).g;
        sum.b += gSceneTex.SampleLevel(gLinearClamp, at - split, 0.0f).b;
    }
    float3 color = sum / (float)kShockTaps;
    // 輪の内側は一瞬暗く、輪の縁は明るく（暗 → 明の波として読める）。張りつめの間は、画の縁を暗くする。
    const float inside = shock * saturate(1.0f - distance / max(gUser[2].y, 1.0e-3f)) * saturate(gUser[2].y * 3.0f);
    color *= (1.0f - 0.35f * inside) * (1.0f + ring * 2.2f);
    const float edge = length((uv - 0.5f) * aspect);
    color *= 1.0f - 0.4f * tension * smoothstep(0.3f, 0.95f, edge);
    const float grey = dot(max(color, 0.0f.xxx), float3(0.2126f, 0.7152f, 0.0722f));
    return lerp(color, grey.xxx * float3(0.92f, 0.97f, 1.06f), 0.4f * dim) * (1.0f - 0.5f * dim);
}

// 剣先の星：横に長く縦に短い細い十字の光と、小さな芯。
float3 StarLight(float2 uv)
{
    const float star = max(gUser[5].x, 0.0f);
    if (star <= 0.0f)
        return 0.0f.xxx;
    const float2 aspect = float2(gResolutionX / max(gResolutionY, 1.0f), 1.0f);
    const float2 offset = (uv - gUser[5].yz) * aspect;
    const float size = max(gUser[5].w, 1.0e-3f);
    const float core = exp(-dot(offset, offset) / (size * size * 0.004f));
    const float wide = exp(-abs(offset.y) / (size * 0.012f)) * exp(-abs(offset.x) / (size * 0.55f));
    const float tall = exp(-abs(offset.x) / (size * 0.012f)) * exp(-abs(offset.y) / (size * 0.28f));
    return float3(0.72f, 0.86f, 1.0f) * (star * gUser[6].x * (core * 2.5f + wide + 0.7f * tall));
}

float4 main(HikariPostEffectInput input) : SV_TARGET
{
    const float4 scene = gSceneTex.SampleLevel(gLinearClamp, input.uv, 0.0f);
    const float3 color = ShockColor(input.uv) + StarLight(input.uv);
    const float strength = saturate(gUser[0].x);
    if (strength <= 0.0f)
        return float4(color, scene.a);
    const float luminance = dot(max(color, 0.0f.xxx), float3(0.2126f, 0.7152f, 0.0722f));
    const float pivot = max(gUser[1].x, 1.0e-4f);
    float mono = saturate(0.5f + (log2(max(luminance, 1.0e-5f)) - log2(pivot)) * max(gUser[1].y, 0.0f));
    mono = lerp(mono, 1.0f - mono, saturate(gUser[0].y));
    const float level = lerp(gUser[1].w, gUser[1].z, mono);
    return float4(lerp(color, level.xxx, strength), scene.a);
}
