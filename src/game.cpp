#include "game.h"
#include "audio.h"

Camera2D camera = {{0, 0}, {0, 0}, 0, 1};
int wall[WORLD_ROWS][WORLD_COLS];
Player player;
Bullet bullets[MAX_BULLETS];
Enemy enemies[MAX_ENEMIES];
int score, spawnTimer, spawnSequence;
int kills;
float survivalTime, shotCooldown;
GameState gameState;
UpgradeKind upgradeChoices[3];
bool practiceMode = true;
bool debugMode = false;

// 沿用第 2 课的怪物配置。
EnemyConfig ConfigOf(EnemyKind kind) {
    switch (kind) {
        case EnemyKind::Grunt: return {SpriteId::Grunt, 1.8f, 1, 10, 40};
        case EnemyKind::Runner: return {SpriteId::Runner, 3.4f, 1, 15, 34};
        case EnemyKind::Heavy: return {SpriteId::Heavy, 1.0f, 3, 30, 48};
        case EnemyKind::Elite: return {SpriteId::Elite, 2.0f, 6, 80, 56};
    }
    return {SpriteId::Grunt, 1.8f, 1, 10, 40};
}
EnemyKind NextKind() {
    static const EnemyKind sequence[] = {EnemyKind::Grunt, EnemyKind::Runner,
        EnemyKind::Grunt, EnemyKind::Heavy, EnemyKind::Runner, EnemyKind::Elite};
    EnemyKind kind = sequence[spawnSequence % 6];
    spawnSequence = (spawnSequence + 1) % 6;
    return kind;
}

