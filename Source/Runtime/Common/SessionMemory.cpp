#include "SessionMemory.h"

#include "HIKARI/Game.h"

namespace Project_TD2_2 {
namespace {

SessionMemory*& Current() noexcept {
    static SessionMemory* current = nullptr;
    return current;
}

} // namespace

// Play の始まりに 1 つだけ作られ、シーンを変えても壊れず、Play の終わりで壊れる（旧 IGameProjectSession の持ち物）。
// 作られた時から使えるよう、持ち主の登録は構造子で行う。
struct SessionState : HIKARI::System {
    HIKARI_SYSTEM(SessionState)
    HIKARI_PERSIST

    SessionState() { Current() = &memory; }
    ~SessionState() override {
        if (Current() == &memory)
            Current() = nullptr;
    }

    SessionMemory memory;
};

SessionMemory* CurrentSessionMemory() noexcept { return Current(); }

} // namespace Project_TD2_2
