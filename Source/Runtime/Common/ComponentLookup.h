#pragma once

#include "HIKARI/Extend/World.h"
#include "HIKARI/Project/WorldView.h"

// 同じ物体に付いたコンポーネントの探し方（各 System で「全部を回して、同じ物体のものだけ拾う」と書いていたものをまとめた）。
namespace Project_TD2_2::ComponentLookup {

// 新しいゲーム層の部品（HIKARI_COMPONENT）を、物体（ObjectView）と一緒に引く。システムの段の中身（旧い IGameSystem の
// 回呼の名前の関数）は、物体の手綱（WorldView・ObjectView）で書いてあるので、新しい層の ForEach・Entity::Get との橋になる。
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

// object と同じ物体に付いた T のコンポーネントごとに callback(ObjectView, T&) を呼ぶ（普通は 1 つ。最初の 1 つだけ使いたい時は
// WithFirstOnObject）。
template <class T, class Callback>
void ForEachOnObject(HIKARI::ObjectView object, Callback&& callback) {
    if (T* component = GameOnObject<T>(object))
        callback(object, *component);
}

// object と同じ物体に付いた T の最初のコンポーネントを callback(T*) に渡す（無ければ nullptr を渡す）。
// 参照はクエリのコールバックの中だけで使うので、callback もその中で呼ぶ。
template <class T, class Callback>
void WithFirstOnObject(HIKARI::ObjectView object, Callback&& callback) {
    callback(GameOnObject<T>(object));
}

} // namespace Project_TD2_2::ComponentLookup
