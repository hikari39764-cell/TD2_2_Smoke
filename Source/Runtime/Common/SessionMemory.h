#pragma once

// Play の間、シーンの読み直し（やり直し）をまたいで覚えておくこと。シーンと一緒に消える部品・システムには置けないので、
// シーンをまたぐシステム（HIKARI_PERSIST。SessionMemory.cpp）が持つ。
namespace Project_TD2_2 {

struct SessionMemory {
    // ボスの登場を、この Play でもう始めたか（やり直しでは、初めから組み上がって立っている）。
    bool bossIntroSeen = false;
};

// いまの Play の覚え書き。Play が始まっていない・終わった後は nullptr。
SessionMemory* CurrentSessionMemory() noexcept;

} // namespace Project_TD2_2
