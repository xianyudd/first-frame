#include "test_support.h"
#include <algorithm>
std::vector<DrawCall> drawCalls;
int modeDepth = 0, modeErrors = 0;
bool keyDown[512]{}, keyPressed[512]{};
int audioLoads = 0, audioStarts = 0, audioUpdates = 0, audioPauses = 0;
int audioResumes = 0, audioUnloads = 0, audioShots = 0, audioHits = 0, audioHurts = 0;
bool audioLoadSucceeds = true;
static void Record(const char* kind, Rectangle rect = {}, Color color = {}, const char* text = "") {
    drawCalls.push_back({kind, text, rect, color, modeDepth});
}
void ResetFakes() {
    std::fill_n(keyDown, 512, false); std::fill_n(keyPressed, 512, false);
    drawCalls.clear(); modeDepth = modeErrors = 0;
    audioLoads = audioStarts = audioUpdates = audioPauses = audioResumes = 0;
    audioUnloads = audioShots = audioHits = audioHurts = 0;
    audioLoadSucceeds = true;
}
void FakeBeginDrawing() { Record("begin"); }
void FakeEndDrawing() { if (modeDepth != 0) ++modeErrors; Record("end"); }
void FakeClearBackground(Color c) { Record("clear", {}, c); }
void FakeBeginMode2D(Camera2D) { if (modeDepth != 0) ++modeErrors; ++modeDepth; Record("world-begin"); }
void FakeEndMode2D() { if (modeDepth != 1) ++modeErrors; --modeDepth; Record("world-end"); }
void FakeDrawRectangleRec(Rectangle r, Color c) { Record("sprite", r, c); }
void FakeDrawTexturePro(Texture2D, Rectangle, Rectangle r, Vector2, float, Color c) { Record("sprite", r, c); }
void FakeDrawRectangleLinesEx(Rectangle r, float, Color c) { Record("outline", r, c); }
void FakeDrawRectangle(int x, int y, int w, int h, Color c) { Record("rect", {float(x), float(y), float(w), float(h)}, c); }
void FakeDrawLine(int x, int y, int ex, int ey, Color c) { Record("line", {float(x), float(y), float(ex), float(ey)}, c); }
void FakeDrawText(const char* t, int x, int y, int size, Color c) { Record("text", {float(x), float(y), 0, float(size)}, c, t ? t : ""); }
bool FakeKeyDown(int k) { return k >= 0 && k < 512 && keyDown[k]; }
bool FakeKeyPressed(int k) { return k >= 0 && k < 512 && keyPressed[k]; }
namespace AudioBackend {
bool Load() { ++audioLoads; return audioLoadSucceeds; }
void Start() { ++audioStarts; } void Update() { ++audioUpdates; }
void Pause() { ++audioPauses; } void Resume() { ++audioResumes; }
void Unload() { ++audioUnloads; } void Shot() { ++audioShots; }
void Hit() { ++audioHits; } void Hurt() { ++audioHurts; }
}
