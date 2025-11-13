/// \file Board.cpp
/// \brief Implémentation des règles de placement et de calcul sur la grille.

#include "Board.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace {
std::vector<std::pair<int, int>> defaultStartSlots(int size) {
    const int max = size - 2;
    const int mid = size / 2;
    return {
            {1, 1},
            {1, max},
            {max, 1},
            {max, max},
            {1, mid},
            {max, mid},
            {mid, 1},
            {mid, max},
            {mid, mid}
    };
}
}

Board::Board(int numPlayers) {
    size = (numPlayers <= 4) ? 20 : 30;
    grid = std::vector<std::vector<char>>(size, std::vector<char>(size, '.'));
    bonuses = std::vector<std::vector<LGBonus>>(size, std::vector<LGBonus>(size, LGBonus::None));
}

void Board::display() const {
    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            char cell = grid[r][c];
            if (cell == '.' && bonuses[r][c] != LGBonus::None) {
                cell = charForBonus(bonuses[r][c]);
            }
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

            if (grid[boardX][boardY] != '.' || bonuses[boardX][boardY] != LGBonus::None) {
                return false;
            }

            if (playerAlreadyOnBoard && !touchesOwnTerritory) {
                touchesOwnTerritory = isAdjacentToPlayerTerritory(boardX, boardY, playerSymbol);
                if (!touchesOwnTerritory) {
                    static constexpr int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                    for (auto dir : dirs) {
                        const int nx = boardX + dir[0];
                        const int ny = boardY + dir[1];
                        if (isInside(nx, ny, size) && grid[nx][ny] == '#' && ownsStoneAt(nx, ny, playerSymbol)) {
                            touchesOwnTerritory = true;
                            break;
                        }
                    }
                }
            }

            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    const int nx = boardX + dx;
                    const int ny = boardY + dy;
                    if (nx >= 0 && nx < size && ny >= 0 && ny < size) {
                        const bool neighborIsOwnStone = (grid[nx][ny] == '#' && ownsStoneAt(nx, ny, playerSymbol));
                        if (grid[nx][ny] != '.' && grid[nx][ny] != playerSymbol && !neighborIsOwnStone) {
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

bool Board::placeStone(int x, int y, char owner) {
    if (x < 0 || y < 0 || x >= size || y >= size) {
        return false;
    }

    if (grid[x][y] != '.' || bonuses[x][y] != LGBonus::None) {
        return false;
    }

    grid[x][y] = '#';
    stoneOwners[x * size + y] = owner;
    return true;
}

int Board::getSize() const {
    return size;
}

const std::vector<std::vector<char>>& Board::getGrid() const {
    return grid;
}

const std::vector<std::vector<LGBonus>>& Board::getBonuses() const {
    return bonuses;
}

const std::unordered_map<char, std::pair<int, int>>& Board::getStartCells() const {
    return startCells;
}

void Board::placeStartingPoints(const std::vector<char>& playerSymbols) {
    startCells.clear();
    auto slots = defaultStartSlots(size);
    std::size_t slotIndex = 0;
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(1, size - 2);

    for (char symbol : playerSymbols) {
        if (symbol == '\0') {
            continue;
        }

        std::pair<int, int> pos{-1, -1};
        while (slotIndex < slots.size()) {
            auto candidate = slots[slotIndex++];
            if (grid[candidate.first][candidate.second] == '.') {
                pos = candidate;
                break;
            }
        }

        while (pos.first < 0 || grid[pos.first][pos.second] != '.') {
            const int row = dist(rng);
            const int col = dist(rng);
            if (grid[row][col] == '.') {
                pos = {row, col};
            }
        }

        grid[pos.first][pos.second] = symbol;
        startCells[symbol] = pos;
    }
}

void Board::placeBonusSquares(int numPlayers) {
    bonuses.assign(size, std::vector<LGBonus>(size, LGBonus::None));

    std::vector<int> available;
    available.reserve(size * size);
    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            if (grid[r][c] == '.') {
                available.push_back(r * size + c);
            }
        }
    }

    if (available.empty()) {
        return;
    }

    std::mt19937 rng(std::random_device{}());
    std::shuffle(available.begin(), available.end(), rng);

    const int couponCount = static_cast<int>(std::ceil(numPlayers * 1.5));
    const int stoneCount = static_cast<int>(std::ceil(numPlayers * 0.5));
    const int robberyCount = numPlayers;

    size_t cursor = 0;
    auto placeType = [&](LGBonus bonusType, int count) {
        for (int i = 0; i < count && cursor < available.size(); ++i) {
            const int index = available[cursor++];
            const int row = index / size;
            const int col = index % size;
            bonuses[row][col] = bonusType;
        }
    };

    placeType(LGBonus::Coupon, couponCount);
    placeType(LGBonus::Stone, stoneCount);
    placeType(LGBonus::Robbery, robberyCount);
}

std::vector<Board::BonusClaim> Board::claimCompletedBonuses(char playerSymbol) {
    std::vector<BonusClaim> claims;
    static constexpr int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            LGBonus bonus = bonuses[r][c];
            if (bonus == LGBonus::None) {
                continue;
            }

            bool surrounded = true;
            for (auto dir : dirs) {
                const int nr = r + dir[0];
                const int nc = c + dir[1];
                if (!isInside(nr, nc, size) || grid[nr][nc] != playerSymbol) {
                    surrounded = false;
                    break;
                }
            }

            if (surrounded) {
                grid[r][c] = playerSymbol;
                claims.push_back(BonusClaim{bonus, r, c});
            }
        }
    }

    return claims;
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

char Board::charForBonus(LGBonus bonus) {
    switch (bonus) {
        case LGBonus::Coupon:
            return 'E';
        case LGBonus::Stone:
            return 'S';
        case LGBonus::Robbery:
            return 'R';
        default:
            return '.';
    }
}

bool Board::isInside(int x, int y, int limit) {
    return x >= 0 && x < limit && y >= 0 && y < limit;
}

void Board::clearCells(const std::vector<std::pair<int, int>>& cells) {
    for (const auto& cell : cells) {
        const int r = cell.first;
        const int c = cell.second;
        if (isInside(r, c, size)) {
            grid[r][c] = '.';
            stoneOwners.erase(r * size + c);
        }
    }
}

LGBonus Board::collectBonusAt(int row, int col, char playerSymbol) {
    if (!isInside(row, col, size)) {
        return LGBonus::None;
    }
    if (grid[row][col] != playerSymbol) {
        return LGBonus::None;
    }
    LGBonus bonus = bonuses[row][col];
    bonuses[row][col] = LGBonus::None;
    return bonus;
}

int Board::calculateLargestSquare(char playerSymbol) const {
    int maxSide = 0;
    std::vector<std::vector<int>> dp(size, std::vector<int>(size, 0));

    for (int r = 0; r < size; ++r) {
        for (int c = 0; c < size; ++c) {
            if (grid[r][c] == playerSymbol) {
                if (r == 0 || c == 0) {
                    dp[r][c] = 1;
                } else {
                    dp[r][c] = std::min({dp[r - 1][c], dp[r][c - 1], dp[r - 1][c - 1]}) + 1;
                }
                maxSide = std::max(maxSide, dp[r][c]);
            }
        }
    }

    return maxSide * maxSide;
}

bool Board::ownsStoneAt(int x, int y, char playerSymbol) const {
    const auto it = stoneOwners.find(x * size + y);
    return it != stoneOwners.end() && it->second == playerSymbol;
}
