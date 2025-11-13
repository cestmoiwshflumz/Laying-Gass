/// \file Board.h
/// \brief Interface de la grille et des bonus associés.

#ifndef BOARD_H
#define BOARD_H

#include <vector>
#include <iostream>
#include <unordered_map>
#include <utility>
#include <algorithm>
#include "LGShared.h"

/**
 * @brief Représente la grille carrée sur laquelle les joueurs placent leurs tuiles.
 *
 * La classe s'occupe de vérifier les collisions, l'adjacence aux territoires,
 * la collecte des bonus et le calcul des zones finales.
 */
class Board {
public:
    /**
     * @brief Événement retourné lorsque des cases bonus ont été complétées.
     */
    struct BonusClaim {
        LGBonus type{LGBonus::None};  //!< Type de bonus obtenu.
        int row{0};                   //!< Ligne de la case déclenchée.
        int col{0};                   //!< Colonne de la case déclenchée.
    };

    /**
     * @brief Construit une grille adaptée au nombre de joueurs (20x20 ou 30x30).
     */
    Board(int numPlayers);

    /**
     * @brief Affiche la grille en mode texte (debug/console).
     */
    void display() const;

    /*!
     * @brief Pose une tuile sur la grille après vérification locale.
     *
     * Cette méthode doit être appelée après `canPlaceTile`. Elle parcourt la matrice
     * `tileShape` et copie le symbole du joueur dans chacune des cases cibles.
     *
     * @param x Colonne (origine) où placer le coin supérieur gauche de la tuile.
     * @param y Ligne (origine) où placer le coin supérieur gauche de la tuile.
     * @param tileShape Matrice binaire représentant la forme normalisée.
     * @param playerSymbol Symbole ASCII du joueur propriétaire.
     *
     * @return `true` si toutes les cellules ont été posées, `false` en cas d'échec (collision).
     */
    bool placeTile(int x, int y, const std::vector<std::vector<int>>& tileShape, char playerSymbol);

    /*!
     * @brief Vérifie hors mutation si un motif peut être placé à une position donnée.
     *
     * @param x Colonne (origine) testée.
     * @param y Ligne (origine) testée.
     * @param tileShape Matrice de la tuile après rotation éventuelle.
     * @param playerSymbol Symbole du joueur qui tente la pose.
     *
     * @return `true` si toutes les cases restent dans la grille, ne chevauchent pas un adversaire
     *         et respectent l'adjacence requise.
     */
    bool canPlaceTile(int x, int y, const std::vector<std::vector<int>>& tileShape, char playerSymbol) const;

    /**
     * @brief Teste l'existence d'au moins une position valide pour la tuile.
     */
    bool hasValidPlacement(const std::vector<std::vector<int>>& tileShape, char playerSymbol) const;

    /**
     * @brief Place une pierre bloquante appartenant à @p owner.
     */
    bool placeStone(int x, int y, char owner);

    /**
     * @brief Taille actuelle de la grille (généralement 20 ou 30).
     */
    int getSize() const;

    /**
     * @brief Retourne une vue sur la grille des symboles.
     */
    const std::vector<std::vector<char>>& getGrid() const;

    /**
     * @brief Retourne la couche des bonus (même dimensions que la grille).
     */
    const std::vector<std::vector<LGBonus>>& getBonuses() const;

    /**
     * @brief Coordonnées des cases de départ par symbole de joueur.
     */
    const std::unordered_map<char, std::pair<int, int>>& getStartCells() const;

    /**
     * @brief Place les cellules de départ (phase initiale).
     */
    void placeStartingPoints(const std::vector<char>& playerSymbols);

    /**
     * @brief Répartit les bonus selon le nombre de joueurs.
     */
    void placeBonusSquares(int numPlayers);

    /*!
     * @brief Retourne les bonus complétés par @p playerSymbol pour le tour en cours.
     *
     * @param playerSymbol Symbole du joueur dont les cases viennent d'être posées.
     * @return Liste des positions et types de bonus à traiter par `Game`.
     */
    std::vector<BonusClaim> claimCompletedBonuses(char playerSymbol);

    /**
     * @brief Vide explicitement certaines cellules (utilisé lors d'un vol).
     */
    void clearCells(const std::vector<std::pair<int, int>>& cells);

    /**
     * @brief Calcule la plus grande zone carrée continue contrôlée par le joueur.
     */
    int calculateLargestSquare(char playerSymbol) const;

    /**
     * @brief Récupère puis annule un bonus présent en (row, col).
     */
    LGBonus collectBonusAt(int row, int col, char playerSymbol);

    /**
     * @brief Indique si la pierre en (x, y) appartient bien au joueur.
     */
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
