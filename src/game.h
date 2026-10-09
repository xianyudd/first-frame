#ifndef L4_GAME_H
#define L4_GAME_H
#include "raylib.h"
#include "assets.h"

struct Player {
    Rectangle rect;
    float speed;
    int hp, maxHp;
    float invulnerability;
    int attack;
    float shotInterval;
    int xp, level;
};
struct Bullet { Rectangle rect; float vx, vy; bool active; int damage; };
enum class EnemyKind { Grunt, Runner, Heavy, Elite };
struct EnemyConfig { SpriteId sprite; float speed; int maxHp; int score; float size; };
struct Enemy { Rectangle rect; EnemyKind kind; int hp; bool active; };
enum class GameState { Playing, ChoosingUpgrade, Won, Lost };
enum class UpgradeKind { Attack, FireRate, MaxHp, Heal };

inline constexpr int MAX_BULLETS = 128, MAX_ENEMIES = 64;
inline constexpr int SCREEN_W = 1280, SCREEN_H = 720, TILE = 40;
inline constexpr int WORLD_ROWS = 120, WORLD_COLS = 200;
inline constexpr int WORLD_W = WORLD_COLS * TILE, WORLD_H = WORLD_ROWS * TILE;
// 这些是待试玩调整的初版数值，时间单位均为秒。
inline constexpr int INITIAL_HP = 100, INITIAL_ATTACK = 1;
inline constexpr int MAX_LEVEL = 6, XP_PER_KILL = 10, XP_PER_LEVEL = 100;
inline constexpr float SURVIVAL_SECONDS = 180.0f, INVUL_SECONDS = 0.8f;
inline constexpr float SHOT_INTERVAL = 0.30f, MIN_SHOT_INTERVAL = 0.12f;

// 普通游戏固定使用这些默认值；06 评分器仅在一个作用域内临时替换参数。
struct UpgradeParameters {
    int healAmount = 30;
    float fireRateMultiplier = 0.8f;
};
inline constexpr UpgradeParameters DEFAULT_UPGRADE_PARAMETERS{};
const UpgradeParameters& GetUpgradeParameters();
class ScopedUpgradeParameters {
public:
    explicit ScopedUpgradeParameters(UpgradeParameters parameters);
    ~ScopedUpgradeParameters();
    ScopedUpgradeParameters(const ScopedUpgradeParameters&) = delete;
    ScopedUpgradeParameters& operator=(const ScopedUpgradeParameters&) = delete;
private:
    UpgradeParameters previous;
};

// 唯一定义在所选的 game.cpp；学生与教师实现不能同时链接。
extern Camera2D camera;
extern int wall[WORLD_ROWS][WORLD_COLS];
extern Player player;
extern Bullet bullets[MAX_BULLETS];
extern Enemy enemies[MAX_ENEMIES];
extern int score, kills, spawnTimer, spawnSequence;
extern float survivalTime, shotCooldown;
extern GameState gameState;
extern UpgradeKind upgradeChoices[3];
extern bool practiceMode, debugMode;

EnemyConfig ConfigOf(EnemyKind kind);
EnemyKind NextKind();
Rectangle CellRect(int row, int col);
bool HitsWall(Rectangle rect);
bool InsideWorld(Rectangle rect);
void InitCamera();
void UpdateCamera();
void InitMap();
void InitGame();
void UpdatePlayer();
void FireBullets();
void UpdateBullets();
void SpawnEnemies();
void UpdateEnemies(float dt);
void HandleHits();
void UpdateProtection(float dt);
void TakeContactDamage();
void UpdateSurvival(float dt);
void CheckWin();
void RewardKill(EnemyKind kind);
int ExperienceNeeded();
void FillUpgradeChoices(UpgradeKind out[3]);
void BeginUpgrade();
void ApplyUpgrade(int index);
const char* UpgradeName(UpgradeKind kind);
const char* UpgradeDescription(UpgradeKind kind);
bool GameEnded();
void UpdateGame(float dt);
#endif
