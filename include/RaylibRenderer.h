/// \file RaylibRenderer.h
/// \brief Déclaration du renderer Raylib et de ses structures auxiliaires.

#ifndef RAYLIB_RENDERER_H
#define RAYLIB_RENDERER_H

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <string>
#include <thread>
#include <vector>
#include <raylib.h>
#include "Board.h"
#include "Player.h"
#include "Tiles.h"

/**
 * @brief Boucle de rendu Raylib indépendante de la logique métier.
 *
 * La classe tourne sur un thread dédié et expose une API synchrone pour
 * demander des placements de tuiles, de pierres ou recueillir des choix UI.
 */
class RaylibRenderer {
public:
    /**
     * @brief Résultat retourné après une phase de placement de tuile.
     */
    struct PlacementResult {
        bool success{false};                       //!< True si une tuile valide a été posée.
        bool cancelled{false};                     //!< True si le joueur a annulé la pose.
        bool swapRequested{false};                 //!< True si le joueur préfère échanger sa tuile.
        int x{0};                                  //!< Coordonnée X retenue.
        int y{0};                                  //!< Coordonnée Y retenue.
        std::vector<std::vector<int>> shape;       //!< Forme finale (après rotations éventuelles).
    };

    /**
     * @brief Résultat d'une phase de placement de pierre.
     */
    struct StoneResult {
        bool placed{false};
        int x{0};
        int y{0};
    };

    /**
     * @brief Structure utilisée lors de l'écran de fin pour récapituler les scores.
     */
    struct FinalScoreEntry {
        std::string name;
        char symbol{'A'};
        int territory{0};
        int largestSquare{0};
        bool winner{false};
    };

    /**
     * @brief Initialise la fenêtre Raylib et la boucle de rendu.
     * @param boardSize Taille du plateau (20 ou 30 cases).
     * @param cellSize  Taille des cellules en pixels (défaut: 24px).
     */
    explicit RaylibRenderer(int boardSize, int cellSize = 24);
    ~RaylibRenderer();

    /**
     * @brief Met à jour les instantanés consommés par le thread de rendu.
     */
    void updateState(const Board& board,
                     const Player& currentPlayer,
                     const Tile* currentTile,
                     const Tile* nextTile);

    /*!
     * @brief Lance un mode interactif permettant au joueur de poser une tuile.
     *
     * @param board          Grille actuelle (pour la prévisualisation).
     * @param currentPlayer  Joueur actif : sert à afficher les ressources/infos.
     * @param tile           Tuile courante (déjà tirée).
     * @param nextTile       Tuile à venir (peek) ou `nullptr`.
     * @param allowSwap      Indique si le bouton d'échange doit être affiché.
     *
     * @return Un `PlacementResult` qui décrit la décision du joueur (pose validée,
     *         annulation ou demande d'échange).
     */
    PlacementResult requestTilePlacement(const Board& board,
                                         const Player& currentPlayer,
                                         const Tile& tile,
                                         const Tile* nextTile,
                                         bool allowSwap);

    /**
     * @brief Démarre une phase de placement de pierre bloquante.
     */
    StoneResult requestStonePlacement(const Board& board,
                                      const Player& currentPlayer);

    /**
     * @brief Attend que le joueur demande une nouvelle partie depuis l'écran final.
     */
    bool waitForRestart();

    /**
     * @brief Affiche une boîte de dialogue booléenne.
     */
    bool confirmAction(const std::string& title,
                       const std::string& message,
                       const std::string& confirmLabel = "Oui",
                       const std::string& cancelLabel = "Non");

    /**
     * @brief Affiche une liste d'options textuelles et retourne l'index choisi.
     */
    int selectFromList(const std::string& title,
                       const std::vector<std::string>& options,
                       const std::string& cancelLabel = "Annuler");

    /**
     * @brief Variante spécialisée pour sélectionner une tuile à partir d'un pool.
     */
    int selectTile(const std::string& title,
                   const std::vector<Tile>& options,
                   const std::string& cancelLabel = "Annuler");

    /**
     * @brief Affiche l'écran de fin de partie avec le classement.
     */
    void showGameOver(const std::vector<FinalScoreEntry>& scores,
                      const std::string& victoryRule);

    /**
     * @brief Indique si la fenêtre est prête à recevoir des événements (après init).
     */
    bool isReady() const;

    /**
     * @brief Retourne true tant que le thread de rendu tourne.
     */
    bool isRunning() const;

private:
    enum class Mode {
        Idle,
        Placement,
        Stone
    };

    enum class DialogKind {
        None,
        Confirm,
        List,
        TilePicker
    };

    struct PlayerSnapshot {
        std::string name;
        char symbol{'A'};
        int score{0};
        int coupons{0};
        int stones{0};
        int steals{0};
    };

