#ifndef L4_TEST_SUPPORT_H
#define L4_TEST_SUPPORT_H
// 先保留 raylib 原声明；只替换设备边界，不替换碰撞/Fade/TextFormat。
#include "raylib.h"
#include <string>
#include <vector>
struct DrawCall { std::string kind, text; Rectangle rect; Color color; int depth; };
extern std::vector<DrawCall> drawCalls;
extern int modeDepth, modeErrors;
extern bool keyDown[512], keyPressed[512];
extern int audioLoads, audioStarts, audioUpdates, audioPauses, audioResumes;
extern int audioUnloads, audioShots, audioHits, audioHurts;
extern bool audioLoadSucceeds;
void ResetFakes();
void FakeBeginDrawing();
void FakeEndDrawing();
void FakeClearBackground(Color);
void FakeBeginMode2D(Camera2D);
void FakeEndMode2D();
void FakeDrawRectangleRec(Rectangle, Color);
void FakeDrawTexturePro(Texture2D, Rectangle, Rectangle, Vector2, float, Color);
void FakeDrawRectangleLinesEx(Rectangle, float, Color);
void FakeDrawRectangle(int, int, int, int, Color);
void FakeDrawLine(int, int, int, int, Color);
void FakeDrawText(const char*, int, int, int, Color);
bool FakeKeyDown(int);
bool FakeKeyPressed(int);
namespace AudioBackend {
bool Load();
void Start(); void Update(); void Pause(); void Resume(); void Unload();
void Shot(); void Hit(); void Hurt();
}
#define BeginDrawing FakeBeginDrawing
#define EndDrawing FakeEndDrawing
#define ClearBackground FakeClearBackground
#define BeginMode2D FakeBeginMode2D
#define EndMode2D FakeEndMode2D
#define DrawRectangleRec FakeDrawRectangleRec
#define DrawTexturePro FakeDrawTexturePro
#define DrawRectangleLinesEx FakeDrawRectangleLinesEx
#define DrawRectangle FakeDrawRectangle
#define DrawLine FakeDrawLine
#define DrawText FakeDrawText
#define IsKeyDown FakeKeyDown
#define IsKeyPressed FakeKeyPressed
#define L4_AUDIO_TEST
#endif
