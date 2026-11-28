#include "HIKARI/Game.h"

// モジュールの入口は 1 行。部品・システムは HIKARI_COMPONENT / HIKARI_SYSTEM が、DLL を読んだ時に集めて登録する
// （型名は TD2_2.*、機能の ID は TD2_2.Game）。ゲームの 1 固定 tick と 1 フレームの中の並びは GameLoop.cpp。
HIKARI_GAME("TD2_2")
