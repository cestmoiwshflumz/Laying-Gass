#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#define NOGDI
#define NOUSER
#include <windows.h>
#include <string>
#include "Game.h"

int main() {
    SetConsoleOutputCP(CP_UTF8);

    const int numPlayers = 2;
    const std::string tilePath = "../data/tiles.json";
    bool restartRequested = false;
    do {
        Game game(numPlayers, tilePath);
        restartRequested = game.start();
    } while (restartRequested);

    return 0;
}
