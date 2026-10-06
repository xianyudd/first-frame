#include "drawing.h"
#include "game.h"
#include "audio.h"
#include "my_experiment.h"

void DrawFloor() {
    for (int row = 0; row < WORLD_ROWS; ++row)
        for (int col = 0; col < WORLD_COLS; ++col) {
            Rectangle box = {(float)(col * TILE), (float)(row * TILE), (float)TILE, (float)TILE};
            DrawSprite(SpriteId::Floor, box);
            if (wall[row][col]) DrawSprite(SpriteId::Wall, box);
        }
}
void DrawCase(CollisionCase test, int y) {
    if (!test.enabled) {
        DrawText("My case: not set (edit src/my_cases.h)", 16, y, 18, GRAY);
        return;
    }
    bool actual = HitsWall(test.rect);
    DrawText(TextFormat("%s: expected %s / actual %s", test.name,
        test.expected ? "wall" : "clear", actual ? "wall" : "clear"),
        16, y, 18, actual == test.expected ? DARKGREEN : MAROON);
}
void DrawCaseWorld(CollisionCase test) {
    if (debugMode && test.enabled) DrawRectangleLinesEx(test.rect, 2, ORANGE);
}
bool PlayerVisible() {
    // TODO(L4-02-C): 保护期间按剩余秒数交替显示；无保护时始终显示。
    return true;
}
void DrawHealthBar() {
    DrawRectangle(16, 66, 200, 16, GRAY);
    // TODO(L4-01): 用 hp / maxHp 的浮点比例绘制绿色前景，宽度 0..200。
}
void DrawHUD() {
    DrawRectangle(0, 0, SCREEN_W, 140, Fade(RAYWHITE, 0.92f));
    DrawText(TextFormat("HP: %d / %d   Score: %d   Kills: %d   Mode: %s", player.hp,
        player.maxHp, score, kills, practiceMode ? "PRACTICE" : "COMBAT"), 16, 10, 22, DARKGRAY);
    DrawText("WASD move | Hold arrows shoot | TAB practice/combat | G grid | ENTER restart", 16, 38, 18, DARKGRAY);
    DrawHealthBar();
    DrawText(TextFormat("Time: %.1f / %.0f s   Level: %d / %d   Attack: %d   Shot interval: %.2f s",
        survivalTime, SURVIVAL_SECONDS, player.level, MAX_LEVEL, player.attack, player.shotInterval), 16, 92, 20, DARKGRAY);
    if (player.level >= MAX_LEVEL) DrawText("XP: MAX LEVEL (no more upgrade choices)", 16, 116, 18, DARKGREEN);
    else DrawText(TextFormat("XP: %d / %d", player.xp, ExperienceNeeded()), 16, 116, 18, DARKGRAY);
    DrawRectangle(0, SCREEN_H - 74, SCREEN_W, 74, Fade(RAYWHITE, 0.92f));
    DrawCase(myCase, SCREEN_H - 68);
    DrawText(myExperiment.recorded ? "Task 06: experiment recorded; teacher review required" :
        "Task 06: record your experiment in src/my_experiment.h (manual review)", 16, SCREEN_H - 44, 18, DARKGRAY);
    if (!GameAudio::ready) DrawText("Audio unavailable: check device and assets/audio WAV files", 16, SCREEN_H - 22, 18, MAROON);
}
void DrawUpgradePanel() {
    if (gameState != GameState::ChoosingUpgrade) return;
    DrawRectangle(180, 190, 920, 350, Fade(RAYWHITE, 0.98f));
    DrawText("LEVEL UP - Choose one (1 / 2 / 3)", 220, 215, 30, DARKGREEN);
    DrawText("Simulation paused; music continues", 220, 258, 20, DARKGRAY);
    for (int i = 0; i < 3; ++i) {
        int y = 300 + i * 70;
        DrawText(TextFormat("%d. %s", i + 1, UpgradeName(upgradeChoices[i])), 220, y, 24, DARKGRAY);
        DrawText(UpgradeDescription(upgradeChoices[i]), 260, y + 28, 18, DARKGRAY);
    }
}
void DrawResult() {
    if (!GameEnded()) return;
    DrawRectangle(330, 230, 620, 260, Fade(RAYWHITE, 0.98f));
    DrawText(gameState == GameState::Won ? "YOU SURVIVED" : "GAME OVER", 390, 255, 42,
        gameState == GameState::Won ? DARKGREEN : MAROON);
    DrawText(TextFormat("Survival: %.1f s   Kills: %d", survivalTime, kills), 390, 318, 24, DARKGRAY);
    DrawText(TextFormat("Level: %d   Score: %d", player.level, score), 390, 354, 24, DARKGRAY);
    DrawText("Press ENTER to restart", 390, 414, 26, DARKGRAY);
}
void DrawGame() {
    // 世界绘制放在镜头内，HUD 使用屏幕坐标。
    BeginDrawing();
    ClearBackground(RAYWHITE);
    BeginMode2D(camera);
    DrawFloor();
    if (debugMode) {
        for (int col = 0; col <= WORLD_COLS; ++col)
            DrawLine(col * TILE, 0, col * TILE, WORLD_H, Fade(DARKGRAY, 0.35f));
        for (int row = 0; row <= WORLD_ROWS; ++row)
            DrawLine(0, row * TILE, WORLD_W, row * TILE, Fade(DARKGRAY, 0.35f));
    }
    if (PlayerVisible()) DrawSprite(SpriteId::Player, player.rect);
    if (debugMode) DrawRectangleLinesEx(player.rect, 2, BLUE);
    for (int i = 0; i < MAX_BULLETS; ++i)
        if (bullets[i].active) {
            DrawSprite(SpriteId::Bullet, bullets[i].rect);
            if (debugMode) DrawRectangleLinesEx(bullets[i].rect, 1, RED);
        }
    for (int i = 0; i < MAX_ENEMIES; ++i)
        if (enemies[i].active) {
            DrawSprite(ConfigOf(enemies[i].kind).sprite, enemies[i].rect);
            if (debugMode) DrawRectangleLinesEx(enemies[i].rect, 2, RED);
        }
    DrawCaseWorld({"Inside wall", {280, 120, 40, 40}, true, true});
    DrawCaseWorld({"Body overlaps", {250, 120, 40, 40}, true, true});
    DrawCaseWorld(myCase);
    EndMode2D();
    DrawHUD();
    DrawUpgradePanel();
    DrawResult();
    EndDrawing();
}
