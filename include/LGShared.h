//
// Created by Antoine on 12/11/2025.
//

#ifndef LAYING_GAME_LGSHARED_H
#define LAYING_GAME_LGSHARED_H
#pragma once
#include <unordered_map>
#include <utility>
#include <vector>

/**
 * @brief Cases de départ par symbole (partagé entre Game et Board).
 */
inline std::unordered_map<char, std::pair<int,int>> LG_START_CELL;

/**
 * @brief Active l'obligation de toucher la zone déjà connectée au départ.
 */
inline bool LG_REQUIRE_TOUCH_TO_START = false;

/**
 * @brief Types de bonus pouvant apparaître sur la grille.
 */
enum class LGBonus { None, Coupon, Stone, Robbery };

/**
 * @brief Enregistrement minimal d'une tuile posée pour permettre un futur vol.
 */
struct LGPlacedTile {
    char owner = '?';                                        //!< Symbole du joueur.
    int shapeIndex = -1;                                     //!< Index original dans Tiles.
    std::vector<std::vector<int>> shape;                     //!< Matrice normalisée (utilisée lors d'un vol).
    std::vector<std::pair<int,int>> cells;                   //!< Coordonnées absolues couvertes sur la grille.
};

#endif //LAYING_GAME_LGSHARED_H
