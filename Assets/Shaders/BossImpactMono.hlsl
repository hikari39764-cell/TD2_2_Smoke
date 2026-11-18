#include "Include/Post/HIKARI_PostEffectInterface.hlsli"

Texture2D<float4> gSceneTex : register(t0);
// s0 は必ず使う（使わないとコンパイラが外し、ポスト効果の契約「b0・t0・s0 が要る」を満たさず、実行時に効果が断られる）。
SamplerState gLinearClamp : register(s0);

// ボスの登場の締めの一振りが当たった瞬間の、白黒の閃き（Action Post。露出済みのシーン線形 HDR のまま返す。ブルームの前）。
// 明るさで白と黒に分け（硬さを上げるとほぼ 2 値になる）、反転もできる。強さで元の色と混ぜる。
// gUser の置き場所（BossImpactMono.hposteffect の params と同じ。0 は Boss が毎フレーム送る）：
// 0: x 強さ (0〜1)、y 反転 (0〜1)
// 1: x 白と黒の境の明るさ（露出済みの線形）、y 硬さ（明るさが 2 倍になる間に進む量）、z 白の明るさ、w 黒の明るさ
float4 main(HikariPostEffectInput input) : SV_TARGET
{
    const float4 scene = gSceneTex.SampleLevel(gLinearClamp, input.uv, 0.0f);
    const float strength = saturate(gUser[0].x);
    if (strength <= 0.0f)
        return scene;
    const float luminance = dot(max(scene.rgb, 0.0f.xxx), float3(0.2126f, 0.7152f, 0.0722f));
    const float pivot = max(gUser[1].x, 1.0e-4f);
    float mono = saturate(0.5f + (log2(max(luminance, 1.0e-5f)) - log2(pivot)) * max(gUser[1].y, 0.0f));
    mono = lerp(mono, 1.0f - mono, saturate(gUser[0].y));
    const float level = lerp(gUser[1].w, gUser[1].z, mono);
    return float4(lerp(scene.rgb, level.xxx, strength), scene.a);
}
