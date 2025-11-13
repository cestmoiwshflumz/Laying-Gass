/// \file Game.cpp
/// \brief Implémentation de la logique de partie : tours, bonus et scoring.

#include "Game.h"
#include "RaylibRenderer.h"
#include <cctype>
#include <algorithm>
#include <iostream>
#include <limits>
#include <thread>
#include <chrono>
#include <unordered_set>
#include <optional>
#include <cmath>

namespace {
int squareSideFromArea(int area) {
    if (area <= 0) {
        return -1;
    }
    const int side = static_cast<int>(std::sqrt(static_cast<double>(area)) + 0.5);
    return (side > 0 && side * side == area) ? side : -1;
}
}

Game::Game(int numPlayers, const std::string& tileFile)
        : board(numPlayers), tiles(tileFile), rng(std::random_device{}()) {
    for (int i = 0; i < numPlayers; ++i) {
        const char symbol = static_cast<char>('A' + i);
        const std::string name = "Player " + std::to_string(i + 1);
        players.emplace_back(name, symbol);
    }

    std::vector<char> symbols;
    symbols.reserve(players.size());
    for (const auto& player : players) {
        symbols.push_back(player.getSymbol());
    }
    board.placeStartingPoints(symbols);
    board.placeBonusSquares(numPlayers);

    renderer = std::make_unique<RaylibRenderer>(board.getSize());
    if (renderer) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!renderer->isReady() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

bool Game::start() {
    if (players.empty()) {
        return false;
    }

    placedTiles.clear();

    std::cout << "Demarrage du jeu !" << std::endl;
    if (!(renderer && renderer->isReady())) {
        std::cout << "Plateau initial :" << std::endl;
        board.display();
    }

    Tile currentTile = drawRandomTile();
    Tile nextTile = drawRandomTile();

    if (renderer && renderer->isReady()) {
        renderer->updateState(board, players.front(), &currentTile, &nextTile);
    }

    const int totalRounds = 9;
    for (int round = 0; round < totalRounds; ++round) {
        std::cout << "=== Tour " << (round + 1) << " ===" << std::endl;
        for (size_t index = 0; index < players.size(); ++index) {
            Player& player = players[index];
            takeTurn(player, currentTile, nextTile);
            if (renderer && !renderer->isRunning()) {
                return false;
            }

            const bool hasMoreTurns = !(round == totalRounds - 1 && index == players.size() - 1);
            if (hasMoreTurns) {
                currentTile = nextTile;
                nextTile = drawRandomTile();

                if (renderer && renderer->isReady()) {
                    Player& nextPlayer = players[(index + 1) % players.size()];
                    renderer->updateState(board, nextPlayer, &currentTile, &nextTile);
                }
            }
        }
    }
    if (renderer && !renderer->isRunning()) {
        return false;
    }

    const std::string victoryRule =
            "Victoire : plus grand carre continu occupe. Egalite departagee par le territoire total.";
    auto finalScores = buildFinalScores();

    std::cout << "Fin du jeu !" << std::endl;
    for (const auto& entry : finalScores) {
        const int squareSide = squareSideFromArea(entry.largestSquare);
        const std::string squareText = squareSide > 0
                ? std::to_string(squareSide) + "x" + std::to_string(squareSide)
                : std::to_string(entry.largestSquare) + " cases";
        std::cout << entry.name << " (" << entry.symbol << ") - Territoire : "
                  << entry.territory << " cases, Plus grand carre : "
                  << squareText
                  << (entry.winner ? " <-- Vainqueur" : "") << std::endl;
    }

    bool restartRequested = false;
    if (renderer && renderer->isReady()) {
        renderer->updateState(board, players.back(), nullptr, nullptr);
        renderer->showGameOver(finalScores, victoryRule);
        restartRequested = renderer->waitForRestart();
    }

    return restartRequested;
}

void Game::takeTurn(Player& player, Tile currentTile, const Tile& nextTile) {
    if (renderer && !renderer->isRunning()) {
        return;
    }

    const bool guiAvailable = renderer && renderer->isReady();
    auto updateGui = [&](const Tile* activeTile) {
        if (renderer && renderer->isReady()) {
            renderer->updateState(board, player, activeTile, &nextTile);
        }
    };

    std::cout << player.getName() << " (" << player.getSymbol() << ") commence son tour." << std::endl;
    std::cout << "Ressources -> Coupons: " << player.getSwapCoupons()
              << ", Pierres: " << player.getStones()
              << ", Vols: " << player.getStealTokens() << std::endl;

    auto stolenTile = handleStealOption(player, currentTile, nextTile);
    if (stolenTile.has_value()) {
        currentTile = *stolenTile;
        updateGui(&currentTile);
    }
    handleStonePlacement(player, guiAvailable ? &currentTile : nullptr, guiAvailable ? &nextTile : nullptr);
    if (renderer && !renderer->isRunning()) {
        return;
    }

    updateGui(&currentTile);

    if (!guiAvailable) {
        if (renderer) {
            return;
        }
        std::cout << "Forme de la tuile :" << std::endl;
        displayTileShape(currentTile);
    }

    if (!board.hasValidPlacement(currentTile.shape, player.getSymbol())) {
        std::cout << "Aucun placement possible. Tour passe." << std::endl;
        updateGui(nullptr);
        return;
    }

    bool tilePlaced = false;
    if (guiAvailable) {
        while (!tilePlaced) {
            auto placement = renderer->requestTilePlacement(board, player, currentTile, &nextTile, player.hasSwapCoupon());

            if (placement.swapRequested) {
                if (!player.hasSwapCoupon()) {
                    std::cout << "Aucun coupon disponible pour " << player.getName() << "." << std::endl;
                    continue;
                }

                bool exchanged = false;
                if (guiAvailable) {
                    auto options = drawTileOptions(5);
                    const int choice = renderer->selectTile("Choisissez une nouvelle tuile", options, "Annuler");
                    if (choice >= 0 && choice < static_cast<int>(options.size())) {
                        player.useSwapCoupon();
                        currentTile = options[choice];
                        exchanged = true;
                        std::cout << player.getName() << " choisit la tuile #" << currentTile.id << " avec un coupon." << std::endl;
                    } else {
                        std::cout << player.getName() << " annule l'echange de tuile." << std::endl;
                        continue;
                    }
                } else {
                    player.useSwapCoupon();
                    currentTile = drawRandomTile();
                    exchanged = true;
                    std::cout << player.getName() << " utilise un coupon pour echanger sa tuile." << std::endl;
                }

                if (exchanged) {
                    if (!board.hasValidPlacement(currentTile.shape, player.getSymbol())) {
                        std::cout << "Aucun placement possible apres l'echange. Tour passe." << std::endl;
                        updateGui(nullptr);
                        return;
                    }
                    updateGui(&currentTile);
                    continue;
                }
            }

            if (!placement.success) {
                std::cout << player.getName() << " annule son placement. Tour passe." << std::endl;
                updateGui(nullptr);
                return;
            }

            if (!board.canPlaceTile(placement.x, placement.y, placement.shape, player.getSymbol())) {
                std::cout << "Placement invalide detecte. Reessayez." << std::endl;
                continue;
            }

            board.placeTile(placement.x, placement.y, placement.shape, player.getSymbol());
            player.incrementScore(countCoveredCells(placement.shape));
            recordPlacement(player.getSymbol(), currentTile.id, placement.shape, placement.x, placement.y);
            auto claims = board.claimCompletedBonuses(player.getSymbol());
            if (!claims.empty()) {
                processBonusClaims(player, claims);
                updateGui(nullptr);
            }
            tilePlaced = true;
        }
    } else {
        if (renderer) {
            return;
        }
        int x = 0;
        int y = 0;
        bool validPlacement = false;
        while (!validPlacement) {
            std::cout << "Entrez les coordonnees pour placer la tuile (x y) : ";
            if (!(std::cin >> x >> y)) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Entree invalide. Merci de fournir deux entiers." << std::endl;
                continue;
            }

            if (board.canPlaceTile(x, y, currentTile.shape, player.getSymbol())) {
                validPlacement = true;
            } else {
                std::cout << "Placement impossible: la tuile doit rester dans la grille sans toucher les adversaires et relier votre territoire." << std::endl;
            }
        }

        board.placeTile(x, y, currentTile.shape, player.getSymbol());
        player.incrementScore(countCoveredCells(currentTile.shape));
        recordPlacement(player.getSymbol(), currentTile.id, currentTile.shape, x, y);
        auto claims = board.claimCompletedBonuses(player.getSymbol());
        processBonusClaims(player, claims);
    }

    std::cout << player.getName() << " complete son coup." << std::endl;
    if (!guiAvailable) {
        board.display();
    }
    updateGui(nullptr);

    updateGui(nullptr);
}

