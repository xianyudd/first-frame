#ifndef L4_ASSETS_H
#define L4_ASSETS_H
#include "raylib.h"

enum class SpriteId { Player, Bullet, Grunt, Runner, Heavy, Elite, Floor, Wall, COUNT };
inline Texture2D g_tex[(int)SpriteId::COUNT];
inline Rectangle g_crop[(int)SpriteId::COUNT];
inline const char* g_paths[(int)SpriteId::COUNT] = {
    "assets/player.png", "assets/bullet.png", "assets/grunt.png", "assets/runner.png",
    "assets/heavy.png", "assets/elite.png", "assets/floor.png", "assets/wall.png"
};
inline void LoadAssets() {
    for (int i = 0; i < (int)SpriteId::COUNT; ++i) {
        Image image = LoadImage(g_paths[i]);
        if (!image.data) continue;
        g_crop[i] = GetImageAlphaBorder(image, 0.1f);
        if (g_crop[i].width <= 0 || g_crop[i].height <= 0)
            g_crop[i] = {0, 0, (float)image.width, (float)image.height};
        g_tex[i] = LoadTextureFromImage(image);
        UnloadImage(image);
        if (g_tex[i].id) SetTextureFilter(g_tex[i], TEXTURE_FILTER_BILINEAR);
    }
}
inline void DrawSprite(SpriteId id, Rectangle box) {
    Texture2D texture = g_tex[(int)id];
    if (!texture.id) { DrawRectangleRec(box, MAGENTA); return; }
    Rectangle source = g_crop[(int)id];
    if (source.width <= 0 || source.height <= 0)
        source = {0, 0, (float)texture.width, (float)texture.height};
    float sx = box.width / source.width, sy = box.height / source.height;
    float scale = sx < sy ? sx : sy;
    Rectangle destination = {
        box.x + (box.width - source.width * scale) / 2,
        box.y + (box.height - source.height * scale) / 2,
        source.width * scale, source.height * scale
    };
    DrawTexturePro(texture, source, destination, {0, 0}, 0, WHITE);
}
inline void UnloadAssets() {
    for (int i = 0; i < (int)SpriteId::COUNT; ++i) {
        if (g_tex[i].id) UnloadTexture(g_tex[i]);
        g_tex[i] = {};
    }
}
#endif
