#ifndef BOARD_H
#define BOARD_H

#include <vector>
#include <iostream>

class Board {
public:
    Board(int numPlayers);
    void display() const;

    bool placeTile(int x, int y, const std::vector<std::vector<int>>& tileShape, char playerSymbol);
    bool canPlaceTile(int x, int y, const std::vector<std::vector<int>>& tileShape, char playerSymbol) const;
    bool hasValidPlacement(const std::vector<std::vector<int>>& tileShape, char playerSymbol) const;
    bool placeStone(int x, int y);
    int getSize() const;
    const std::vector<std::vector<char>>& getGrid() const;

private:
    int size;
    std::vector<std::vector<char>> grid;

    bool playerHasTiles(char playerSymbol) const;
    bool isAdjacentToPlayerTerritory(int x, int y, char playerSymbol) const;
};

#endif // BOARD_H