void Game::handleStonePlacement(Player& player, const Tile* currentTile, const Tile* nextTile) {
    while (player.hasStone()) {
        const bool guiReady = renderer && renderer->isReady();

        if (guiReady) {
            const bool wantsStone = renderer->confirmAction(
                    "Pierre disponible",
                    "Utiliser une pierre pour bloquer une case avant la pose de tuile ?",
                    "Placer la pierre",
                    "Plus tard");
            if (!wantsStone) {
                break;
            }

            auto result = renderer->requestStonePlacement(board, player);
            if (!result.placed) {
                break;
            }

            if (board.placeStone(result.x, result.y, player.getSymbol())) {
                player.useStone();
                renderer->updateState(board, player, currentTile, nextTile);
                std::cout << "Pierre placee en (" << result.x << ", " << result.y << ")." << std::endl;
            } else {
                std::cout << "Position invalide. Reessayez." << std::endl;
            }
            continue;
        }

        if (!promptYesNo("Souhaitez-vous placer une pierre avant votre tuile ? (o/n) ")) {
            break;
        }

        int x = 0;
        int y = 0;
        std::cout << "Coordonnees de la pierre (x y) : ";
        if (!(std::cin >> x >> y)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Entree invalide. Pierre non placee." << std::endl;
            continue;
        }

        if (board.placeStone(x, y, player.getSymbol())) {
            player.useStone();
            std::cout << "Pierre placee en (" << x << ", " << y << ")." << std::endl;
            board.display();
        } else {
            std::cout << "Placement impossible (case occupee ou hors plateau)." << std::endl;
        }
    }
}

