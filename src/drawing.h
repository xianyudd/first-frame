#ifndef L4_DRAWING_H
#define L4_DRAWING_H
#include "my_cases.h"

void DrawFloor();
void DrawCase(CollisionCase test, int y);
void DrawCaseWorld(CollisionCase test);
bool PlayerVisible();
void DrawHealthBar();
void DrawHUD();
void DrawUpgradePanel();
void DrawResult();
void DrawGame();
#endif
