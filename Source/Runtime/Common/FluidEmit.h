#pragma once

#include <algorithm>

#include "VectorMath.h"

// 流体の場の霧へ煙を注入する時の量の計算（ボスの煙と、ひょうたんの砲撃の戻りで同じ式を使っていたものをまとめた）。
namespace Project_TD2_2::FluidEmit {

// 注入（Emit）が 1 秒に足す量 [密度 × m³] ÷ 強さ。重みは中心から (1 - r/R)² で、床（中心から height 下）より下は
// 域の外なので数えない。球全体の上半分は πR³/15、中心より下の厚さ h の層は 2π(R²h/12 − h³/6 + h⁴/6R − h⁵/20R²)。
// エンジンの注入の式が変わったら、ここを直す。radius は 0 より大きいこと。
inline float EmitVolume(float radius, float height) {
    using VectorMath::kPi;
    const float h = std::clamp(height, 0.0f, radius);
    const float upper = kPi * radius * radius * radius / 15.0f;
    const float lower = 2.0f * kPi *
                        (radius * radius * h / 12.0f - h * h * h / 6.0f + h * h * h * h / (6.0f * radius) -
                         h * h * h * h * h / (20.0f * radius * radius));
    return upper + lower;
}

} // namespace Project_TD2_2::FluidEmit
