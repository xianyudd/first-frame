#include "raylib.h"
#include "game.h"
#include "drawing.h"
#include "audio.h"

int main() {
    InitWindow(SCREEN_W, SCREEN_H, "Mini Roguelike - Survive and Upgrade");
    SetTargetFPS(60);
    LoadAssets();
    LoadGameAudio();
    InitGame();
    while (!WindowShouldClose()) {
        if (IsKeyPressed(KEY_TAB)) {
            practiceMode = !practiceMode;
            InitGame();
        }
        if (IsKeyPressed(KEY_G)) debugMode = !debugMode;
        if (IsKeyPressed(KEY_ENTER)) InitGame();
        float dt = GetFrameTime();
        UpdateGame(dt);
        UpdateCamera();
        UpdateGameAudio(GameEnded());
        DrawGame();
    }
    UnloadGameAudio();
    UnloadAssets();
    CloseWindow();
    return 0;
}
