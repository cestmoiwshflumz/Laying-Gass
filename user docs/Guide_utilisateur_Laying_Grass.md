# Guide utilisateur - Laying Grass

## 1. Apercu rapide
- Jeu strategique inspire de Laying Grass (The Devil's Plan). Objectif: construire la plus grande zone carree apres 9 tours.
- 2 a 9 joueurs humains se partagent un meme plateau (20x20 jusqu'a 4 joueurs, 30x30 au-dela).
- Chaque joueur controle une couleur/symbole (A-I) et pose des tuiles d'herbe tirees depuis `data/tiles.json` (96 formes).
- Les bonus (coupon, pierre, vol) s'obtiennent en entourant des cases speciales et offrent des actions supplementaires.
- L'interface principale est assuree par Raylib (fenetre interactive); un mode texte minimal prend le relais si Raylib ne peut pas demarrer.

## 2. Prerequis techniques
- CMake 3.20+ et un compilateur C++17 (MSVC, Clang ou GCC).
- Raylib est fourni dans le depot (via `src/RaylibRenderer.cpp`). Sous Windows, veillez a disposer des DLL necessaires si vous reinstallez Raylib.
- CMake cherche les dependances dans le dossier du projet (nlohmann/json vendori se dans `nlohmann/`).
- L'executable s'attend a trouver `../data/tiles.json` et les textures `../ressources/*.jpg`. Lancez donc le binaire depuis `build/` (ou adaptez les chemins dans le code).

## 3. Installation et lancement
### 3.1 Recuperer le code
```bash
git clone https://github.com/cestmoiwshflumz/Laying-Game.git
cd Laying_Game
```

### 3.2 Compiler
```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```
Sous Windows, vous pouvez aussi ouvrir le dossier dans Visual Studio et utiliser la generation CMake integree.

### 3.3 Choisir le nombre de joueurs
`main.cpp` contient `const int numPlayers = 2;`. Modifiez cette valeur (2 a 9) puis recompilez pour changer automatiquement la taille du plateau et le nombre de symboles distribues. Les noms visibles dans l'interface suivent le format `Player N`; personnalisez-les dans le constructeur de `Game` si besoin.

### 3.4 Lancer une partie
- Placez-vous dans `build/`.
- Sur Windows: `.\Release\laying-grass.exe` (ou `.\laying-grass.exe` selon la configuration).
- Sur Linux/macOS: `./laying-grass`.
- La fenetre Raylib doit s'ouvrir avec le plateau initial. Si elle se ferme immediatement, consultez la console pour voir si un message signale un probleme de ressource.

## 4. Interface graphique
### 4.1 Plateau
- Zone carree a gauche, quadrillee. Chaque case affiche soit une texture d'herbe (territoire), soit un pave gris (vide), soit une pierre (`#`). Les bonus sont representes par une icone coupon/pierre/masque.
- Un surlignage translucide apparait quand vous deplacez la souris: vert si la tuile courante peut etre posee, rouge sinon.

### 4.2 Panneau d'information (colonne droite)
- Rappels joueur: nom, symbole, score (nombre total de cases controlees).
- Ressources disponibles: `Coupons`, `Pierres`, `Vols`.
- Apercu de la tuile courante (haut) et de la tuile suivante (bas) sous forme de mini-grilles.
- Bouton `Echanger la tuile` (190 px de large) actif uniquement si vous disposez d'au moins un coupon.

### 4.3 Dialogues contextuels
- Boites de confirmation (fond gris fonce) pour utiliser un bonus (pierre, vol, echange). Cliquez sur les boutons verts/rouges ou appuyez sur `Echap` pour annuler.
- Listes de selection (vol de tuile) et galerie de tuiles (echange) affichent les options cliquables; un bouton `Annuler` est toujours disponible.
- En cas d'erreur (placement invalide, pierre interdite), un message rouge temporaire s'affiche sous le plateau.

## 5. Deroulement d'un tour
### 5.1 Sequence generale
1. Optionnel: utiliser un jeton de vol si disponible.
2. Optionnel: placer une pierre avant la tuile.
3. Obligation: poser la tuile tiree (ou la remplacer via coupon). Si aucun placement n'est possible, le tour est automatiquement passe.
Chaque manche comporte 9 tours complets, et tous les joueurs jouent dans l'ordre A -> I a chaque manche.

### 5.2 Bonus de vol
- Une fenetre `Bonus de vol` apparait quand vous possedez un jeton. Choisissez `Voler une tuile` ou `Plus tard`.
- Si vous acceptez, une liste affiche chaque tuile eligible sous la forme `Symbole - N cases`. Cliquer sur une ligne retire immediatement la tuile du plateau adverse, reduit son score du meme nombre de cases, et rend la tuile volee disponible comme tuile courante.
- La tuile volee conserve sa forme telle qu'elle etait posee (elle n'est pas reinitialisee). Vous devez donc la replacer telle quelle, mais vous pouvez toujours la tourner ou la retourner en miroir avant de jouer.

### 5.3 Placement d'une pierre
- Une pierre bloque definitivement une case libre; seul son proprietaire peut ensuite s'y accoler pour continuer son territoire.
- Quand vous avez au moins une pierre, un pop-up propose `Placer la pierre`. Si vous acceptez, l'ecran passe en mode pierre:
  - Clique gauche sur une case vide pour poser.
  - Bouton `Passer` pour conserver la pierre pour plus tard.
  - Les cases rouges indiquent un blocage (case deja occupee, bonus, bord).
- Chaque pierre consommee reduit votre compteur de pierres.

### 5.4 Placement de la tuile
- Si Raylib est actif, utilisez la souris sur le plateau. Surbrillance verte = position valide; rouge = collision ou regle violee.
- Laisser la souris sur une cellule du plateau pour voir l'ombre de la tuile.
- Cliquez pour poser une fois satisfait. Le score augmente immediatement du nombre de cases couvrees.
- Sans interface graphique (mode console), les coordonnees `(x y)` vous sont demandees dans le terminal.

### 5.5 Commandes pendant le placement
- Molette: rotation (sens horaire/antihoraire selon le sens de defilement).
- Clic droit: miroir horizontal.
- Clic gauche: pose sur la case survolee (si valide).
- Bouton `Echanger la tuile`: ouvre la galerie d'options si un coupon est disponible.
- `Echap` dans une boite de dialogue annule l'action en cours.

### 5.6 Echange de tuile (coupons)
- Chaque coupon permet de piocher jusqu'a 5 nouvelles tuiles candidates (selectionnees aleatoirement et affichees avec leur ID).
- Cliquer sur une carte consomme le coupon et remplace votre tuile courante. Cliquer sur `Annuler` ferme la galerie sans depenser de ressource.
- Si la nouvelle tuile n'a aucune position valide, le tour est passe automatiquement et la tuile suivante devient la tuile active du joueur suivant.

### 5.7 Aucun placement possible
- Le jeu teste automatiquement toutes les positions (`Board::hasValidPlacement`). Si aucune n'est possible (murs adverses, plateau plein, bonus bloquants), le tour se termine immediatement.
- L'interface se met a jour pour informer le joueur suivant.

## 6. Bonus et ressources
- **Coupon (icone ticket violet, lettre `E` en mode console)**: autorise un echange de tuile. Ressource cumulable.
- **Pierre (icone rocher, lettre `S`)**: bloque une case vide. Seul le proprietaire peut passer a travers en utilisant l'adjacence a sa pierre.
- **Vol (icone masque, lettre `R`)**: permet de voler une tuile deja posee.
- Distribution initiale: ~1,5 coupon, 0,5 pierre et 1 vol par joueur, places aleatoirement sur le plateau.
- Capture d'un bonus: vous ne pouvez pas couvrir la case bonus directement. Il faut entourer orthogonalement (haut/bas/gauche/droite) la case cible avec vos propres cellules. Une fois les quatre voisins occupes, la case bonus devient a vous, votre ressource est creditee et la case se convertit automatiquement en herbe.

## 7. Regles de placement detaillees
- Les tuiles doivent rester entierement dans la grille.
- Elles ne peuvent jamais recouvrir une case deja occupee (joueur adverse, pierre, bonus).
- Elles doivent etre adjacentes (orthogonalement) a votre territoire existant. Si vous avez deja une pierre a vous (`#`), elle peut servir de point de contact.
- Aucun carre de la tuile ne doit toucher, meme en diagonale, un carre appartenant a un adversaire ou a une pierre adverse.
- Les bonus ne forment pas des obstacles permanents mais servent de cases a entourer.
- Les tuiles volees conservent les cellules exactes qu'elles occupaient; quand vous les rejouez, elles doivent respecter les memes contraintes.

## 8. Fin de partie et redemarrage
- Apres 9 tours complets, une fenetre de resultat affiche: classement, territoire total et plus grand carre de chaque joueur (converti en NxN si c'est un carre parfait).
- Condition de victoire: plus grande zone carree. En cas d'egalite, le total de cases controlees departage.
- Bouton `Rejouer` dans l'ecran de fin relance immediatement une partie avec le meme nombre de joueurs. Sinon, fermez la fenetre pour quitter.
- Les resultats sont egalement traces en console (pratique si vous jouez en mode texte).

## 9. Mode console de secours
- Si Raylib echoue a s'initialiser (GPU incompatible, machine distante), le jeu bascule automatiquement en saisie console.
- Les interactions se font alors via des invites texte:
  - Visualisation des tuiles en ASCII.
  - Entrees `(x y)` pour les placements.
  - Questions `o/n` pour les bonus.
- Le moteur applique exactement les memes regles que dans la version graphique.

## 10. Conseils pratiques
- Verifiez votre repertoire de travail avant de lancer l'executable; s'il ne trouve pas `../ressources/*.jpg`, le plateau utilisera un fond gris mais restera jouable.
- Utilisez vos pierres pour creer des passerelles: une pierre bien placee vous permet de contourner un mur adverse sans violer la regle de contact.
- Gardez un coupon pour les derniers tours: il devient crucial si votre tuile finale est impossible a placer.
- Les cases bonus sont de petites cibles; pensez a les encercler progressivement plutot qu'a tenter de les isoler d'un coup (les bonus sont revendiques uniquement quand les quatre cotes sont pris).
- Fermer la fenetre Raylib au milieu d'une partie provoque l'arret immediat de la boucle de jeu (aucune sauvegarde). Utilisez le bouton `Rejouer` si vous souhaitez enchainer.
