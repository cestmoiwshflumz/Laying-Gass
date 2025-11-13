#include "RaylibRenderer.h"
#include <algorithm>
#include <cmath>

std::vector<std::vector<int>> bonusSnapshot;

struct ConfirmBox {
    bool active = false;
    std::string question;
    std::string yes = "Oui";
    std::string no = "Non";
    bool answered = false;
    bool answerYes = false;

} confirm;


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
    renderThread = std::thread(&RaylibRenderer::renderLoop, this);
    bonusSnapshot.assign(boardSize, std::vector<int>(boardSize, 0));

}

RaylibRenderer::~RaylibRenderer() {
    running = false;
    placementCv.notify_all();
    if (renderThread.joinable()) {
        renderThread.join();
    }
}

bool RaylibRenderer::isReady() const {
    return windowReady.load();
}

void RaylibRenderer::updateState(const Board& board,
                                 const Player& currentPlayer,
                                 const Tile* currentTile,
                                 const Tile* nextTile) {
    std::lock_guard<std::mutex> lock(stateMutex);
    gridSnapshot = board.getGrid();
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

void RaylibRenderer::setBonusGrid(const std::vector<std::vector<int>>& grid) {
    std::lock_guard<std::mutex> lock(stateMutex);
    bonusSnapshot = grid;
}


RaylibRenderer::PlacementResult RaylibRenderer::requestTilePlacement(const Board& board,
                                                                     const Player& currentPlayer,
                                                                     const Tile& tile,
                                                                     const Tile* nextTile,
                                                                     bool allowSwap) {
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        gridSnapshot = board.getGrid();
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

void RaylibRenderer::renderLoop() {
    const int width = margin * 2 + boardSize * cellSize + sidebarWidth;
    const int height = margin * 2 + boardSize * cellSize + 60;

    SetConfigFlags(FLAG_WINDOW_RESIZABLE);
    InitWindow(width, height, "Laying Grass - Interface");
    SetTargetFPS(60);
    windowReady = true;

    while (running.load()) {
        if (WindowShouldClose()) {
            running = false;
            placementCv.notify_all();
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

        {
            std::lock_guard<std::mutex> lock(stateMutex);
            localGrid = gridSnapshot;
            localPlayer = playerSnapshot;
            localCurrentPreview = currentTileSnapshot;
            localNextPreview = nextTileSnapshot;
            localPlacementShape = placement.workingShape;
            localAllowSwap = placement.allowSwap;
            localMode = mode;
            localGameOver = gameOverActive;
            localFinalScores = finalScores;
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);

        if (localMode == Mode::Placement && !localPlacementShape.empty()) {
            drawPlacement(localGrid, localPlayer, localPlacementShape, localNextPreview, localAllowSwap);
        } else if (localMode == Mode::Stone) {
            drawStonePlacement(localGrid, localPlayer, true);
        } else {
            drawIdle(localGrid, localPlayer, localCurrentPreview, localNextPreview);
        }

        if (localGameOver) {
            drawGameOverOverlay(localFinalScores);
        }

        if (feedbackTimer > 0.0f && !feedbackMessage.empty()) {
            DrawText(feedbackMessage.c_str(),
                     margin,
                     margin + boardSize * cellSize + 20,
                     18,
                     RED);
            feedbackTimer -= GetFrameTime();
        }

        if (confirm.active) {
            const int w = GetScreenWidth(), h = GetScreenHeight();
            DrawRectangle(0, 0, w, h, ColorAlpha(BLACK, 0.55f));
            const int bw = 520, bh = 160;
            const int bx = (w - bw) / 2, by = (h - bh) / 2;
            DrawRectangleRounded({(float)bx,(float)by,(float)bw,(float)bh}, 0.08f, 8, RAYWHITE);
            DrawText(confirm.question.c_str(), bx + 20, by + 20, 22, BLACK);

            Rectangle yesBtn{(float)(bx + 60), (float)(by + bh - 60), 160.0f, 36.0f};
            Rectangle noBtn {(float)(bx + bw - 220), (float)(by + bh - 60), 160.0f, 36.0f};
            Vector2 m = GetMousePosition();
            bool hYes = CheckCollisionPointRec(m, yesBtn);
            bool hNo  = CheckCollisionPointRec(m, noBtn);
            DrawRectangleRounded(yesBtn, 0.2f, 8, hYes ? DARKGREEN : GREEN);
            DrawRectangleRounded(noBtn,  0.2f, 8, hNo  ? MAROON    : RED);
            DrawText(confirm.yes.c_str(), (int)yesBtn.x + 16, (int)yesBtn.y + 8, 20, RAYWHITE);
            DrawText(confirm.no.c_str(),  (int)noBtn.x  + 16, (int)noBtn.y  + 8, 20, RAYWHITE);

            if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
                if (hYes || hNo) {
                    std::lock_guard<std::mutex> lock(stateMutex);
                    confirm.answerYes = hYes;
                    confirm.answered  = true;
                    placementCv.notify_one();
                }
            }
        }


        EndDrawing();
    }

    windowReady = false;
    CloseWindow();
}

void RaylibRenderer::drawIdle(const std::vector<std::vector<char>>& grid,
                              const PlayerSnapshot& player,
                              const std::vector<std::vector<int>>& currentTile,
                              const std::vector<std::vector<int>>& nextTile) const {
    const Rectangle boardRect = boardArea();

    for (int row = 0; row < boardSize; ++row) {
        for (int col = 0; col < boardSize; ++col) {
            const char cellValue = grid[row][col];
            const Rectangle rect{
                    boardRect.x + static_cast<float>(col * cellSize),
                    boardRect.y + static_cast<float>(row * cellSize),
                    static_cast<float>(cellSize),
                    static_cast<float>(cellSize)};

            Color fill = LIGHTGRAY;
            if (cellValue == '#') {
                fill = BLACK;
            } else if (cellValue != '.') {
                fill = colorForSymbol(cellValue);
            }

            DrawRectangleRec(rect, fill);
            DrawRectangleLinesEx(rect, 1.0f, DARKGRAY);
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
                                   bool allowSwap) {
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

            Color fill = LIGHTGRAY;
            if (cellValue == '#') {
                fill = BLACK;
            } else if (cellValue != '.') {
                fill = colorForSymbol(cellValue);
            }

            DrawRectangleRec(rect, fill);
            DrawRectangleLinesEx(rect, 1.0f, DARKGRAY);
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
                                        bool allowSkip) {
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

            Color fill = LIGHTGRAY;
            if (cellValue == '#') {
                fill = BLACK;
            } else if (cellValue != '.') {
                fill = colorForSymbol(cellValue);
            }

            DrawRectangleRec(rect, fill);
            DrawRectangleLinesEx(rect, 1.0f, DARKGRAY);
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

void RaylibRenderer::showGameOver(const std::vector<Player>& players) {
    std::lock_guard<std::mutex> lock(stateMutex);
    finalScores.clear();
    finalScores.reserve(players.size());
    for (const auto& player : players) {
        finalScores.push_back(FinalScoreEntry{
                player.getName(),
                player.getSymbol(),
                player.getScore()
        });
    }
    std::sort(finalScores.begin(), finalScores.end(), [](const FinalScoreEntry& lhs, const FinalScoreEntry& rhs) {
        return lhs.score > rhs.score;
    });
    gameOverActive = true;
}

void RaylibRenderer::drawGameOverOverlay(const std::vector<FinalScoreEntry>& scores) const {
    if (scores.empty()) {
        return;
    }

    const int width = GetScreenWidth();
    const int height = GetScreenHeight();
    DrawRectangle(0, 0, width, height, ColorAlpha(BLACK, 0.6f));

    const int panelX = margin;
    const int panelY = margin;
    const int panelW = width - margin * 2;
    const int panelH = height - margin * 2;
    DrawRectangle(panelX, panelY, panelW, panelH, ColorAlpha(DARKGRAY, 0.9f));
    DrawRectangleLines(panelX, panelY, panelW, panelH, RAYWHITE);

    DrawText("Fin de partie", panelX + 20, panelY + 20, 32, RAYWHITE);
    DrawText("Classement :", panelX + 20, panelY + 70, 22, RAYWHITE);

    int y = panelY + 110;
    int rank = 1;
    for (const auto& entry : scores) {
        DrawText(TextFormat("%d. %s (%c) - %d pts",
                            rank,
                            entry.name.c_str(),
                            entry.symbol,
                            entry.score),
                 panelX + 40,
                 y,
                 20,
                 RAYWHITE);
        y += 28;
        ++rank;
    }

    DrawText("Fermez la fenetre pour quitter.", panelX + 20, panelY + panelH - 40, 18, RAYWHITE);
}

Color RaylibRenderer::colorForSymbol(char symbol) const {
    const int index = std::max(0, symbol - 'A');
    const float hue = std::fmod(static_cast<float>(index) * 45.0f, 360.0f);
    return ColorFromHSV(hue, 0.65f, 0.85f);
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

void RaylibRenderer::flash(const std::string& text, float seconds) {
    std::lock_guard<std::mutex> lock(stateMutex);
    feedbackMessage = text;
    feedbackTimer   = seconds;
}

bool RaylibRenderer::confirmYesNo(const std::string& question, const char* yesLabel, const char* noLabel) {
    {
        std::lock_guard<std::mutex> lock(stateMutex);
        confirm.active    = true;
        confirm.question  = question;
        confirm.yes       = yesLabel ? yesLabel : "Oui";
        confirm.no        = noLabel  ? noLabel  : "Non";
        confirm.answered  = false;
        mode              = Mode::Idle; // let overlay render atop normal screen
    }

    std::unique_lock<std::mutex> lock(stateMutex);
    placementCv.wait(lock, [&](){ return !running.load() || confirm.answered; });

    bool result = confirm.answerYes;
    confirm.active = false;
    confirm.answered = false;
    return result;
}
