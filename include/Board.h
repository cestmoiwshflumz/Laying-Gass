#ifndef BOARD_H
#define BOARD_H

#include <vector>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include "LGShared.h"

class Board {
public:
    struct BonusClaim {
        LGBonus type{LGBonus::None};
        int row{0};
        int col{0};
    };

    Board(int numPlayers);
    void display() const;

    bool placeTile(int x, int y, const std::vector<std::vector<int>>& tileShape, char playerSymbol);
    bool canPlaceTile(int x, int y, const std::vector<std::vector<int>>& tileShape, char playerSymbol) const;
    bool hasValidPlacement(const std::vector<std::vector<int>>& tileShape, char playerSymbol) const;
    bool placeStone(int x, int y, char owner);
    int getSize() const;
    const std::vector<std::vector<char>>& getGrid() const;
    const std::vector<std::vector<LGBonus>>& getBonuses() const;
    const std::unordered_map<char, std::pair<int, int>>& getStartCells() const;

    void placeStartingPoints(const std::vector<char>& playerSymbols);
    void placeBonusSquares(int numPlayers);
    std::vector<BonusClaim> claimCompletedBonuses(char playerSymbol);
    void clearCells(const std::vector<std::pair<int, int>>& cells);
    int calculateLargestSquare(char playerSymbol) const;
    LGBonus collectBonusAt(int row, int col, char playerSymbol);
    bool ownsStoneAt(int x, int y, char playerSymbol) const;

private:
    int size;
    std::vector<std::vector<char>> grid;
    std::vector<std::vector<LGBonus>> bonuses;
    std::unordered_map<char, std::pair<int, int>> startCells;
    std::unordered_map<int, char> stoneOwners;

    bool playerHasTiles(char playerSymbol) const;
    bool isAdjacentToPlayerTerritory(int x, int y, char playerSymbol) const;
    static char charForBonus(LGBonus bonus);
    static bool isInside(int x, int y, int size);
};

#endif // BOARD_H
