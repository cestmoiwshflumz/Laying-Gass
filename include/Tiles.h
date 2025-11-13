#ifndef TILES_H
#define TILES_H

#include <vector>
#include <string>
#include <iostream>
#include <nlohmann/json.hpp> // Inclure la bibliothÃ¨que JSON

struct Tile {
    int id;
    std::vector<std::vector<int>> shape;  // Forme de la tuile (matrice)
};

class Tiles {
public:
    Tiles(const std::string& filename);  // Constructeur pour charger les tuiles depuis un fichier JSON
    [[nodiscard]] const Tile& getTile(int index) const; // AccÃ¨de Ã  une tuile par index
    int getTotalTiles() const;            // Renvoie le nombre total de tuiles

private:
    std::vector<Tile> tiles;  // Collection de toutes les tuiles
    void loadTiles(const std::string& filename);  // MÃ©thode pour charger les tuiles depuis JSON
};

#endif // TILES_H


