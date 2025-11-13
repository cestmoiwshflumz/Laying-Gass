#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#define NOUSER
#include <windows.h>
#include "Game.h"

int main() {
    SetConsoleOutputCP(CP_UTF8);

    Game game(2, "../data/tiles.json");
    game.start();

    return 0;
}
