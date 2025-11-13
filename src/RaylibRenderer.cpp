#include "RaylibRenderer.h"
#include <algorithm>
#include <cmath>

namespace {
constexpr const char* WINDOW_TITLE = "Laying Grass - Interface";
constexpr const char* WINDOW_ICON_PATH = "../ressources/icon.png";

const char* bonusLabel(LGBonus bonus) {
    switch (bonus) {
        case LGBonus::Coupon:
            return "E";
        case LGBonus::Stone:
            return "S";
        case LGBonus::Robbery:
            return "R";
        default:
            return "";
    }
}

int squareSideFromArea(int area) {
    if (area <= 0) {
        return -1;
    }
    const int side = static_cast<int>(std::sqrt(static_cast<double>(area)) + 0.5);
    return (side > 0 && side * side == area) ? side : -1;
}
}

RaylibRenderer::RaylibRenderer(int boardSize, int cellSize)
        : boardSize(boardSize),
          cellSize(cellSize),
          sidebarWidth(240),
          margin(20),
          running(true),
          windowReady(false),
          mode(Mode::Idle),
          hasCurrentTile(false),
          hasNextTile(false),
          feedbackTimer(0.0f) {
    gridSnapshot.assign(boardSize, std::vector<char>(boardSize, '.'));
    bonusSnapshot.assign(boardSize, std::vector<LGBonus>(boardSize, LGBonus::None));
    renderThread = std::thread(&RaylibRenderer::renderLoop, this);
}

RaylibRenderer::~RaylibRenderer() {
    running = false;
    placementCv.notify_all();
    dialogCv.notify_all();
    restartCv.notify_all();
    if (renderThread.joinable()) {
        renderThread.join();
    }
}

bool RaylibRenderer::isReady() const {
    return windowReady.load();
}

bool RaylibRenderer::isRunning() const {
    return running.load();
}

void RaylibRenderer::updateState(const Board& board,
                                 const Player& currentPlayer,
                                 const Tile* currentTile,
                                 const Tile* nextTile) {
    std::lock_guard<std::mutex> lock(stateMutex);
    gridSnapshot = board.getGrid();
    bonusSnapshot = board.getBonuses();
    playerSnapshot.name = currentPlayer.getName();
    playerSnapshot.symbol = currentPlayer.getSymbol();
    playerSnapshot.score = currentPlayer.getScore();
    playerSnapshot.coupons = currentPlayer.getSwapCoupons();
    playerSnapshot.stones = currentPlayer.getStones();
    playerSnapshot.steals = currentPlayer.getStealTokens();

    if (currentTile) {
        currentTileSnapshot = currentTile->shape;
        hasCurrentTile = true;
    } else {
        currentTileSnapshot.clear();
        hasCurrentTile = false;
    }

    if (nextTile) {
        nextTileSnapshot = nextTile->shape;
        hasNextTile = true;
    } else {
        nextTileSnapshot.clear();
        hasNextTile = false;
    }
}

RaylibRenderer::PlacementResult RaylibRenderer::requestTilePlacement(const Board& board,
                                                                     const Player& currentPlayer,
                                                                     const Tile& tile,
                                                                     const Tile* nextTile,
                                                                     bool allowSwap) {
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        gridSnapshot = board.getGrid();
        bonusSnapshot = board.getBonuses();
        playerSnapshot.name = currentPlayer.getName();
        playerSnapshot.symbol = currentPlayer.getSymbol();
        playerSnapshot.score = currentPlayer.getScore();
        playerSnapshot.coupons = currentPlayer.getSwapCoupons();
        playerSnapshot.stones = currentPlayer.getStones();
        playerSnapshot.steals = currentPlayer.getStealTokens();

        currentTileSnapshot = tile.shape;
        hasCurrentTile = true;

        if (nextTile) {
            nextTileSnapshot = nextTile->shape;
            hasNextTile = true;
        } else {
            nextTileSnapshot.clear();
            hasNextTile = false;
        }

        placement.active = true;
        placement.allowSwap = allowSwap;
        placement.resultReady = false;
        placement.resultSwap = false;
        placement.resultSuccess = false;
        placement.resultCancelled = false;
        placement.resultShape.clear();
        placement.resultX = 0;
        placement.resultY = 0;
        placement.workingShape = tile.shape;
        stone.active = false;
        mode = Mode::Placement;
        feedbackMessage.clear();
        feedbackTimer = 0.0f;
    }

    std::unique_lock<std::mutex> lock(stateMutex);
    placementCv.wait(lock, [&]() { return !running.load() || placement.resultReady; });

    PlacementResult result;
    if (!placement.resultReady) {
        result.cancelled = true;
    } else if (placement.resultSwap) {
        result.swapRequested = true;
    } else {
        result.success = placement.resultSuccess;
        result.cancelled = placement.resultCancelled;
        result.x = placement.resultX;
        result.y = placement.resultY;
        result.shape = placement.resultShape;
    }

    placement.active = false;
    placement.resultReady = false;
    mode = Mode::Idle;
    return result;
}

RaylibRenderer::StoneResult RaylibRenderer::requestStonePlacement(const Board& board,
                                                                  const Player& currentPlayer) {
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        gridSnapshot = board.getGrid();
        bonusSnapshot = board.getBonuses();
        playerSnapshot.name = currentPlayer.getName();
        playerSnapshot.symbol = currentPlayer.getSymbol();
        playerSnapshot.score = currentPlayer.getScore();
        playerSnapshot.coupons = currentPlayer.getSwapCoupons();
        playerSnapshot.stones = currentPlayer.getStones();
        playerSnapshot.steals = currentPlayer.getStealTokens();

        stone.active = true;
        stone.resultReady = false;
        stone.resultPlaced = false;
        placement.active = false;
        mode = Mode::Stone;
        feedbackMessage.clear();
        feedbackTimer = 0.0f;
    }

    std::unique_lock<std::mutex> lock(stateMutex);
    placementCv.wait(lock, [&]() { return !running.load() || stone.resultReady; });

    StoneResult result;
    result.placed = stone.resultPlaced;
    result.x = stone.resultX;
    result.y = stone.resultY;

    stone.active = false;
    stone.resultReady = false;
    mode = Mode::Idle;
    return result;
}

