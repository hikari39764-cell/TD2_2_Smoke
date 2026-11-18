#pragma once

// Play の間、シーンの読み直し（やり直し）をまたいで覚えておくこと。World と一緒に消えるシステム・部品には置けないので、
// プロジェクトの Play session（IGameProjectSession の実装）が持つ。読む側は world.GetGameSession() を
// ISessionMemory へ dynamic_cast して取る（Play の外では取れない）。
namespace Project_TD2_2 {

struct SessionMemory {
    // ボスの登場を、この Play でもう始めたか（やり直しでは、初めから組み上がって立っている）。
    bool bossIntroSeen = false;
};

class ISessionMemory {
public:
    virtual ~ISessionMemory() = default;
    virtual SessionMemory& GetMemory() noexcept = 0;
};

} // namespace Project_TD2_2