std::optional<Tile> Game::handleStealOption(Player& player, const Tile& currentTile, const Tile& nextTile) {
    if (!player.hasStealToken()) {
        return std::nullopt;
    }

    std::vector<size_t> candidates;
    candidates.reserve(placedTiles.size());
    for (size_t i = 0; i < placedTiles.size(); ++i) {
        if (placedTiles[i].owner != player.getSymbol() && !placedTiles[i].cells.empty()) {
            candidates.push_back(i);
        }
    }

    if (candidates.empty()) {
        return std::nullopt;
    }

    const bool guiReady = renderer && renderer->isReady();
    bool wantsSteal = false;
    if (guiReady) {
        wantsSteal = renderer->confirmAction(
                "Bonus de vol",
                "Volez une tuile deja posee pour la rejouer a votre tour.",
                "Voler une tuile",
                "Plus tard");
    } else {
        wantsSteal = promptYesNo("Utiliser un bonus de vol pour voler une tuile posee ? (o/n) ");
    }

    if (!wantsSteal) {
        return std::nullopt;
    }

    int selection = -1;
    if (guiReady) {
        std::vector<std::string> labels;
        labels.reserve(candidates.size());
        for (size_t idx : candidates) {
            const auto& placement = placedTiles[idx];
            labels.push_back(
                    std::string(1, placement.owner) + " - "
                    + std::to_string(placement.cells.size()) + " cases");
        }
        selection = renderer->selectFromList("Choisissez une tuile a voler", labels, "Annuler");
        if (selection < 0 || selection >= static_cast<int>(candidates.size())) {
            return std::nullopt;
        }
    } else {
        std::cout << "Tuiles disponibles :" << std::endl;
        for (size_t i = 0; i < candidates.size(); ++i) {
            const auto& placement = placedTiles[candidates[i]];
            std::cout << (i + 1) << " - Joueur " << placement.owner
                      << " (" << placement.cells.size() << " cases)" << std::endl;
        }
        size_t input = 0;
        while (true) {
            std::cout << "Numero de la tuile a voler : ";
            if (!(std::cin >> input)) {
                std::cin.clear();
                std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
                std::cout << "Entree invalide." << std::endl;
                continue;
            }
            if (input >= 1 && input <= candidates.size()) {
                selection = static_cast<int>(input - 1);
                break;
            }
            std::cout << "Choix hors limites." << std::endl;
        }
    }

    if (selection < 0) {
        return std::nullopt;
    }

    const size_t recordIndex = candidates[selection];
    LGPlacedTile placement = placedTiles[recordIndex];

    if (!player.useStealToken()) {
        return std::nullopt;
    }

    board.clearCells(placement.cells);

    if (Player* victim = findPlayer(placement.owner)) {
        victim->incrementScore(-static_cast<int>(placement.cells.size()));
        std::cout << victim->getName() << " perd " << placement.cells.size()
                  << " points, une tuile a ete volee." << std::endl;
    }

    placedTiles.erase(placedTiles.begin() + recordIndex);

    Tile stolen;
    stolen.id = placement.shapeIndex;
    stolen.shape = placement.shape;

    if (renderer && renderer->isReady()) {
        renderer->updateState(board, player, &stolen, &nextTile);
    }

    std::cout << player.getName() << " vole une tuile et peut la rejouer immediatement." << std::endl;
    return stolen;
}

