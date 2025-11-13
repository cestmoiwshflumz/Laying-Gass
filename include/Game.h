#ifndef GAME_H
#define GAME_H

#include "Board.h"
#include "Player.h"
#include "Tiles.h"
#include "RaylibRenderer.h"
#include "LGShared.h"
#include <memory>
#include <optional>
#include <random>
#include <vector>

class Game {
public:
    Game(int numPlayers, const std::string& tileFile);
    bool start();

private:
    Board board;
    Tiles tiles;
    std::vector<Player> players;
    std::mt19937 rng;
    std::unique_ptr<RaylibRenderer> renderer;
    std::vector<LGPlacedTile> placedTiles;

    void takeTurn(Player& player, Tile currentTile, const Tile& nextTile);
    void handleStonePlacement(Player& player, const Tile* currentTile, const Tile* nextTile);
    std::optional<Tile> handleStealOption(Player& player, const Tile& currentTile, const Tile& nextTile);
    Tile drawRandomTile();
    std::vector<Tile> drawTileOptions(int count);
    void processBonusClaims(Player& player, const std::vector<Board::BonusClaim>& claims);
    void recordPlacement(char owner, int tileId, const std::vector<std::vector<int>>& shape, int originX, int originY);
    Player* findPlayer(char symbol);
    std::vector<RaylibRenderer::FinalScoreEntry> buildFinalScores();
    static bool promptYesNo(const std::string& message);
    static void displayTileShape(const Tile& tile);
    static int countCoveredCells(const std::vector<std::vector<int>>& shape);
};

#endif // GAME_H
