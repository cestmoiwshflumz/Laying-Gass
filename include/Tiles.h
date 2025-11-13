#ifndef TILES_H
#define TILES_H

#include <vector>
#include <string>
#include <iostream>
#include <nlohmann/json.hpp>

/**
 * @brief Description d'une forme disponible dans le jeu.
 */
struct Tile {
    int id{0};                                //!< Identifiant unique fourni par le JSON.
    std::vector<std::vector<int>> shape;      //!< Matrice binaire normalisée (1 = case couverte).
};

/**
 * @brief Charge et expose l'ensemble des tuiles définies dans data/tiles.json.
 */
class Tiles {
public:
    /**
     * @brief Charge les tuiles contenues dans @p filename (fichier JSON).
     */
    explicit Tiles(const std::string& filename);

    /**
     * @brief Accède à une tuile par index (souvent l'ID tiré aléatoirement).
     */
    [[nodiscard]] const Tile& getTile(int index) const;

    /**
     * @brief Nombre total de tuiles disponibles.
     */
    int getTotalTiles() const;

private:
    std::vector<Tile> tiles;  //!< Stockage interne des formes.

    /**
     * @brief Parse et normalise le fichier JSON pour remplir @p tiles.
     */
    void loadTiles(const std::string& filename);
};

#endif // TILES_H