Tile Game::drawRandomTile() {
    std::uniform_int_distribution<int> distribution(0, tiles.getTotalTiles() - 1);
    return tiles.getTile(distribution(rng));
}

void Game::recordPlacement(char owner, int tileId, const std::vector<std::vector<int>>& shape, int originX, int originY) {
    if (shape.empty()) {
        return;
    }
    LGPlacedTile record;
    record.owner = owner;
    record.shapeIndex = tileId;
    record.shape = shape;
    for (int i = 0; i < static_cast<int>(shape.size()); ++i) {
        for (int j = 0; j < static_cast<int>(shape[i].size()); ++j) {
            if (shape[i][j] == 1) {
                record.cells.emplace_back(originX + i, originY + j);
            }
        }
    }
    placedTiles.push_back(std::move(record));
}

Player* Game::findPlayer(char symbol) {
    for (auto& p : players) {
        if (p.getSymbol() == symbol) {
            return &p;
        }
    }
    return nullptr;
}

std::vector<Tile> Game::drawTileOptions(int count) {
    std::vector<Tile> options;
    if (tiles.getTotalTiles() <= 0 || count <= 0) {
        return options;
    }

    const int available = tiles.getTotalTiles();
    options.reserve(std::min(count, available));
    std::unordered_set<int> used;
    std::uniform_int_distribution<int> distribution(0, available - 1);

    while (static_cast<int>(options.size()) < count && static_cast<int>(used.size()) < available) {
        const int index = distribution(rng);
        if (used.insert(index).second) {
            options.push_back(tiles.getTile(index));
        }
    }

    return options;
}

void Game::processBonusClaims(Player& player, const std::vector<Board::BonusClaim>& claims) {
    if (claims.empty()) {
        return;
    }

    for (const auto& claim : claims) {
        LGBonus granted = board.collectBonusAt(claim.row, claim.col, player.getSymbol());
        if (granted == LGBonus::None) {
            granted = claim.type;
        }
        switch (granted) {
            case LGBonus::Coupon:
                player.addSwapCoupon();
                std::cout << player.getName() << " capture une case echange en (" << claim.row << ", "
                          << claim.col << ") et gagne un coupon." << std::endl;
                break;
            case LGBonus::Stone:
                player.addStone();
                std::cout << player.getName() << " capture une case pierre en (" << claim.row << ", "
                          << claim.col << ") et gagne une pierre." << std::endl;
                break;
            case LGBonus::Robbery:
                player.addStealToken();
                std::cout << player.getName() << " capture une case vol en (" << claim.row << ", "
                          << claim.col << ") et gagne un bonus de vol." << std::endl;
                break;
            default:
                break;
        }
    }
}

std::vector<RaylibRenderer::FinalScoreEntry> Game::buildFinalScores() {
    std::vector<RaylibRenderer::FinalScoreEntry> scores;
    scores.reserve(players.size());

    int bestSquare = -1;
    int bestTerritory = -1;
    for (const auto& player : players) {
        RaylibRenderer::FinalScoreEntry entry;
        entry.name = player.getName();
        entry.symbol = player.getSymbol();
        entry.territory = player.getScore();
        entry.largestSquare = board.calculateLargestSquare(player.getSymbol());
        scores.push_back(entry);

        if (entry.largestSquare > bestSquare ||
            (entry.largestSquare == bestSquare && entry.territory > bestTerritory)) {
            bestSquare = entry.largestSquare;
            bestTerritory = entry.territory;
        }
    }

    for (auto& entry : scores) {
        entry.winner = (entry.largestSquare == bestSquare && entry.territory == bestTerritory);
    }

    std::sort(scores.begin(), scores.end(), [](const auto& lhs, const auto& rhs) {
        if (lhs.largestSquare != rhs.largestSquare) {
            return lhs.largestSquare > rhs.largestSquare;
        }
        if (lhs.territory != rhs.territory) {
            return lhs.territory > rhs.territory;
        }
        return lhs.name < rhs.name;
    });

    return scores;
}

bool Game::promptYesNo(const std::string& message) {
    char response = 'n';
    std::cout << message;
    std::cin >> response;
    return std::tolower(static_cast<unsigned char>(response)) == 'o';
}

void Game::displayTileShape(const Tile& tile) {
    for (const auto& row : tile.shape) {
        for (int cell : row) {
            std::cout << (cell == 1 ? '#' : ' ');
        }
        std::cout << std::endl;
    }
}

int Game::countCoveredCells(const std::vector<std::vector<int>>& shape) {
    int total = 0;
    for (const auto& row : shape) {
        for (int cell : row) {
            if (cell == 1) {
                ++total;
            }
        }
    }
    return total;
}
