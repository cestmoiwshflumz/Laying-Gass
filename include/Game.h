/// \file Game.h
/// \brief Déclaration de la classe Game, orchestrateur principal du jeu.

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

/**
 * @brief Orchestrateur principal du jeu Laying Grass.
 *
 * La classe instancie la grille, charge les tuiles, gère la boucle des tours
 * et pilote le renderer Raylib pour l'affichage/les interactions.
 */
class Game {
public:
    /**
     * @brief Prépare une partie avec @p numPlayers et un fichier de tuiles donné.
     * @param numPlayers Nombre de joueurs humains (2 à 9).
     * @param tileFile   Fichier JSON décrivant les formes disponibles.
     */
    Game(int numPlayers, const std::string& tileFile);

    /*!
     * @brief Lance la boucle de jeu complète (9 tours) et retourne l'intention de redémarrage.
     *
     * Cette méthode s'occupe d'initialiser le renderer, d'enchaîner les tours,
     * puis de calculer/afficher les scores finaux.
     *
     * @return `true` si les joueurs souhaitent relancer une partie, `false` sinon.
     */
    bool start();

private:
    Board board;                                  //!< Grille logique partagée.
    Tiles tiles;                                  //!< Bibliothèque de formes.
    std::vector<Player> players;                  //!< Joueurs inscrits à la partie.
    std::mt19937 rng;                             //!< Générateur pseudo-aléatoire.
    std::unique_ptr<RaylibRenderer> renderer;     //!< Couche d'affichage interactive.
    std::vector<LGPlacedTile> placedTiles;        //!< Historique des tuiles posées pour la mécanique de vol.

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
