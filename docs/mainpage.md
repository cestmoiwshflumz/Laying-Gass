# Laying Grass – Documentation Technique

Bienvenue sur la documentation Doxygen du projet **Laying Grass**. Ce jeu de plateau numérique (C++17 + Raylib) propose de reproduire les mécaniques du mini-jeu introduit dans *The Devil's Plan*.  

## Architecture

| Module | Fichiers | Rôle |
|--------|----------|------|
| `Game` | `include/Game.h`, `src/Game.cpp` | Gère l'initialisation des joueurs, la boucle des tours, la distribution des tuiles et la collecte des bonus. |
| `Board` | `include/Board.h`, `src/Board.cpp` | Maintient la grille, vérifie les collisions, calcule les plus grands carrés et attribue les bonus. |
| `Tiles` | `include/Tiles.h`, `src/Tiles.cpp`, `data/tiles.json` | Charge les 96 formes disponibles et fournit rotation/retournement. |
| `Player` | `include/Player.h`, `src/Player.cpp` | Stocke le score, les coupons d'échange, les pierres et les jetons de vol. |
| `RaylibRenderer` | `include/RaylibRenderer.h`, `src/RaylibRenderer.cpp` | Offre l'interface graphique, gère les interactions souris/clavier et l'affichage des états de jeu. |
| `LGShared` | `include/LGShared.h` | Contient les structures partagées (`LGBonus`, `LGPlacedTile`, coordonnées de départ). |

## Génération de la documentation

```bash
doxygen docs/Doxyfile
```

La sortie HTML est placée dans `doc/html/index.html`.

## Contribuer

- Ajoutez des commentaires Doxygen aux signatures publiques (`@brief`, `@param`, `@return`).
- Documentez les invariants importants (ex: adjacency rules) directement dans les classes/structures.
- Mettez à jour cette page si de nouveaux modules apparaissent (IA, multijoueur en ligne, etc.).
