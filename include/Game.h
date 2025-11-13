#ifndef GAME_H
#define GAME_H

#include "Board.h"
#include "Player.h"
#include "Tiles.h"
#include "RaylibRenderer.h"
#include <memory>
#include <random>
#include <unordered_map>
#include <vector>

class Game {
public:
    Game(int numPlayers, const std::string& tileFile);
    void start();

private:
    Board board;
    Tiles tiles;
    std::vector<Player> players;
    std::mt19937 rng;
    std::unordered_map<char, int> skippedTurns;
    std::unique_ptr<RaylibRenderer> renderer;

    void takeTurn(Player& player, Tile currentTile, const Tile& nextTile);
    bool skipTurnIfNeeded(Player& player);
    void handleStonePlacement(Player& player, const Tile* currentTile, const Tile* nextTile);
    void handleStealOption(Player& player);
    void maybeAwardBonus(Player& player);
    Tile drawRandomTile();
    std::vector<Tile> drawTileOptions(int count);
    static bool promptYesNo(const std::string& message);
    static void displayTileShape(const Tile& tile);
    static int countCoveredCells(const std::vector<std::vector<int>>& shape);
};

#endif // GAME_H