Rectangle CellRect(int row, int col) {
    // 格子行列转换为世界矩形。
    return {(float)(col * TILE), (float)(row * TILE), (float)TILE, (float)TILE};
}
bool HitsWall(Rectangle rect) {
    // 整个身体与墙格判断重叠。
    if (rect.width <= 0 || rect.height <= 0) return false;
    for (int row = 0; row < WORLD_ROWS; ++row)
        for (int col = 0; col < WORLD_COLS; ++col)
            if (wall[row][col] && CheckCollisionRecs(rect, CellRect(row, col))) return true;
    return false;
}
bool InsideWorld(Rectangle rect) {
    // 整个身体必须在世界内。
    return rect.x >= 0 && rect.y >= 0 && rect.width >= 0 && rect.height >= 0 &&
        rect.x + rect.width <= WORLD_W && rect.y + rect.height <= WORLD_H;
}
void InitCamera() {
    // 镜头对准玩家身体中心。
    camera.offset = {SCREEN_W / 2.0f, SCREEN_H / 2.0f};
    camera.target = {player.rect.x + player.rect.width / 2,
                     player.rect.y + player.rect.height / 2};
    camera.rotation = 0;
    camera.zoom = 1;
}
void UpdateCamera() {
    // 每帧跟随玩家中心。
    camera.target = {player.rect.x + player.rect.width / 2,
                     player.rect.y + player.rect.height / 2};
}
void InitMap() {
    for (int row = 0; row < WORLD_ROWS; ++row)
        for (int col = 0; col < WORLD_COLS; ++col) wall[row][col] = 0;
    wall[3][7] = 1;
    wall[8][10] = 1;
    wall[12][24] = 1;
    for (int row = 5; row <= 11; ++row) wall[row][20] = 1;
    for (int col = 5; col <= 11; ++col) wall[13][col] = 1;
    // 远处墙需要走出初始视野探索。
    wall[20][40] = 1;
    for (int row = 22; row <= 28; ++row) wall[row][45] = 1;

    for (int row = 10; row <= 106; row += 32)
        for (int col = 36; col <= 164; col += 32) {
            for (int offset = 0; offset <= 18; ++offset) {
                if (offset < 8 || offset > 11) wall[row][col + offset] = 1;
                wall[row + 10][col + offset] = 1;
            }
            for (int offset = 1; offset < 10; ++offset)
                if (offset < 4 || offset > 6) wall[row + offset][col + 18] = 1;
        }
}
void InitGame() {
    InitMap();
    player = {{SCREEN_W / 2.0f - 20, SCREEN_H / 2.0f - 20, 40, 40},
        5, INITIAL_HP, INITIAL_HP, 0, INITIAL_ATTACK, SHOT_INTERVAL, 0, 1};
    for (int i = 0; i < MAX_BULLETS; ++i) bullets[i] = {};
    for (int i = 0; i < MAX_ENEMIES; ++i)
        enemies[i] = {{0, 0, 0, 0}, EnemyKind::Grunt, 0, false};
    score = 0;
    kills = 0;
    spawnTimer = 0;
    spawnSequence = 0;
    survivalTime = 0;
    shotCooldown = 0;
    gameState = GameState::Playing;
    upgradeChoices[0] = UpgradeKind::Attack;
    upgradeChoices[1] = UpgradeKind::FireRate;
    upgradeChoices[2] = UpgradeKind::MaxHp;
    InitCamera();
}
void UpdatePlayer() {
    // 先尝试 X，再从已接受的 X 尝试 Y。
    float dx = (IsKeyDown(KEY_D) - IsKeyDown(KEY_A)) * player.speed;
    float dy = (IsKeyDown(KEY_S) - IsKeyDown(KEY_W)) * player.speed;
    Rectangle next = player.rect;
    next.x += dx;
    if (InsideWorld(next) && !HitsWall(next)) player.rect = next;
    next = player.rect;
    next.y += dy;
    if (InsideWorld(next) && !HitsWall(next)) player.rect = next;
}
void FireBullets() {
    // TODO(L4-05-C): 冷却尚未归零时不能创建新子弹。
    int vx = 0, vy = 0;
    if (IsKeyDown(KEY_RIGHT)) vx = 9;
    else if (IsKeyDown(KEY_LEFT)) vx = -9;
    else if (IsKeyDown(KEY_UP)) vy = -9;
    else if (IsKeyDown(KEY_DOWN)) vy = 9;
    if (vx == 0 && vy == 0) return;
    for (int i = 0; i < MAX_BULLETS; ++i) {
        if (bullets[i].active) continue;
        bullets[i].rect = {player.rect.x + player.rect.width / 2 - 5,
            player.rect.y + player.rect.height / 2 - 5, 10, 10};
        bullets[i].vx = (float)vx;
        bullets[i].vy = (float)vy;
        bullets[i].active = true;
        // TODO(L4-05-D): 成功创建时记录攻击快照并设置冷却；池满不要设置。
        bullets[i].damage = INITIAL_ATTACK;
        PlayShotSound();
        return;
    }
}
void UpdateBullets() {
    for (int i = 0; i < MAX_BULLETS; ++i) {
        if (!bullets[i].active) continue;
        bullets[i].rect.x += bullets[i].vx;
        bullets[i].rect.y += bullets[i].vy;
        if (!InsideWorld(bullets[i].rect) || HitsWall(bullets[i].rect))
            bullets[i].active = false;
    }
}
void SpawnEnemies() {
    if (practiceMode) return;
    if (++spawnTimer < 55) return;
    spawnTimer = 0;
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (enemies[i].active) continue;
        const int previousSequence = spawnSequence;
        EnemyKind kind = NextKind();
        EnemyConfig cfg = ConfigOf(kind);
        // 沿用近处环形刷怪和原有配置顺序。
        int first = GetRandomValue(0, 142);
        int playerCol = (int)(player.rect.x / TILE);
        int playerRow = (int)(player.rect.y / TILE);
        for (int offset = 0; offset < 143; ++offset) {
            int cell = (first + offset) % 143;
            int dx = cell % 13 - 6, dy = cell / 13 - 5;
            if (dx != -6 && dx != 6 && dy != -5 && dy != 5) continue;
            int col = playerCol + dx, row = playerRow + dy;
            Rectangle box = {(float)(col * TILE), (float)(row * TILE), cfg.size, cfg.size};
            if (col < 0 || row < 0 || box.x + box.width > WORLD_W ||
                box.y + box.height > WORLD_H || CheckCollisionRecs(box, player.rect)) continue;
            // 刷怪身体覆盖的墙格也要检查。
            bool blocked = false;
            int cells = cfg.size > TILE ? 2 : 1;
            for (int r = row; r < row + cells; ++r)
                for (int c = col; c < col + cells; ++c)
                    if (wall[r][c]) blocked = true;
            if (blocked || HitsWall(box)) continue;
            enemies[i] = {box, kind, cfg.maxHp, true};
            return;
        }
        spawnSequence = previousSequence; // 成功刷怪才消耗配置顺序。
        return;
    }
}
void UpdateEnemies(float) {
    for (int i = 0; i < MAX_ENEMIES; ++i) {
        if (!enemies[i].active) continue;
        float speed = ConfigOf(enemies[i].kind).speed;
        Rectangle next = enemies[i].rect;
        if (player.rect.x > next.x) next.x += speed;
        else if (player.rect.x < next.x) next.x -= speed;
        if (InsideWorld(next) && !HitsWall(next)) enemies[i].rect = next;
        next = enemies[i].rect;
        if (player.rect.y > next.y) next.y += speed;
        else if (player.rect.y < next.y) next.y -= speed;
        if (InsideWorld(next) && !HitsWall(next)) enemies[i].rect = next;
        if (CheckCollisionRecs(enemies[i].rect, player.rect)) {
            TakeContactDamage();
            if (gameState == GameState::Lost) return;
        }
    }
}
void HandleHits() {
    for (int i = 0; i < MAX_BULLETS; ++i) {
        if (!bullets[i].active) continue;
        for (int j = 0; j < MAX_ENEMIES; ++j) {
            if (!enemies[j].active) continue;
            if (CheckCollisionRecs(bullets[i].rect, enemies[j].rect)) {
                bullets[i].active = false;
                enemies[j].hp -= bullets[i].damage;
                // 每次实际命中都播放声音。
                PlayHitSound();
                if (enemies[j].hp <= 0) {
                    enemies[j].active = false;
                    score += ConfigOf(enemies[j].kind).score;
                    RewardKill(enemies[j].kind);
                }
                break;
            }
        }
    }
}