bool RaylibRenderer::waitForRestart() {
    if (!windowReady.load()) {
        return false;
    }

    std::unique_lock<std::mutex> lock(stateMutex);
    restartCv.wait(lock, [&]() { return !running.load() || restartPressed; });
    return restartPressed;
}

bool RaylibRenderer::confirmAction(const std::string& title,
                                   const std::string& message,
                                   const std::string& confirmLabel,
                                   const std::string& cancelLabel) {
    if (!windowReady.load()) {
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        dialog = {};
        dialog.active = true;
        dialog.kind = DialogKind::Confirm;
        dialog.title = title;
        dialog.message = message;
        dialog.confirmLabel = confirmLabel;
        dialog.cancelLabel = cancelLabel;
        dialog.completed = false;
    }

    std::unique_lock<std::mutex> lock(stateMutex);
    dialogCv.wait(lock, [&]() { return !running.load() || dialog.completed; });

    if (!dialog.completed) {
        dialog.active = false;
        return false;
    }

    const bool value = dialog.boolResult;
    dialog.completed = false;
    dialog.kind = DialogKind::None;
    dialog.options.clear();
    dialog.tiles.clear();
    return value;
}

int RaylibRenderer::selectFromList(const std::string& title,
                                   const std::vector<std::string>& options,
                                   const std::string& cancelLabel) {
    if (!windowReady.load() || options.empty()) {
        return -1;
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        dialog = {};
        dialog.active = true;
        dialog.kind = DialogKind::List;
        dialog.title = title;
        dialog.options = options;
        dialog.cancelLabel = cancelLabel;
        dialog.completed = false;
    }

    std::unique_lock<std::mutex> lock(stateMutex);
    dialogCv.wait(lock, [&]() { return !running.load() || dialog.completed; });

    if (!dialog.completed) {
        dialog.active = false;
        return -1;
    }

    const int choice = dialog.choice;
    dialog.completed = false;
    dialog.kind = DialogKind::None;
    dialog.options.clear();
    dialog.tiles.clear();
    return choice;
}

int RaylibRenderer::selectTile(const std::string& title,
                               const std::vector<Tile>& options,
                               const std::string& cancelLabel) {
    if (!windowReady.load() || options.empty()) {
        return -1;
    }

    {
        std::lock_guard<std::mutex> lock(stateMutex);
        dialog = {};
        dialog.active = true;
        dialog.kind = DialogKind::TilePicker;
        dialog.title = title;
        dialog.cancelLabel = cancelLabel;
        dialog.tiles.clear();
        dialog.tiles.reserve(options.size());
        for (const auto& option : options) {
            dialog.tiles.push_back(TilePreview{option.id, option.shape});
        }
        dialog.completed = false;
    }

    std::unique_lock<std::mutex> lock(stateMutex);
    dialogCv.wait(lock, [&]() { return !running.load() || dialog.completed; });

    if (!dialog.completed) {
        dialog.active = false;
        return -1;
    }

    const int choice = dialog.choice;
    dialog.completed = false;
    dialog.kind = DialogKind::None;
    dialog.options.clear();
    dialog.tiles.clear();
    return choice;
}

