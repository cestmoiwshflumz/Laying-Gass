#include "Game.h"
#include "RaylibRenderer.h"
#include <cctype>
#include <algorithm>
#include <iostream>
#include <limits>
#include <thread>
#include <chrono>
#include <unordered_set>

Game::Game(int numPlayers, const std::string& tileFile)
        : board(numPlayers), tiles(tileFile), rng(std::random_device{}()) {
    for (int i = 0; i < numPlayers; ++i) {
        const char symbol = static_cast<char>('A' + i);
        const std::string name = "Player " + std::to_string(i + 1);
        players.emplace_back(name, symbol);
    }
    renderer = std::make_unique<RaylibRenderer>(board.getSize());
    if (renderer) {
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
        while (!renderer->isReady() && std::chrono::steady_clock::now() < deadline) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
}

void Game::start() {
    if (players.empty()) {
        return;
    }

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

    if (renderer && renderer->isReady()) {
        renderer->updateState(board, players.back(), nullptr, nullptr);
        renderer->showGameOver(players);
    }

    std::cout << "Fin du jeu !" << std::endl;
    for (const auto& player : players) {
        std::cout << player.getName() << " - Score : " << player.getScore() << std::endl;
    }
}

void Game::takeTurn(Player& player, Tile currentTile, const Tile& nextTile) {
    if (skipTurnIfNeeded(player)) {
        return;
    }

    const bool guiAvailable = renderer && renderer->isReady();
    auto updateGui = [&](const Tile* activeTile) {
        if (guiAvailable) {
            renderer->updateState(board, player, activeTile, &nextTile);
        }
    };

    std::cout << player.getName() << " (" << player.getSymbol() << ") commence son tour." << std::endl;
    std::cout << "Ressources -> Coupons: " << player.getSwapCoupons()
              << ", Pierres: " << player.getStones()
              << ", Vols: " << player.getStealTokens() << std::endl;

    handleStealOption(player);
    handleStonePlacement(player, guiAvailable ? &currentTile : nullptr, guiAvailable ? &nextTile : nullptr);

    updateGui(&currentTile);

    if (!guiAvailable) {
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
            tilePlaced = true;
        }
    } else {
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
    }

    std::cout << player.getName() << " complete son coup." << std::endl;
    if (!guiAvailable) {
        board.display();
    }
    updateGui(nullptr);

    maybeAwardBonus(player);
    updateGui(nullptr);
}

bool Game::skipTurnIfNeeded(Player& player) {
    auto it = skippedTurns.find(player.getSymbol());
    if (it != skippedTurns.end() && it->second > 0) {
        --(it->second);
        std::cout << player.getName() << " perd ce tour suite a un vol." << std::endl;
        return true;
    }
    return false;
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

            if (board.placeStone(result.x, result.y)) {
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

        if (board.placeStone(x, y)) {
            player.useStone();
            std::cout << "Pierre placee en (" << x << ", " << y << ")." << std::endl;
            board.display();
        } else {
            std::cout << "Placement impossible (case occupee ou hors plateau)." << std::endl;
        }
    }
}

void Game::handleStealOption(Player& player) {
    if (!player.hasStealToken() || players.size() < 2) {
        return;
    }

    std::vector<Player*> targets;
    for (auto& candidate : players) {
        if (candidate.getSymbol() == player.getSymbol()) {
            continue;
        }
        targets.push_back(&candidate);
    }

    if (targets.empty()) {
        return;
    }

    const bool guiReady = renderer && renderer->isReady();
    if (guiReady) {
        const bool wantsSteal = renderer->confirmAction(
                "Bonus de vol",
                "Utiliser un bonus pour faire passer le prochain tour d'un adversaire ?",
                "Utiliser le vol",
                "Plus tard");
        if (!wantsSteal) {
            return;
        }

        std::vector<std::string> labels;
        labels.reserve(targets.size());
        for (const auto* target : targets) {
            labels.push_back(target->getName() + " (" + std::string(1, target->getSymbol()) + ")");
        }
        const int choice = renderer->selectFromList("Choisissez la cible", labels, "Annuler");
        if (choice < 0 || choice >= static_cast<int>(targets.size())) {
            return;
        }

        Player* target = targets[choice];
        if (player.useStealToken()) {
            ++skippedTurns[target->getSymbol()];
            std::cout << target->getName() << " perdra son prochain tour." << std::endl;
        }
        return;
    }

    if (!promptYesNo("Utiliser un bonus de vol pour faire perdre un tour a un adversaire ? (o/n) ")) {
        return;
    }

    std::cout << "Choisissez la cible :" << std::endl;
    for (size_t i = 0; i < targets.size(); ++i) {
        std::cout << (i + 1) << " - " << targets[i]->getName() << " (" << targets[i]->getSymbol() << ")" << std::endl;
    }

    size_t choice = 0;
    while (true) {
        std::cout << "Numero de joueur a voler : ";
        if (!(std::cin >> choice)) {
            std::cin.clear();
            std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
            std::cout << "Entree invalide." << std::endl;
            continue;
        }
        if (choice >= 1 && choice <= targets.size()) {
            break;
        }
        std::cout << "Choix hors limites." << std::endl;
    }

    Player* target = targets[choice - 1];
    if (player.useStealToken()) {
        ++skippedTurns[target->getSymbol()];
        std::cout << target->getName() << " perdra son prochain tour." << std::endl;
    }
}

void Game::maybeAwardBonus(Player& player) {
    std::uniform_int_distribution<int> chance(0, 99);
    if (chance(rng) >= 30) {
        return;
    }

    std::uniform_int_distribution<int> bonusDist(0, 2);
    switch (bonusDist(rng)) {
        case 0:
            player.addSwapCoupon();
            std::cout << player.getName() << " gagne un coupon d'echange." << std::endl;
            break;
        case 1:
            player.addStone();
            std::cout << player.getName() << " gagne une pierre de blocage." << std::endl;
            break;
        case 2:
            player.addStealToken();
            std::cout << player.getName() << " gagne un bonus de vol." << std::endl;
            break;
    }
}

Tile Game::drawRandomTile() {
    std::uniform_int_distribution<int> distribution(0, tiles.getTotalTiles() - 1);
    return tiles.getTile(distribution(rng));
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
