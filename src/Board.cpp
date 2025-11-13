#include "Board.h"

Board::Board(int numPlayers) {
    size = (numPlayers <= 4) ? 20 : 30;
    grid = std::vector<std::vector<char>>(size, std::vector<char>(size, '.'));
}

void Board::display() const {
    for (const auto& row : grid) {
        for (char cell : row) {
            std::cout << cell << ' ';
        }
        std::cout << std::endl;
    }
    std::cout << std::endl;
}

bool Board::canPlaceTile(int x, int y, const std::vector<std::vector<int>>& tileShape, char playerSymbol) const {
    const int tileHeight = static_cast<int>(tileShape.size());
    const int tileWidth = static_cast<int>(tileShape[0].size());

    if (x < 0 || y < 0 || x + tileHeight > size || y + tileWidth > size) {
        return false;
    }

    const bool playerAlreadyOnBoard = playerHasTiles(playerSymbol);
    bool touchesOwnTerritory = !playerAlreadyOnBoard;

    for (int i = 0; i < tileHeight; ++i) {
        for (int j = 0; j < tileWidth; ++j) {
            if (tileShape[i][j] != 1) {
                continue;
            }

            const int boardX = x + i;
            const int boardY = y + j;

            if (grid[boardX][boardY] != '.') {
                return false;
            }

            if (playerAlreadyOnBoard && !touchesOwnTerritory) {
                touchesOwnTerritory = isAdjacentToPlayerTerritory(boardX, boardY, playerSymbol);
            }

            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    const int nx = boardX + dx;
                    const int ny = boardY + dy;
                    if (nx >= 0 && nx < size && ny >= 0 && ny < size) {
                        if (grid[nx][ny] != '.' && grid[nx][ny] != playerSymbol) {
                            return false;
                        }
                    }
                }
            }
        }
    }

    if (playerAlreadyOnBoard && !touchesOwnTerritory) {
        return false;
    }

    return true;
}

bool Board::placeTile(int x, int y, const std::vector<std::vector<int>>& tileShape, char playerSymbol) {
    if (!canPlaceTile(x, y, tileShape, playerSymbol)) {
        return false;
    }

    const int tileHeight = static_cast<int>(tileShape.size());
    const int tileWidth = static_cast<int>(tileShape[0].size());

    for (int i = 0; i < tileHeight; ++i) {
        for (int j = 0; j < tileWidth; ++j) {
            if (tileShape[i][j] == 1) {
                grid[x + i][y + j] = playerSymbol;
            }
        }
    }
    return true;
}

bool Board::playerHasTiles(char playerSymbol) const {
    for (const auto& row : grid) {
        for (char cell : row) {
            if (cell == playerSymbol) {
                return true;
            }
        }
    }
    return false;
}

bool Board::hasValidPlacement(const std::vector<std::vector<int>>& tileShape, char playerSymbol) const {
    for (int x = 0; x < size; ++x) {
        for (int y = 0; y < size; ++y) {
            if (canPlaceTile(x, y, tileShape, playerSymbol)) {
                return true;
            }
        }
    }
    return false;
}

bool Board::placeStone(int x, int y) {
    if (x < 0 || y < 0 || x >= size || y >= size) {
        return false;
    }

    if (grid[x][y] != '.') {
        return false;
    }

    grid[x][y] = '#';
    return true;
}

int Board::getSize() const {
    return size;
}

const std::vector<std::vector<char>>& Board::getGrid() const {
    return grid;
}

bool Board::isAdjacentToPlayerTerritory(int x, int y, char playerSymbol) const {
    static constexpr int directions[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
    for (const auto& dir : directions) {
        const int nx = x + dir[0];
        const int ny = y + dir[1];
        if (nx >= 0 && nx < size && ny >= 0 && ny < size) {
            if (grid[nx][ny] == playerSymbol) {
                return true;
            }
        }
    }
    return false;
}