    struct PlacementState {
        bool active{false};
        bool resultReady{false};
        bool allowSwap{false};
        bool resultSwap{false};
        bool resultSuccess{false};
        bool resultCancelled{false};
        int resultX{0};
        int resultY{0};
        std::vector<std::vector<int>> workingShape;
        std::vector<std::vector<int>> resultShape;
    };

    struct StoneState {
        bool active{false};
        bool resultReady{false};
        bool resultPlaced{false};
        int resultX{0};
        int resultY{0};
    };

    struct TilePreview {
        int id{0};
        std::vector<std::vector<int>> shape;
    };

    struct DialogState {
        bool active{false};
        DialogKind kind{DialogKind::None};
        bool completed{false};
        bool boolResult{false};
        int choice{-1};
        std::string title;
        std::string message;
        std::string confirmLabel{"Oui"};
        std::string cancelLabel{"Non"};
        std::vector<std::string> options;
        std::vector<TilePreview> tiles;
    };

    struct DialogRenderResult {
        bool completed{false};
        bool boolValue{false};
        int indexValue{-1};
    };

    void renderLoop();
    void drawIdle(const std::vector<std::vector<char>>& grid,
                  const PlayerSnapshot& player,
                  const std::vector<std::vector<int>>& currentTile,
                  const std::vector<std::vector<int>>& nextTile,
                  const std::vector<std::vector<LGBonus>>& bonuses) const;
    void drawPlacement(const std::vector<std::vector<char>>& grid,
                       const PlayerSnapshot& player,
                       std::vector<std::vector<int>> tileShape,
                       const std::vector<std::vector<int>>& nextTilePreview,
                       bool allowSwap,
                       const std::vector<std::vector<LGBonus>>& bonuses);
    void drawStonePlacement(const std::vector<std::vector<char>>& grid,
                            const PlayerSnapshot& player,
                            bool allowSkip,
                            const std::vector<std::vector<LGBonus>>& bonuses);
    bool drawGameOverOverlay(const std::vector<FinalScoreEntry>& scores,
                             const std::string& ruleText,
                             bool allowRestart) const;
    Rectangle boardArea() const;
    bool canPlaceLocally(int x, int y, const std::vector<std::vector<int>>& tileShape, char playerSymbol, const std::vector<std::vector<char>>& grid) const;
    bool canPlaceStone(int x, int y, const std::vector<std::vector<char>>& grid) const;
    DialogRenderResult drawDialogOverlay(const DialogState& dialog,
                                         const Vector2& mouse,
                                         bool mouseClick,
                                         bool escapePressed) const;
    void fulfillDialogResult(const DialogRenderResult& result, DialogKind kind);
    Color colorForSymbol(char symbol) const;
    Color colorForBonus(LGBonus bonus) const;
    void drawCellBase(float x, float y, char cellValue) const;
    void drawBonusIcon(float x, float y, LGBonus bonus) const;
    void loadTextures();
    void unloadTextures();
    void signalRestart();
    void drawTilePreview(const std::vector<std::vector<int>>& tileShape, int originX, int originY) const;
    static std::vector<std::vector<int>> rotateClockwise(const std::vector<std::vector<int>>& shape);
    static std::vector<std::vector<int>> rotateCounterClockwise(const std::vector<std::vector<int>>& shape);
    static std::vector<std::vector<int>> flipHorizontal(const std::vector<std::vector<int>>& shape);

    const int boardSize;
    const int cellSize;
    const int sidebarWidth;
    const int margin;

    std::atomic<bool> running;
    std::atomic<bool> windowReady;
    std::thread renderThread;
    mutable std::mutex stateMutex;
    std::condition_variable placementCv;
    std::condition_variable dialogCv;
    std::condition_variable restartCv;

    Mode mode;
    std::vector<std::vector<char>> gridSnapshot;
    std::vector<std::vector<LGBonus>> bonusSnapshot;
    PlayerSnapshot playerSnapshot;
    std::vector<std::vector<int>> currentTileSnapshot;
    std::vector<std::vector<int>> nextTileSnapshot;
    bool hasCurrentTile;
    bool hasNextTile;
    PlacementState placement;
    StoneState stone;
    DialogState dialog;
    bool gameOverActive{false};
    bool restartButtonActive{false};
    bool restartPressed{false};
    std::string victoryRuleText;
    std::vector<FinalScoreEntry> finalScores;
    std::string feedbackMessage;
    float feedbackTimer;
    Texture2D grassTexture{};
    Texture2D stoneTexture{};
    Texture2D bonusCouponTexture{};
    Texture2D bonusStoneTexture{};
    Texture2D bonusRobberyTexture{};
    bool texturesLoaded{false};
};

#endif // RAYLIB_RENDERER_H
