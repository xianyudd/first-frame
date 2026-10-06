#ifndef L4_MY_CASES_H
#define L4_MY_CASES_H
#include "raylib.h"
struct CollisionCase {
    const char* name;
    Rectangle rect;
    bool expected;
    bool enabled;
};
// 保留第 3 课的自选碰撞案例，默认未启用。
inline CollisionCase myCase = {"My case", {0, 0, 0, 0}, false, false};
#endif