void RaylibRenderer::renderLoop() {
    const int width = margin * 2 + boardSize * cellSize + sidebarWidth;
    const int height = margin * 2 + boardSize * cellSize + 60;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(width, height, WINDOW_TITLE);
    Image icon = LoadImage(WINDOW_ICON_PATH);
    if (icon.data != nullptr) {
        SetWindowIcon(icon);
        UnloadImage(icon);
    } else {
        TraceLog(LOG_WARNING, "Failed to load window icon from %s", WINDOW_ICON_PATH);
    }
    SetTargetFPS(60);
    loadTextures();
    windowReady = true;

    while (running.load()) {
        if (WindowShouldClose()) {
            running = false;
            placementCv.notify_all();
            dialogCv.notify_all();
            break;
        }

        std::vector<std::vector<char>> localGrid;
        PlayerSnapshot localPlayer;
        std::vector<std::vector<int>> localCurrentPreview;
        std::vector<std::vector<int>> localNextPreview;
        std::vector<std::vector<int>> localPlacementShape;
        bool localAllowSwap = false;
        bool localGameOver = false;
        std::vector<FinalScoreEntry> localFinalScores;
        Mode localMode;
        DialogState localDialog;
        std::vector<std::vector<LGBonus>> localBonuses;
        std::string localVictoryRule;
        bool localRestartActive = false;

        {
            std::lock_guard<std::mutex> lock(stateMutex);
            localGrid = gridSnapshot;
            localBonuses = bonusSnapshot;
            localPlayer = playerSnapshot;
            localCurrentPreview = currentTileSnapshot;
            localNextPreview = nextTileSnapshot;
            localPlacementShape = placement.workingShape;
            localAllowSwap = placement.allowSwap;
            localMode = mode;
            localGameOver = gameOverActive;
            localFinalScores = finalScores;
            localDialog = dialog;
            localVictoryRule = victoryRuleText;
            localRestartActive = restartButtonActive;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (localDialog.active) {
            drawIdle(localGrid, localPlayer, localCurrentPreview, localNextPreview, localBonuses);
        } else if (localMode == Mode::Placement && !localPlacementShape.empty()) {
            drawPlacement(localGrid, localPlayer, localPlacementShape, localNextPreview, localAllowSwap, localBonuses);
        } else if (localMode == Mode::Stone) {
            drawStonePlacement(localGrid, localPlayer, true, localBonuses);
        } else {
            drawIdle(localGrid, localPlayer, localCurrentPreview, localNextPreview, localBonuses);
        }

        bool restartClicked = false;
        if (localGameOver) {
            restartClicked = drawGameOverOverlay(localFinalScores, localVictoryRule, localRestartActive);
        }

        DialogRenderResult dialogResult;
        if (localDialog.active) {
            dialogResult = drawDialogOverlay(
                    localDialog,
                    GetMousePosition(),
                    IsMouseButtonPressed(MOUSE_BUTTON_LEFT),
                    IsKeyPressed(KEY_ESCAPE));
        }

        if (feedbackTimer > 0.0f && !feedbackMessage.empty()) {
            DrawText(feedbackMessage.c_str(),
                     margin,
                     margin + boardSize * cellSize + 20,
                     18,
                     RED);
            feedbackTimer -= GetFrameTime();
        }

        EndDrawing();

        if (restartClicked) {
            signalRestart();
        }

        if (localDialog.active && dialogResult.completed) {
            fulfillDialogResult(dialogResult, localDialog.kind);
        }
    }

    unloadTextures();
    windowReady = false;
    CloseWindow();
}

void RaylibRenderer::drawIdle(const std::vector<std::vector<char>>& grid,
                              const PlayerSnapshot& player,
                              const std::vector<std::vector<int>>& currentTile,
                              const std::vector<std::vector<int>>& nextTile,
                              const std::vector<std::vector<LGBonus>>& bonuses) const {
    const Rectangle boardRect = boardArea();

    for (int row = 0; row < boardSize; ++row) {
        for (int col = 0; col < boardSize; ++col) {
            const char cellValue = grid[row][col];
            const Rectangle rect{
                    boardRect.x + static_cast<float>(col * cellSize),
                    boardRect.y + static_cast<float>(row * cellSize),
                    static_cast<float>(cellSize),
                    static_cast<float>(cellSize)};

            drawCellBase(rect.x, rect.y, cellValue);
            if (cellValue != '.' && cellValue != '#') {
                DrawRectangleLinesEx(rect, 2.0f, colorForSymbol(cellValue));
            } else {
                DrawRectangleLinesEx(rect, 1.0f, DARKGRAY);
            }

            if (row < static_cast<int>(bonuses.size()) && col < static_cast<int>(bonuses[row].size())) {
                const LGBonus bonus = bonuses[row][col];
                if (bonus != LGBonus::None) {
                    drawBonusIcon(rect.x, rect.y, bonus);
                }
            }
        }
    }

    const float infoX = boardRect.x + boardRect.width + 12.0f;
    DrawText(TextFormat("Joueur : %s (%c)", player.name.c_str(), player.symbol),
             infoX, boardRect.y, 20, BLACK);
    DrawText(TextFormat("Score : %d", player.score), infoX, boardRect.y + 32, 20, BLACK);
    DrawText(TextFormat("Coupons : %d", player.coupons), infoX, boardRect.y + 64, 18, DARKGRAY);
    DrawText(TextFormat("Pierres : %d", player.stones), infoX, boardRect.y + 90, 18, DARKGRAY);
    DrawText(TextFormat("Vols : %d", player.steals), infoX, boardRect.y + 116, 18, DARKGRAY);

    DrawText("Tuile actuelle :", infoX, boardRect.y + 160, 18, BLACK);
    if (!currentTile.empty()) {
        drawTilePreview(currentTile, static_cast<int>(infoX), static_cast<int>(boardRect.y + 190));
    } else {
        DrawText("Aucune", infoX, boardRect.y + 190, 16, DARKGRAY);
    }

    DrawText("Tuile suivante :", infoX, boardRect.y + 280, 18, BLACK);
    if (!nextTile.empty()) {
        drawTilePreview(nextTile, static_cast<int>(infoX), static_cast<int>(boardRect.y + 310));
    } else {
        DrawText("Aucune", infoX, boardRect.y + 310, 16, DARKGRAY);
    }
}

void RaylibRenderer::drawPlacement(const std::vector<std::vector<char>>& grid,
                                   const PlayerSnapshot& player,
                                   std::vector<std::vector<int>> tileShape,
                                   const std::vector<std::vector<int>>& nextTilePreview,
                                   bool allowSwap,
                                   const std::vector<std::vector<LGBonus>>& bonuses) {
    const Rectangle boardRect = boardArea();
    const Vector2 mouse = GetMousePosition();
    const int hoveredCol = static_cast<int>((mouse.x - boardRect.x) / cellSize);
    const int hoveredRow = static_cast<int>((mouse.y - boardRect.y) / cellSize);
    const bool insideBoard = mouse.x >= boardRect.x && mouse.x < boardRect.x + boardRect.width &&
                             mouse.y >= boardRect.y && mouse.y < boardRect.y + boardRect.height;

    bool canPlace = false;
    if (insideBoard) {
        canPlace = canPlaceLocally(hoveredRow, hoveredCol, tileShape, player.symbol, grid);
    }

    for (int row = 0; row < boardSize; ++row) {
        for (int col = 0; col < boardSize; ++col) {
            const char cellValue = grid[row][col];
            const Rectangle rect{
                    boardRect.x + static_cast<float>(col * cellSize),
                    boardRect.y + static_cast<float>(row * cellSize),
                    static_cast<float>(cellSize),
                    static_cast<float>(cellSize)};

            drawCellBase(rect.x, rect.y, cellValue);
            if (cellValue != '.' && cellValue != '#') {
                DrawRectangleLinesEx(rect, 2.0f, colorForSymbol(cellValue));
            } else {
                DrawRectangleLinesEx(rect, 1.0f, DARKGRAY);
            }

            if (row < static_cast<int>(bonuses.size()) && col < static_cast<int>(bonuses[row].size())) {
                const LGBonus bonus = bonuses[row][col];
                if (bonus != LGBonus::None) {
                    drawBonusIcon(rect.x, rect.y, bonus);
                }
            }
        }
    }

    if (insideBoard) {
        const Color ghostColor = canPlace ? ColorAlpha(GREEN, 0.45f) : ColorAlpha(RED, 0.45f);
        for (int row = 0; row < static_cast<int>(tileShape.size()); ++row) {
            for (int col = 0; col < static_cast<int>(tileShape[row].size()); ++col) {
                if (tileShape[row][col] != 1) {
                    continue;
                }
                const Rectangle rect{
                        boardRect.x + static_cast<float>((hoveredCol + col) * cellSize),
                        boardRect.y + static_cast<float>((hoveredRow + row) * cellSize),
                        static_cast<float>(cellSize),
                        static_cast<float>(cellSize)};
                DrawRectangleRec(rect, ghostColor);
                DrawRectangleLinesEx(rect, 1.0f, DARKGRAY);
            }
        }
    }

    const float infoX = boardRect.x + boardRect.width + 12.0f;
    DrawText("Placement en cours", infoX, boardRect.y, 20, BLACK);
    DrawText(TextFormat("Joueur : %s (%c)", player.name.c_str(), player.symbol),
             infoX, boardRect.y + 32, 18, BLACK);
    DrawText(TextFormat("Coupons : %d", player.coupons), infoX, boardRect.y + 60, 18, BLACK);

    DrawText("Contrôles :", infoX, boardRect.y + 100, 18, DARKGRAY);
    DrawText("- Molette : rotation", infoX, boardRect.y + 125, 16, DARKGRAY);
    DrawText("- Clic droit : miroir", infoX, boardRect.y + 145, 16, DARKGRAY);
    DrawText("- Clic gauche : poser", infoX, boardRect.y + 165, 16, DARKGRAY);

    const Rectangle swapButton{infoX, boardRect.y + 200, 190.0f, 38.0f};
    const bool swapEnabled = allowSwap && player.coupons > 0;
    const bool swapHovered = CheckCollisionPointRec(mouse, swapButton);
    const bool swapClicked = swapHovered && swapEnabled && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    DrawRectangleRounded(swapButton, 0.2f, 8, swapEnabled ? DARKGREEN : GRAY);
    DrawText("Échanger la tuile",
             static_cast<int>(swapButton.x + 8),
             static_cast<int>(swapButton.y + 10),
             16,
             RAYWHITE);

    DrawText("Tuile suivante :", infoX, boardRect.y + 250, 18, BLACK);
    if (!nextTilePreview.empty()) {
        drawTilePreview(nextTilePreview, static_cast<int>(infoX), static_cast<int>(boardRect.y + 280));
    } else {
        DrawText("Aucune", infoX, boardRect.y + 280, 16, DARKGRAY);
    }

    bool shapeUpdated = false;
    const float wheel = GetMouseWheelMove();
    if (wheel > 0.0f) {
        tileShape = rotateClockwise(tileShape);
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            placement.workingShape = tileShape;
        }
        shapeUpdated = true;
    } else if (wheel < 0.0f) {
        tileShape = rotateCounterClockwise(tileShape);
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            placement.workingShape = tileShape;
        }
        shapeUpdated = true;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_RIGHT)) {
        tileShape = flipHorizontal(tileShape);
        {
            std::lock_guard<std::mutex> lock(stateMutex);
            placement.workingShape = tileShape;
        }
        shapeUpdated = true;
    }

    if (swapClicked) {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (placement.active && !placement.resultReady) {
            placement.resultReady = true;
            placement.resultSwap = true;
            placementCv.notify_one();
        }
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && swapHovered) {
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && insideBoard && canPlace && !shapeUpdated) {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (placement.active && !placement.resultReady) {
            placement.resultReady = true;
            placement.resultSuccess = true;
            placement.resultSwap = false;
            placement.resultCancelled = false;
            placement.resultX = hoveredRow;
            placement.resultY = hoveredCol;
            placement.resultShape = tileShape;
            placementCv.notify_one();
        }
    } else if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && insideBoard && !canPlace) {
        feedbackMessage = "Placement impossible à cet endroit.";
        feedbackTimer = 1.0f;
    }
}

