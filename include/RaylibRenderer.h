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

class RaylibRenderer {
public:
    struct PlacementResult {
        bool success{false};
        bool cancelled{false};
        bool swapRequested{false};
        int x{0};
        int y{0};
        std::vector<std::vector<int>> shape;
    };

    struct StoneResult {
        bool placed{false};
        int x{0};
        int y{0};
    };

    struct FinalScoreEntry {
        std::string name;
        char symbol{'A'};
        int score{0};
    };

    explicit RaylibRenderer(int boardSize, int cellSize = 24);
    ~RaylibRenderer();

    void updateState(const Board& board,
                     const Player& currentPlayer,
                     const Tile* currentTile,
                     const Tile* nextTile);

    PlacementResult requestTilePlacement(const Board& board,
                                         const Player& currentPlayer,
                                         const Tile& tile,
                                         const Tile* nextTile,
                                         bool allowSwap);

    StoneResult requestStonePlacement(const Board& board,
                                      const Player& currentPlayer);

    bool confirmAction(const std::string& title,
                       const std::string& message,
                       const std::string& confirmLabel = "Oui",
                       const std::string& cancelLabel = "Non");

    int selectFromList(const std::string& title,
                       const std::vector<std::string>& options,
                       const std::string& cancelLabel = "Annuler");

    int selectTile(const std::string& title,
                   const std::vector<Tile>& options,
                   const std::string& cancelLabel = "Annuler");

    void showGameOver(const std::vector<Player>& players);

    bool isReady() const;

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
                  const std::vector<std::vector<int>>& nextTile) const;
    void drawPlacement(const std::vector<std::vector<char>>& grid,
                       const PlayerSnapshot& player,
                       std::vector<std::vector<int>> tileShape,
                       const std::vector<std::vector<int>>& nextTilePreview,
                       bool allowSwap);
    void drawStonePlacement(const std::vector<std::vector<char>>& grid,
                            const PlayerSnapshot& player,
                            bool allowSkip);
    void drawGameOverOverlay(const std::vector<FinalScoreEntry>& scores) const;
    Rectangle boardArea() const;
    bool canPlaceLocally(int x, int y, const std::vector<std::vector<int>>& tileShape, char playerSymbol, const std::vector<std::vector<char>>& grid) const;
    bool canPlaceStone(int x, int y, const std::vector<std::vector<char>>& grid) const;
    DialogRenderResult drawDialogOverlay(const DialogState& dialog,
                                         const Vector2& mouse,
                                         bool mouseClick,
                                         bool escapePressed) const;
    void fulfillDialogResult(const DialogRenderResult& result, DialogKind kind);
    Color colorForSymbol(char symbol) const;
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

    Mode mode;
    std::vector<std::vector<char>> gridSnapshot;
    PlayerSnapshot playerSnapshot;
    std::vector<std::vector<int>> currentTileSnapshot;
    std::vector<std::vector<int>> nextTileSnapshot;
    bool hasCurrentTile;
    bool hasNextTile;
    PlacementState placement;
    StoneState stone;
    DialogState dialog;
    bool gameOverActive{false};
    std::vector<FinalScoreEntry> finalScores;
    std::string feedbackMessage;
    float feedbackTimer;
};

#endif // RAYLIB_RENDERER_H
