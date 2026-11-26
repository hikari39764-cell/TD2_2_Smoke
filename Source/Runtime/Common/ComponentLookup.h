#pragma once

#include <string_view>
#include <type_traits>

#include "HIKARI/Extend/World.h"
#include "HIKARI/Project/WorldView.h"
#include "HIKARI/Runtime/ComponentTypeId.h"

// 同じ物体に付いたコンポーネントの探し方（各 System で「全部を回して、同じ物体のものだけ拾う」と書いていたものをまとめた）。
namespace Project_TD2_2::ComponentLookup {

// 新しいゲーム層の部品（HIKARI_COMPONENT）を、旧い System の回呼の中から物体と一緒に引く。旧い回呼の間も新しい層の
// 呼び出しの場が立っているので、ForEach・Entity::Get がそのまま使える（移行の途中だけ。System が新しい層へ移れば消す）。
// 場面の T を全部、callback(ObjectView, T&) で回す。
template <class T, class Callback>
void ForEachGame(Callback&& callback) {
    HIKARI::ForEach<T>(
        [&](HIKARI::Entity entity, T& component) { callback(HIKARI::Extend::ObjectOf(entity), component); });
}

// object と同じ物体に付いた新しい層の部品 T。無ければ nullptr。
template <class T>
T* GameOnObject(HIKARI::ObjectView object) {
    return HIKARI::Extend::EntityOf(object.GetHandle()).template Get<T>();
}

// object と同じ物体に付いた、型 typeName のコンポーネントごとに callback(ObjectView, T&) を呼ぶ。並びは
// ForEachComponent のまま（普通は 1 つ）。最初の 1 つだけ使いたい時は、呼ぶ側で印を持つ。
// 新しい層の部品（HIKARI::Component）は型名を使わず、その物体の部品を直接引く。
template <class T, class Callback>
void ForEachOnObject(HIKARI::WorldView& world, std::string_view typeName, HIKARI::ObjectView object,
                     Callback&& callback) {
    if constexpr (std::is_base_of_v<HIKARI::Component, T>) {
        (void)world;
        (void)typeName;
        if (T* component = GameOnObject<T>(object))
            callback(object, *component);
    } else {
        const auto handle = object.GetHandle().value;
        world.ForEachComponent<T>(HIKARI::MakeComponentTypeId(typeName), [&](HIKARI::ObjectView other, T& component) {
            if (other.GetHandle().value == handle)
                callback(other, component);
        });
    }
}

// object と同じ物体に付いた、型 typeName の最初のコンポーネントを callback(T*) に渡す（無ければ nullptr を渡す）。
// 参照はクエリのコールバックの中だけで使うので、callback もその中で呼ぶ。
template <class T, class Callback>
void WithFirstOnObject(HIKARI::WorldView& world, std::string_view typeName, HIKARI::ObjectView object,
                       Callback&& callback) {
    bool done = false;
    ForEachOnObject<T>(world, typeName, object, [&](HIKARI::ObjectView, T& component) {
        if (done)
            return;
        done = true;
        callback(&component);
    });
    if (!done)
        callback(static_cast<T*>(nullptr));
}

} // namespace Project_TD2_2::ComponentLookup
