#include "Tiles.h"
#include <fstream>

using json = nlohmann::json;

// Constructeur qui charge les tuiles depuis un fichier JSON
Tiles::Tiles(const std::string& filename) {
    loadTiles(filename);
}

// Charge les tuiles depuis le fichier JSON
void Tiles::loadTiles(const std::string& filename) {
    std::cout << "Tentative d'ouverture du fichier : " << filename << std::endl;
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Erreur : Impossible d'ouvrir le fichier " << filename << std::endl;
        exit(1);
    }

    json j;
    file >> j;

    // Parcourt chaque élément dans le JSON et charge les tuiles
    for (const auto& item : j) {
        Tile tile;
        tile.id = item["id"].get<int>();
        tile.shape = item["shape"].get<std::vector<std::vector<int>>>();
        tiles.push_back(tile);
    }
}

// Renvoie la tuile à l'index donné
const Tile& Tiles::getTile(int index) const {
    if (index < 0 || index >= tiles.size()) {
        std::cerr << "Erreur : Index de tuile invalide" << std::endl;
        exit(1);
    }
    return tiles[index];
}

// Renvoie le nombre total de tuiles
int Tiles::getTotalTiles() const {
    return tiles.size();
}
