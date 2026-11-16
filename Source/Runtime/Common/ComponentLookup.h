#pragma once

#include <string_view>

#include "HIKARI/Project/WorldView.h"
#include "HIKARI/Runtime/ComponentTypeId.h"

// 同じ物体に付いたコンポーネントの探し方（各 System で「全部を回して、同じ物体のものだけ拾う」と書いていたものをまとめた）。
namespace Project_TD2_2::ComponentLookup {

// object と同じ物体に付いた、型 typeName のコンポーネントごとに callback(ObjectView, T&) を呼ぶ。並びは
// ForEachComponent のまま（普通は 1 つ）。最初の 1 つだけ使いたい時は、呼ぶ側で印を持つ。
template <class T, class Callback>
void ForEachOnObject(HIKARI::WorldView& world, std::string_view typeName, HIKARI::ObjectView object,
                     Callback&& callback) {
    const auto handle = object.GetHandle().value;
    world.ForEachComponent<T>(HIKARI::MakeComponentTypeId(typeName), [&](HIKARI::ObjectView other, T& component) {
        if (other.GetHandle().value == handle)
            callback(other, component);
    });
}

} // namespace Project_TD2_2::ComponentLookup
