#pragma once

#include <cstdint>
#include <cstdio>
#include <string_view>

#include "HIKARI/Project/GameComponent.h"
#include "HIKARI/Project/ProjectLog.h"
#include "HIKARI/Project/WorldView.h"

namespace Project_TD2_2::ObjectVariables {

// 物体変数は ProjectSettings/runtime_bindings.json に宣言（ID と型）してからでないと書けない。
// 毎フレーム書くので、断られた時は部品ごとに最初の 1 回だけ警告を出し、後はログを埋めない。
// warned は部品が持つ。
inline void WarnRejectedOnce(bool& warned, std::string_view category, std::string_view id) {
    if (warned)
        return;
    warned = true;
    char message[512]{};
    std::snprintf(message, sizeof(message),
                  "物体変数 %.*s を書き込めません。"
                  "ProjectSettings/runtime_bindings.json の宣言（ID と型）を確認してください。",
                  static_cast<int>(id.size()), id.data());
    HIKARI::WriteProjectLog(HIKARI::GameProjectLogLevel::Warning, category, message);
}

// 宣言が Float の変数へ書く。
inline void PublishFloat(HIKARI::GameComponentContext& context, std::string_view category, std::string_view id,
                         float value, bool& warned) {
    HIKARI::GameProjectBindingValue binding{};
    binding.type = HIKARI::GameProjectBindingValueType::Float;
    binding.numeric[0] = value;
    if (!context.WriteObjectVariable(id, binding))
        WarnRejectedOnce(warned, category, id);
}

// 宣言が Int の変数へ書く。
inline void PublishInt(HIKARI::GameComponentContext& context, std::string_view category, std::string_view id,
                       int64_t value, bool& warned) {
    HIKARI::GameProjectBindingValue binding{};
    binding.type = HIKARI::GameProjectBindingValueType::Int;
    binding.integer = value;
    if (!context.WriteObjectVariable(id, binding))
        WarnRejectedOnce(warned, category, id);
}

// 確かめ用の読み戻し。読めて、型が合った時だけ true（out はその時だけ変える）。
inline bool ReadFloat(const HIKARI::WorldView& world, HIKARI::GameProjectObjectHandle object, std::string_view id,
                      float& out) {
    HIKARI::GameProjectBindingValue binding{};
    if (!world.ReadObjectVariable(object, id, binding) || binding.type != HIKARI::GameProjectBindingValueType::Float)
        return false;
    out = binding.numeric[0];
    return true;
}

inline bool ReadInt(const HIKARI::WorldView& world, HIKARI::GameProjectObjectHandle object, std::string_view id,
                    int64_t& out) {
    HIKARI::GameProjectBindingValue binding{};
    if (!world.ReadObjectVariable(object, id, binding) || binding.type != HIKARI::GameProjectBindingValueType::Int)
        return false;
    out = binding.integer;
    return true;
}

} // namespace Project_TD2_2::ObjectVariables