void UpdateProtection(float dt) {
    // TODO(L4-02-A): 保护剩余秒数只减 dt 一次，最低为 0。
    (void)dt;
}
void TakeContactDamage() {
    // TODO(L4-02-B): 未受保护的实际接触扣 10 HP、开启保护并播放受伤声；致命进入 Lost。
}
void UpdateSurvival(float dt) {
    // TODO(L4-03-A): 只在战斗累加生存秒数，并封顶 SURVIVAL_SECONDS。
    (void)dt;
}
void CheckWin() {
    // TODO(L4-03-B): 只有仍在 Playing 的战斗模式到时才进入 Won，不能覆盖 Lost。
}
void RewardKill(EnemyKind kind) {
    // TODO(L4-04-A): 只在死亡结算增加击败数和 XP_PER_KILL 经验。
    (void)kind;
}
int ExperienceNeeded() { return XP_PER_LEVEL; }
void FillUpgradeChoices(UpgradeKind out[3]) {
    UpgradeKind pool[] = {UpgradeKind::Attack, UpgradeKind::FireRate,
        UpgradeKind::MaxHp, UpgradeKind::Heal};
    // Fisher-Yates：打乱四种，再取前三种，保证不同。
    for (int i = 3; i > 0; --i) {
        int j = GetRandomValue(0, i);
        UpgradeKind saved = pool[i];
        pool[i] = pool[j];
        pool[j] = saved;
    }
    for (int i = 0; i < 3; ++i) out[i] = pool[i];
}
void BeginUpgrade() {
    // TODO(L4-04-B): 检查状态、等级上限和经验阈值；保留余量并升一级。
    // TODO(L4-05-A): 本次升级成立后抽取三个候选，进入 ChoosingUpgrade。
}
void ApplyUpgrade(int index) {
    // TODO(L4-05-B): 只接受选择状态中的合法索引，应用一次效果，再回到 Playing。
    (void)index;
}
const char* UpgradeName(UpgradeKind kind) {
    switch (kind) {
        case UpgradeKind::Attack: return "Attack";
        case UpgradeKind::FireRate: return "Fire rate";
        case UpgradeKind::MaxHp: return "Maximum HP";
        case UpgradeKind::Heal: return "Heal";
    }
    return "Unknown";
}
const char* UpgradeDescription(UpgradeKind kind) {
    switch (kind) {
        case UpgradeKind::Attack: return "Attack +1 (new bullets)";
        case UpgradeKind::FireRate: return "Shot interval x0.8 (minimum 0.12s)";
        case UpgradeKind::MaxHp: return "Maximum HP +20 and current HP +20";
        case UpgradeKind::Heal: return "Current HP +30 (up to maximum)";
    }
    return "";
}
bool GameEnded() { return gameState == GameState::Won || gameState == GameState::Lost; }
void UpdateGame(float dt) {
    if (GameEnded()) return;
    if (gameState == GameState::ChoosingUpgrade) {
        if (IsKeyPressed(KEY_ONE)) ApplyUpgrade(0);
        else if (IsKeyPressed(KEY_TWO)) ApplyUpgrade(1);
        else if (IsKeyPressed(KEY_THREE)) ApplyUpgrade(2);
        // 选择这一帧不恢复模拟，也不立刻消费下一次升级。
        return;
    }
    if (dt < 0) dt = 0;
    UpdateSurvival(dt);
    UpdateProtection(dt);
    shotCooldown -= dt;
    if (shotCooldown < 0) shotCooldown = 0;
    UpdatePlayer();
    FireBullets();
    UpdateBullets();
    SpawnEnemies();
    UpdateEnemies(dt);
    if (gameState == GameState::Lost) return;
    HandleHits();
    CheckWin();
    if (gameState == GameState::Playing) BeginUpgrade();
}
