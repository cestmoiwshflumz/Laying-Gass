# Laying Grass

Ce projet est inspiré du jeu **Laying Grass** tiré de l'émission *The Devil's Plan* sur Netflix (épisode 8). Le but du jeu est d’étendre son territoire en plaçant des tuiles d'herbe sur une grille pour créer la plus grande zone carrée possible.

## Règles du Jeu

1. **Nombre de joueurs** : 2 à 9 joueurs
    - Grille 20x20 pour 2-4 joueurs
    - Grille 30x30 pour 5-9 joueurs

2. **But du Jeu** :
    - Le joueur ayant la plus grande zone carrée d’herbe à la fin des 9 tours remporte la partie.
    - En cas d’égalité, le joueur ayant le plus grand nombre de cases couvertes gagne.

3. **Déroulement de la partie** :
    - Chaque joueur reçoit une tuile de départ 1x1.
    - Chaque joueur commence avec un coupon d’échange de tuile.
    - Au début de chaque tour, le joueur reçoit une tuile d'herbe aléatoire parmi 96 formes possibles.

4. **Placement des Tuiles** :
    - Les tuiles doivent être placées adjacentes aux autres tuiles du joueur.
    - Elles ne doivent pas chevaucher les tuiles des autres joueurs.
    - Les tuiles peuvent être tournées ou retournées.

5. **Bonus** :
    - **Échange de tuile** : Donne un coupon pour échanger de tuile.
    - **Pierre** : Bloque une case et empêche d’y placer d’autres tuiles.
    - **Vol de tuile** : Permet de voler une tuile d’un autre joueur.

## Prérequis

- [CMake](https://cmake.org/) pour gérer la construction du projet
- Compilateur C++ compatible avec la norme C++17 ou supérieure

## Structure du Projet
```plaintext
.
├── src
│   ├── Game.cpp             # Logique principale du jeu
│   ├── Player.cpp           # Gestion des joueurs
│   ├── Tile.cpp             # Gestion des tuiles
│   └── Board.cpp            # Gestion de la grille
├── include
│   ├── Game.h               # Header de la classe Game
│   ├── Player.h             # Header de la classe Player
│   ├── Tile.h               # Header de la classe Tile
│   └── Board.h              # Header de la classe Board
├── data
│   └── tiles.json           # Fichier JSON contenant les tuiles
├── main.cpp                 # Point d'entrée du programme
├── LICENSE                  # license MIT
├── README.md                # Ce fichier
└── CMakeLists.txt           # Fichier de configuration CMake
```

## Compilation et Exécution

1. **Cloner le dépôt** :
   ```bash
   git clone https://github.com/cestmoiwshflumz/Laying-Game.git
   cd Laying_Game
    ```
2. **Construire le projet** :

    ```bash
    mkdir build
    cd build
    cmake ..
    make
    ```

3.  **Exécuter le jeu** :

    ```bash
    ./laying-grass
    ```

## Fonctionnalités Implémentées :
- **Initialisation des joueurs :** Choix de nom et couleur pour chaque joueur.
- **Distribution et placement des tuiles :** Système de rotation et retournement des tuiles.
- **Gestion des bonus :** Systèmes de bonus pour échange de tuile, blocage de cases avec des pierres, et vol de tuiles.
- **Calcul du score final :** Calcul de la plus grande zone carrée à la fin du jeu.

## Guide d'Utilisation :
Le jeu se joue dans le terminal. À chaque tour, les joueurs peuvent :
- Placer leur tuile selon les règles de proximité avec leur territoire.
- Utiliser des commandes pour pivoter ou retourner les tuiles.
- Utiliser un coupon pour échanger une tuile.

Les informations de jeu (tuiles disponibles, grille de jeu, etc.) sont affichées en ligne de commande.

## Documentation Technique (Doxygen)

1. **Installer Doxygen** : disponible sur [doxygen.nl](https://www.doxygen.nl/) ou via votre gestionnaire de paquets.
2. **Générer la documentation** :
   ```bash
   doxygen docs/Doxyfile
   ```
3. **Consulter la sortie** :
   - HTML : `docs/build/html/index.html`
   - XML (pour intégrations CI/CD) : `docs/build/xml`

Le fichier `docs/technical_overview.md` sert de page principale et décrit l'architecture (modules, flux de données, invariants). Ajoutez vos commentaires Doxygen directement dans les en-têtes `include/*.h` pour enrichir cette base lors des prochaines générations.