void RaylibRenderer::drawStonePlacement(const std::vector<std::vector<char>>& grid,
                                        const PlayerSnapshot& player,
                                        bool allowSkip,
                                        const std::vector<std::vector<LGBonus>>& bonuses) {
    const Rectangle boardRect = boardArea();
    const Vector2 mouse = GetMousePosition();
    const int hoveredCol = static_cast<int>((mouse.x - boardRect.x) / cellSize);
    const int hoveredRow = static_cast<int>((mouse.y - boardRect.y) / cellSize);
    const bool insideBoard = mouse.x >= boardRect.x && mouse.x < boardRect.x + boardRect.width &&
                             mouse.y >= boardRect.y && mouse.y < boardRect.y + boardRect.height;

    bool canPlace = false;
    if (insideBoard) {
        canPlace = canPlaceStone(hoveredRow, hoveredCol, grid);
    }

    for (int row = 0; row < boardSize; ++row) {
        for (int col = 0; col < boardSize; ++col) {
            const char cellValue = grid[row][col];
            const Rectangle rect{
                    boardRect.x + static_cast<float>(col * cellSize),
                    boardRect.y + static_cast<float>(row * cellSize),
                    static_cast<float>(cellSize),
                    static_cast<float>(cellSize)};

            drawCellBase(rect.x, rect.y, cellValue);
            if (cellValue != '.' && cellValue != '#') {
                DrawRectangleLinesEx(rect, 2.0f, colorForSymbol(cellValue));
            } else {
                DrawRectangleLinesEx(rect, 1.0f, DARKGRAY);
            }

            if (row < static_cast<int>(bonuses.size()) && col < static_cast<int>(bonuses[row].size())) {
                const LGBonus bonus = bonuses[row][col];
                if (bonus != LGBonus::None) {
                    drawBonusIcon(rect.x, rect.y, bonus);
                }
            }
        }
    }

    if (insideBoard) {
        const Color ghostColor = canPlace ? ColorAlpha(BLACK, 0.45f) : ColorAlpha(RED, 0.45f);
        const Rectangle rect{
                boardRect.x + static_cast<float>(hoveredCol * cellSize),
                boardRect.y + static_cast<float>(hoveredRow * cellSize),
                static_cast<float>(cellSize),
                static_cast<float>(cellSize)};
        DrawRectangleRec(rect, ghostColor);
        DrawRectangleLinesEx(rect, 1.0f, DARKGRAY);
    }

    const float infoX = boardRect.x + boardRect.width + 12.0f;
    DrawText("Placement de pierre", infoX, boardRect.y, 20, BLACK);
    DrawText(TextFormat("Joueur : %s (%c)", player.name.c_str(), player.symbol),
             infoX, boardRect.y + 32, 18, BLACK);
    DrawText("Clic gauche sur une case vide pour bloquer.", infoX, boardRect.y + 70, 16, DARKGRAY);

    const Rectangle skipButton{infoX, boardRect.y + 110, 190.0f, 38.0f};
    const bool skipHovered = CheckCollisionPointRec(mouse, skipButton);
    const bool skipClicked = allowSkip && skipHovered && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
    DrawRectangleRounded(skipButton, 0.2f, 8, allowSkip ? MAROON : GRAY);
    DrawText("Passer",
             static_cast<int>(skipButton.x + 60),
             static_cast<int>(skipButton.y + 10),
             16,
             RAYWHITE);

    if (skipClicked) {
        std::lock_guard<std::mutex> lock(stateMutex);
        if (stone.active && !stone.resultReady) {
            stone.resultReady = true;
            stone.resultPlaced = false;
            placementCv.notify_one();
        }
        return;
    }

    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && insideBoard) {
        if (canPlace) {
            std::lock_guard<std::mutex> lock(stateMutex);
            if (stone.active && !stone.resultReady) {
                stone.resultReady = true;
                stone.resultPlaced = true;
                stone.resultX = hoveredRow;
                stone.resultY = hoveredCol;
                placementCv.notify_one();
            }
        } else {
            feedbackMessage = "Impossible de placer la pierre ici.";
            feedbackTimer = 1.0f;
        }
    }
}

