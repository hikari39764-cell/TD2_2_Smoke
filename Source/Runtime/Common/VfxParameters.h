#pragma once

#include <cstdio>

#include "HIKARI/Project/VfxTypes.h"

// エフェクト資産の入力（input.xxx）へ渡す値を組み立てる。入力の名前は資産側の契約。
namespace Project_TD2_2::VfxParameters {

inline HIKARI::GameProjectBindingValue& Add(HIKARI::GameProjectVfxParameterOverrides& parameters,
                                            const char* inputId) {
    // 上限を超える呼び出しはコードの誤り。範囲外へ書かず、最後の枠を使い回す。
    const uint32_t last = HIKARI::kGameProjectVfxMaximumParameterOverrides - 1u;
    const uint32_t index = parameters.count <= last ? parameters.count++ : last;
    auto& entry = parameters.values[index];
    std::snprintf(entry.inputId, sizeof(entry.inputId), "%s", inputId);
    entry.value = {};
    return entry.value;
}

inline void SetFloat(HIKARI::GameProjectVfxParameterOverrides& parameters, const char* inputId, float value) {
    Add(parameters, inputId).numeric[0] = value;
}

inline void SetColor(HIKARI::GameProjectVfxParameterOverrides& parameters, const char* inputId,
                     HIKARI::GameProjectFloat3 color) {
    auto& entry = Add(parameters, inputId);
    entry.type = HIKARI::GameProjectBindingValueType::Color;
    entry.numeric[0] = color.x;
    entry.numeric[1] = color.y;
    entry.numeric[2] = color.z;
    entry.numeric[3] = 1.0f;
}

} // namespace Project_TD2_2::VfxParameters