void RaylibRenderer::drawCellBase(float x, float y, char cellValue) const {
    const Rectangle dest{x, y, static_cast<float>(cellSize), static_cast<float>(cellSize)};
    const bool isPlayerCell = (cellValue != '.' && cellValue != '#');

    if (isPlayerCell && texturesLoaded && grassTexture.id > 0) {
        const Rectangle src{0.0f, 0.0f, static_cast<float>(grassTexture.width), static_cast<float>(grassTexture.height)};
        DrawTexturePro(grassTexture, src, dest, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
        return;
    }

    if (cellValue == '#' && texturesLoaded && stoneTexture.id > 0) {
        const Rectangle src{0.0f, 0.0f, static_cast<float>(stoneTexture.width), static_cast<float>(stoneTexture.height)};
        DrawTexturePro(stoneTexture, src, dest, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
        return;
    }

    const Color fallback = (cellValue == '#') ? DARKGRAY : LIGHTGRAY;
    DrawRectangleRec(dest, fallback);
}

void RaylibRenderer::drawBonusIcon(float x, float y, LGBonus bonus) const {
    if (bonus == LGBonus::None) {
        return;
    }

    const Texture2D* texture = nullptr;
    if (texturesLoaded) {
        switch (bonus) {
            case LGBonus::Coupon:
                if (bonusCouponTexture.id > 0) texture = &bonusCouponTexture;
                break;
            case LGBonus::Stone:
                if (bonusStoneTexture.id > 0) texture = &bonusStoneTexture;
                break;
            case LGBonus::Robbery:
                if (bonusRobberyTexture.id > 0) texture = &bonusRobberyTexture;
                break;
            default:
                break;
        }
    }

    const float cell = static_cast<float>(cellSize);
    const float size = std::max(12.0f, cell * 0.65f);
    const Rectangle dest{
            x + (cell - size) * 0.5f,
            y + (cell - size) * 0.5f,
            size,
            size};

    if (texture) {
        const Rectangle src{0.0f, 0.0f, static_cast<float>(texture->width), static_cast<float>(texture->height)};
        DrawTexturePro(*texture, src, dest, Vector2{0.0f, 0.0f}, 0.0f, WHITE);
        return;
    }

    const float centerX = dest.x + dest.width / 2.0f;
    const float centerY = dest.y + dest.height / 2.0f;
    DrawCircleV(Vector2{centerX, centerY}, dest.width / 2.5f, ColorAlpha(colorForBonus(bonus), 0.85f));
    const char* label = bonusLabel(bonus);
    const int textWidth = MeasureText(label, 14);
    DrawText(label,
             static_cast<int>(centerX - textWidth / 2.0f),
             static_cast<int>(centerY - 7.0f),
             14,
             RAYWHITE);
}

Rectangle RaylibRenderer::boardArea() const {
    return {
            static_cast<float>(margin),
            static_cast<float>(margin),
            static_cast<float>(boardSize * cellSize),
            static_cast<float>(boardSize * cellSize)
    };
}

bool RaylibRenderer::canPlaceLocally(int x, int y, const std::vector<std::vector<int>>& tileShape, char playerSymbol, const std::vector<std::vector<char>>& grid) const {
    if (x < 0 || y < 0) {
        return false;
    }
    if (x + static_cast<int>(tileShape.size()) > boardSize ||
        y + static_cast<int>(tileShape[0].size()) > boardSize) {
        return false;
    }

    const bool playerAlreadyOnBoard = std::any_of(
            grid.begin(),
            grid.end(),
            [playerSymbol](const std::vector<char>& row) {
                return std::any_of(row.begin(), row.end(), [playerSymbol](char c) { return c == playerSymbol; });
            });

    bool touchesOwnTerritory = !playerAlreadyOnBoard;

    for (int i = 0; i < static_cast<int>(tileShape.size()); ++i) {
        for (int j = 0; j < static_cast<int>(tileShape[i].size()); ++j) {
            if (tileShape[i][j] != 1) {
                continue;
            }

            char cell = grid[x + i][y + j];
            if (cell != '.') {
                return false;
            }

            if (playerAlreadyOnBoard && !touchesOwnTerritory) {
                static constexpr int dirs[4][2] = {{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
                for (auto dir : dirs) {
                    const int nx = x + i + dir[0];
                    const int ny = y + j + dir[1];
                    if (nx >= 0 && nx < boardSize && ny >= 0 && ny < boardSize) {
                        if (grid[nx][ny] == playerSymbol) {
                            touchesOwnTerritory = true;
                            break;
                        }
                    }
                }
            }

            for (int dx = -1; dx <= 1; ++dx) {
                for (int dy = -1; dy <= 1; ++dy) {
                    if (dx == 0 && dy == 0) {
                        continue;
                    }
                    const int nx = x + i + dx;
                    const int ny = y + j + dy;
                    if (nx >= 0 && nx < boardSize && ny >= 0 && ny < boardSize) {
                        const char neighbor = grid[nx][ny];
                        if (neighbor != '.' && neighbor != playerSymbol) {
                            return false;
                        }
                    }
                }
            }
        }
    }

    return touchesOwnTerritory;
}

bool RaylibRenderer::canPlaceStone(int x, int y, const std::vector<std::vector<char>>& grid) const {
    if (x < 0 || y < 0 || x >= boardSize || y >= boardSize) {
        return false;
    }
    return grid[x][y] == '.';
}

RaylibRenderer::DialogRenderResult RaylibRenderer::drawDialogOverlay(const DialogState& dialogState,
                                                                     const Vector2& mouse,
                                                                     bool mouseClick,
                                                                     bool escapePressed) const {
    DialogRenderResult result;
    if (!dialogState.active) {
        return result;
    }

    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    DrawRectangle(0, 0, width, height, ColorAlpha(BLACK, 0.6f));

    int panelW = 480;
    int panelH = 240;

    switch (dialogState.kind) {
        case DialogKind::Confirm:
            panelW = 480;
            panelH = 220;
            break;
        case DialogKind::List:
            panelW = 460;
            panelH = 160 + static_cast<int>(dialogState.options.size()) * 44;
            break;
        case DialogKind::TilePicker:
            panelW = std::min(width - 80,
                              std::max(520, static_cast<int>(dialogState.tiles.size()) * 140));
            panelH = 360;
            break;
        default:
            break;
    }

    panelW = std::clamp(panelW, 320, width - 80);
    panelH = std::clamp(panelH, 180, height - 80);

    const int panelX = (width - panelW) / 2;
    const int panelY = (height - panelH) / 2;

    DrawRectangle(panelX, panelY, panelW, panelH, ColorAlpha(DARKGRAY, 0.92f));
    DrawRectangleLines(panelX, panelY, panelW, panelH, RAYWHITE);

    DrawText(dialogState.title.c_str(), panelX + 20, panelY + 18, 24, RAYWHITE);
    if (!dialogState.message.empty()) {
        DrawText(dialogState.message.c_str(), panelX + 20, panelY + 58, 18, RAYWHITE);
    }

    auto drawButton = [&](const Rectangle& rect, const std::string& label, Color color) {
        DrawRectangleRounded(rect, 0.2f, 8, color);
        const int textWidth = MeasureText(label.c_str(), 18);
        DrawText(label.c_str(),
                 static_cast<int>(rect.x + (rect.width - textWidth) / 2.0f),
                 static_cast<int>(rect.y + rect.height / 2.0f - 9),
                 18,
                 RAYWHITE);
    };

    auto handleConfirm = [&]() {
        const Rectangle confirmRect{static_cast<float>(panelX + 40),
                                    static_cast<float>(panelY + panelH - 70),
                                    150.0f,
                                    44.0f};
        const Rectangle cancelRect{static_cast<float>(panelX + panelW - 190),
                                   static_cast<float>(panelY + panelH - 70),
                                   150.0f,
                                   44.0f};
        const bool confirmHover = CheckCollisionPointRec(mouse, confirmRect);
        const bool cancelHover = CheckCollisionPointRec(mouse, cancelRect);

        drawButton(confirmRect, dialogState.confirmLabel, confirmHover ? DARKGREEN : GREEN);
        drawButton(cancelRect, dialogState.cancelLabel, cancelHover ? MAROON : DARKGRAY);

        if (escapePressed) {
            result.completed = true;
            result.boolValue = false;
            return;
        }

        if (mouseClick && confirmHover) {
            result.completed = true;
            result.boolValue = true;
        } else if (mouseClick && cancelHover) {
            result.completed = true;
            result.boolValue = false;
        }
    };

    auto handleList = [&]() {
        int y = panelY + 70;
        for (size_t i = 0; i < dialogState.options.size(); ++i) {
            Rectangle row{static_cast<float>(panelX + 30),
                          static_cast<float>(y),
                          static_cast<float>(panelW - 60),
                          36.0f};
            const bool hovered = CheckCollisionPointRec(mouse, row);
            DrawRectangleRounded(row, 0.15f, 6, hovered ? DARKGREEN : Color{60, 60, 60, 255});
            DrawText(dialogState.options[i].c_str(),
                     static_cast<int>(row.x + 12),
                     static_cast<int>(row.y + 8),
                     18,
                     RAYWHITE);

            if (mouseClick && hovered) {
                result.completed = true;
                result.indexValue = static_cast<int>(i);
                return;
            }
            y += 44;
        }

        const Rectangle cancelRect{static_cast<float>(panelX + 30),
                                   static_cast<float>(panelY + panelH - 60),
                                   static_cast<float>(panelW - 60),
                                   40.0f};
        const bool cancelHover = CheckCollisionPointRec(mouse, cancelRect);
        drawButton(cancelRect, dialogState.cancelLabel, cancelHover ? MAROON : DARKGRAY);

        if ((mouseClick && cancelHover) || escapePressed) {
            result.completed = true;
            result.indexValue = -1;
        }
    };

    auto handleTiles = [&]() {
        const float cardWidth = 130.0f;
        const float cardHeight = 190.0f;
        const float spacing = 16.0f;
        const float totalWidth = dialogState.tiles.empty()
                                 ? 0.0f
                                 : dialogState.tiles.size() * cardWidth + (dialogState.tiles.size() - 1) * spacing;
        const float startX = panelX + std::max(20.0f, (panelW - totalWidth) / 2.0f);
        const float cardY = panelY + 70.0f;

        for (size_t i = 0; i < dialogState.tiles.size(); ++i) {
            Rectangle card{startX + static_cast<float>(i) * (cardWidth + spacing),
                           cardY,
                           cardWidth,
                           cardHeight};
            const bool hovered = CheckCollisionPointRec(mouse, card);
            DrawRectangleRounded(card, 0.15f, 6, hovered ? DARKGREEN : Color{70, 70, 70, 255});
            DrawRectangleLinesEx(card, 2.0f, RAYWHITE);

            const std::string label = "Tuile #" + std::to_string(dialogState.tiles[i].id);
            DrawText(label.c_str(),
                     static_cast<int>(card.x + 8),
                     static_cast<int>(card.y + 8),
                     18,
                     RAYWHITE);

            drawTilePreview(dialogState.tiles[i].shape,
                            static_cast<int>(card.x + 10),
                            static_cast<int>(card.y + 40));

            if (mouseClick && hovered) {
                result.completed = true;
                result.indexValue = static_cast<int>(i);
                return;
            }
        }

        const Rectangle cancelRect{static_cast<float>(panelX + panelW - 170),
                                   static_cast<float>(panelY + panelH - 60),
                                   150.0f,
                                   40.0f};
        const bool cancelHover = CheckCollisionPointRec(mouse, cancelRect);
        drawButton(cancelRect, dialogState.cancelLabel, cancelHover ? MAROON : DARKGRAY);

        if ((mouseClick && cancelHover) || escapePressed) {
            result.completed = true;
            result.indexValue = -1;
        }
    };

    switch (dialogState.kind) {
        case DialogKind::Confirm:
            handleConfirm();
            break;
        case DialogKind::List:
            handleList();
            break;
        case DialogKind::TilePicker:
            handleTiles();
            break;
        default:
            break;
    }

    return result;
}

void RaylibRenderer::fulfillDialogResult(const DialogRenderResult& result, DialogKind kind) {
    if (!result.completed) {
        return;
    }

    std::lock_guard<std::mutex> lock(stateMutex);
    if (!dialog.active || dialog.kind != kind) {
        return;
    }

    if (kind == DialogKind::Confirm) {
        dialog.boolResult = result.boolValue;
    } else {
        dialog.choice = result.indexValue;
    }
    dialog.completed = true;
    dialog.active = false;
    dialogCv.notify_all();
}

void RaylibRenderer::showGameOver(const std::vector<FinalScoreEntry>& scores,
                                  const std::string& victoryRule) {
    std::lock_guard<std::mutex> lock(stateMutex);
    finalScores = scores;
    victoryRuleText = victoryRule;
    gameOverActive = true;
    restartButtonActive = true;
    restartPressed = false;
}

bool RaylibRenderer::drawGameOverOverlay(const std::vector<FinalScoreEntry>& scores,
                                         const std::string& ruleText,
                                         bool allowRestart) const {
    if (scores.empty()) {
        return false;
    }

    bool restartClicked = false;
    const Vector2 mouse = GetMousePosition();
    const bool mouseClick = IsMouseButtonPressed(MOUSE_BUTTON_LEFT);

    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    DrawRectangle(0, 0, width, height, ColorAlpha(BLACK, 0.6f));

    const int panelX = margin;
    const int panelY = margin;
    const int panelW = width - margin * 2;
    const int panelH = height - margin * 2;
    DrawRectangle(panelX, panelY, panelW, panelH, ColorAlpha(DARKGRAY, 0.92f));
    DrawRectangleLines(panelX, panelY, panelW, panelH, RAYWHITE);

    DrawText("Fin de partie", panelX + 20, panelY + 20, 32, RAYWHITE);
    if (!ruleText.empty()) {
        DrawText(ruleText.c_str(), panelX + 20, panelY + 60, 20, RAYWHITE);
    }

    std::string winnerNames;
    for (const auto& entry : scores) {
        if (!entry.winner) {
            continue;
        }
        if (!winnerNames.empty()) {
            winnerNames += ", ";
        }
        winnerNames += entry.name;
    }
    if (!winnerNames.empty()) {
        DrawText(TextFormat("Vainqueur%s : %s",
                            (winnerNames.find(',') == std::string::npos) ? "" : "s",
                            winnerNames.c_str()),
                 panelX + 20,
                 panelY + 95,
                 22,
                 GOLD);
    }

    DrawText("Classement :", panelX + 20, panelY + 135, 22, RAYWHITE);
    const int headerY = panelY + 170;
    const int colName = panelX + 40;
    const int colTerritory = panelX + panelW / 2 - 60;
    const int colSquare = panelX + panelW - 200;

    DrawText("Joueur", colName, headerY, 18, RAYWHITE);
    DrawText("Territoire", colTerritory, headerY, 18, RAYWHITE);
    DrawText("Plus grand carre", colSquare, headerY, 18, RAYWHITE);

    int y = headerY + 30;
    int rank = 1;
    for (const auto& entry : scores) {
        const Color rowColor = entry.winner ? GOLD : RAYWHITE;
        DrawText(TextFormat("%d. %s (%c)",
                            rank,
                            entry.name.c_str(),
                            entry.symbol),
                 colName,
                 y,
                 20,
                 rowColor);
        DrawText(TextFormat("%d cases", entry.territory),
                 colTerritory,
                 y,
                 20,
                 rowColor);
        const int squareSide = squareSideFromArea(entry.largestSquare);
        const char* squareLabel = (squareSide > 0)
                                  ? TextFormat("%dx%d", squareSide, squareSide)
                                  : TextFormat("%d cases", entry.largestSquare);
        DrawText(squareLabel,
                 colSquare,
                 y,
                 20,
                 rowColor);        y += 32;
        ++rank;
    }

    if (allowRestart) {
        const Rectangle restartBtn{
                static_cast<float>(panelX + panelW - 220),
                static_cast<float>(panelY + panelH - 70),
                180.0f,
                44.0f};
        const bool hovered = CheckCollisionPointRec(mouse, restartBtn);
        DrawRectangleRounded(restartBtn, 0.2f, 8, hovered ? DARKGREEN : GREEN);
        const char* label = "Rejouer";
        const int textWidth = MeasureText(label, 20);
        DrawText(label,
                 static_cast<int>(restartBtn.x + (restartBtn.width - textWidth) / 2.0f),
                 static_cast<int>(restartBtn.y + 10),
                 20,
                 RAYWHITE);
        if (hovered && mouseClick) {
            restartClicked = true;
        }
    } else {
        DrawText("Fermez la fenetre pour quitter.", panelX + 20, panelY + panelH - 60, 18, RAYWHITE);
    }

    return restartClicked;
}

Color RaylibRenderer::colorForSymbol(char symbol) const {
    const int index = std::max(0, symbol - 'A');
    const float hue = std::fmod(static_cast<float>(index) * 45.0f, 360.0f);
    return ColorFromHSV(hue, 0.65f, 0.85f);
}

Color RaylibRenderer::colorForBonus(LGBonus bonus) const {
    switch (bonus) {
        case LGBonus::Coupon:
            return ORANGE;
        case LGBonus::Stone:
            return DARKGRAY;
        case LGBonus::Robbery:
            return PURPLE;
        default:
            return WHITE;
    }
}

void RaylibRenderer::loadTextures() {
    if (texturesLoaded) {
        return;
    }

    grassTexture = LoadTexture("../ressources/grass.jpg");
    stoneTexture = LoadTexture("../ressources/placed_stone.jpg");
    bonusCouponTexture = LoadTexture("../ressources/powerup_exchange.jpg");
    bonusStoneTexture = LoadTexture("../ressources/powerup_stone.jpg");
    bonusRobberyTexture = LoadTexture("../ressources/powerup_steal.jpg");
    texturesLoaded = true;
}

void RaylibRenderer::unloadTextures() {
    if (!texturesLoaded) {
        return;
    }

    auto release = [](Texture2D& texture) {
        if (texture.id > 0) {
            UnloadTexture(texture);
            texture = {};
        }
    };

    release(grassTexture);
    release(stoneTexture);
    release(bonusCouponTexture);
    release(bonusStoneTexture);
    release(bonusRobberyTexture);
    texturesLoaded = false;
}

void RaylibRenderer::signalRestart() {
    std::lock_guard<std::mutex> lock(stateMutex);
    if (!restartButtonActive || restartPressed) {
        return;
    }
    restartPressed = true;
    restartButtonActive = false;
    gameOverActive = false;
    restartCv.notify_all();
}

void RaylibRenderer::drawTilePreview(const std::vector<std::vector<int>>& tileShape, int originX, int originY) const {
    if (tileShape.empty()) {
        return;
    }

    const int previewCell = std::max(8, cellSize / 2);
    const int padding = 4;

    for (int row = 0; row < static_cast<int>(tileShape.size()); ++row) {
        for (int col = 0; col < static_cast<int>(tileShape[row].size()); ++col) {
            if (tileShape[row][col] != 1) {
                continue;
            }

            const Rectangle rect{
                    static_cast<float>(originX + col * (previewCell + padding)),
                    static_cast<float>(originY + row * (previewCell + padding)),
                    static_cast<float>(previewCell),
                    static_cast<float>(previewCell)};
            DrawRectangleRec(rect, DARKGREEN);
            DrawRectangleLinesEx(rect, 1.0f, BLACK);
        }
    }
}

std::vector<std::vector<int>> RaylibRenderer::rotateClockwise(const std::vector<std::vector<int>>& shape) {
    const int rows = static_cast<int>(shape.size());
    const int cols = static_cast<int>(shape[0].size());
    std::vector<std::vector<int>> rotated(cols, std::vector<int>(rows, 0));

    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            rotated[j][rows - 1 - i] = shape[i][j];
        }
    }
    return rotated;
}

std::vector<std::vector<int>> RaylibRenderer::rotateCounterClockwise(const std::vector<std::vector<int>>& shape) {
    const int rows = static_cast<int>(shape.size());
    const int cols = static_cast<int>(shape[0].size());
    std::vector<std::vector<int>> rotated(cols, std::vector<int>(rows, 0));

    for (int i = 0; i < rows; ++i) {
        for (int j = 0; j < cols; ++j) {
            rotated[cols - 1 - j][i] = shape[i][j];
        }
    }
    return rotated;
}

std::vector<std::vector<int>> RaylibRenderer::flipHorizontal(const std::vector<std::vector<int>>& shape) {
    std::vector<std::vector<int>> flipped = shape;
    for (auto& row : flipped) {
        std::reverse(row.begin(), row.end());
    }
    return flipped;
}

