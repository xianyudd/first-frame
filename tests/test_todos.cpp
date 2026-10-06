#include "test_support.h"
#include "game.h"
#include "drawing.h"
#include "audio.h"
#include <cmath>
#include <cstdio>
#include <cstring>

static bool verbose = false;
static int group = 0, passed[6]{}, total[6]{};
static bool Near(float a, float b) { return std::fabs(a - b) < 0.0001f; }
static void RecordCheck(bool ok, const char* name, int line, const char* expected) {
    ++total[group]; passed[group] += ok;
    if (!ok) {
        std::printf("[FAIL] %02d %s (test_todos.cpp:%d) expected: %s; actual: "
            "HP=%d/%d protection=%.3f time=%.3f state=%d XP=%d level=%d "
            "attack=%d interval=%.3f kills=%d cooldown=%.3f hurt=%d shot=%d",
            group, name, line, expected, player.hp, player.maxHp, player.invulnerability,
            survivalTime, int(gameState), player.xp, player.level, player.attack,
            player.shotInterval, kills, shotCooldown, audioHurts, audioShots);
        if (group==1) {
            bool found=false;
            for (const auto& c:drawCalls) {
                if (c.kind=="rect" && Near(c.rect.x,16) && Near(c.rect.y,66) &&
                    c.color.r==GREEN.r && c.color.g==GREEN.g && c.color.b==GREEN.b) {
                    std::printf(" foregroundWidth=%.1f",c.rect.width); found=true;
                }
            }
            if (!found) std::printf(" foregroundWidth=missing");
        }
        std::printf("\n");
    } else if (verbose) std::printf("[PASS] %02d %s\n",group,name);
}
// 行号及期望来自调用处；实测只读当前夹具，不更改输入或游戏状态。
#define Check(condition, name) RecordCheck((condition), (name), __LINE__, #condition)
// 每个独立夹具只初始化一次，然后施加受控输入；不注入生产规则答案。
static void Reset() {
    UnloadGameAudio(); ResetFakes(); practiceMode = true; debugMode = false;
    InitGame(); LoadGameAudio();
}
static void Contact(int slot = 0) { enemies[slot] = {player.rect, EnemyKind::Grunt, 1, true}; }
static bool SameRect(Rectangle a, Rectangle b) {
    return Near(a.x,b.x) && Near(a.y,b.y) && Near(a.width,b.width) && Near(a.height,b.height);
}
static bool SamePlayer(Player a, Player b) {
    return SameRect(a.rect,b.rect) && Near(a.speed,b.speed) && a.hp==b.hp && a.maxHp==b.maxHp &&
        Near(a.invulnerability,b.invulnerability) && a.attack==b.attack &&
        Near(a.shotInterval,b.shotInterval) && a.xp==b.xp && a.level==b.level;
}
static bool Bar(int width) {
    int bg = -1, fg = -1;
    for (unsigned i=0; i<drawCalls.size(); ++i) {
        const auto& c=drawCalls[i];
        if (c.kind!="rect" || c.depth!=0 || !Near(c.rect.x,16) || !Near(c.rect.y,66) || !Near(c.rect.height,16)) continue;
        if (c.color.r==GRAY.r && c.color.g==GRAY.g && c.color.b==GRAY.b && Near(c.rect.width,200)) bg=int(i);
        if (c.color.r==GREEN.r && c.color.g==GREEN.g && c.color.b==GREEN.b && Near(c.rect.width,float(width))) fg=int(i);
    }
    return bg>=0 && fg>bg;
}
static bool Layering() {
    int endWorld=-1, hud=-1, firstText=-1, bar=-1, panel=-1, panelText=-1;
    for (unsigned i=0;i<drawCalls.size();++i) {
        const auto& c=drawCalls[i];
        if (c.kind=="world-end") endWorld=int(i);
        if (c.kind=="rect" && SameRect(c.rect,{0,0,float(SCREEN_W),140})) hud=int(i);
        if (c.kind=="text" && c.text.find("HP:")==0) firstText=int(i);
        if (c.kind=="rect" && Near(c.rect.y,66)) bar=int(i);
        if (c.kind=="rect" && Near(c.rect.x,180) && Near(c.rect.y,190)) panel=int(i);
        if (c.kind=="text" && c.text.find("LEVEL UP")==0) panelText=int(i);
        if ((c.kind=="text" || (c.kind=="rect" && Near(c.rect.y,66))) && c.depth!=0) return false;
    }
    return modeErrors==0 && endWorld>=0 && hud>endWorld && firstText>hud && bar>hud && panel>bar && panelText>panel;
}
static void Task01() {
    group=1;
    const int hp[]={100,50,0,-4,150,1,20,20};
    const int maxHp[]={100,100,100,100,100,3,0,-1};
    const int width[]={200,100,0,0,200,66,0,0};
    const char* names[]={"full bar","half bar","empty bar","negative HP clamped","excess HP clamped","fractional ratio","zero denominator","negative denominator"};
    for (int i=0;i<8;++i) { Reset(); player.hp=hp[i]; player.maxHp=maxHp[i]; DrawHealthBar(); Check(Bar(width[i]),names[i]); }
    Reset(); player.hp=50; camera.target={5000,3000}; gameState=GameState::ChoosingUpgrade;
    DrawGame(); Check(Bar(100),"bar remains in screen coordinates"); Check(Layering(),"world/HUD/bar/choice overlay order");
}
static void Task02() {
    group=2; Reset(); player.invulnerability=0.6f; UpdateProtection(0.1f);
    Check(Near(player.invulnerability,0.5f),"protection decrements once"); UpdateProtection(0.5f);
    Check(Near(player.invulnerability,0),"protection exact zero"); UpdateProtection(1);
    Check(Near(player.invulnerability,0),"protection clamps zero");
    Reset(); player.hp=50; Contact(); Contact(1); UpdateEnemies(0.1f);
    Check(player.hp==40 && audioHurts==1,"multiple contacts one damage and hurt");
    Check(enemies[0].active && enemies[1].active,"contact does not consume enemies");
    Check(Near(player.invulnerability,0.8f),"contact starts 0.8s protection");
    UpdateEnemies(0.1f); Check(player.hp==40 && audioHurts==1,"protected contact silent");
    UpdateProtection(0.8f); UpdateEnemies(0); Check(player.hp==30 && audioHurts==2,"zero protection permits next contact");
    Reset(); player.invulnerability=0; Check(PlayerVisible(),"unprotected visible");
    player.invulnerability=0.15f; Check(!PlayerVisible(),"odd flash phase hidden");
    player.invulnerability=0.25f; Check(PlayerVisible(),"even flash phase visible");
    Reset(); player.hp=3; Contact(); enemies[1]={{100,100,40,40},EnemyKind::Grunt,1,true}; UpdateEnemies(0);
    Check(player.hp==0 && gameState==GameState::Lost && audioHurts==1,"fatal damage clamps HP and loses");
    Check(Near(enemies[1].rect.x,100),"fatal contact stops later enemies");
    Reset(); practiceMode=false; survivalTime=179.9f; player.hp=10; player.xp=100; Contact();
    enemies[1]={{100,100,40,40},EnemyKind::Grunt,1,true}; bullets[0]={{100,100,10,10},0,0,true,1};
    UpdateGame(0.2f); Check(gameState==GameState::Lost && player.level==1 && kills==0 && bullets[0].active,"fatal frame stops hits/win/upgrade");
}
static void Task03() {
    group=3; Reset(); UpdateSurvival(200); Check(Near(survivalTime,0),"practice does not accumulate");
    survivalTime=180; CheckWin(); Check(gameState==GameState::Playing,"practice cannot win");
    Reset(); practiceMode=false; UpdateSurvival(2.5f); Check(Near(survivalTime,2.5f),"combat uses seconds");
    survivalTime=179.9f; UpdateSurvival(1); Check(Near(survivalTime,180),"survival capped at 180");
    survivalTime=179.9f; CheckWin(); Check(gameState==GameState::Playing,"not won before deadline");
    survivalTime=180; CheckWin(); Check(gameState==GameState::Won,"deadline wins");
    gameState=GameState::Lost; CheckWin(); Check(gameState==GameState::Lost,"win never overwrites loss");
    Reset(); practiceMode=false; survivalTime=179.9f; player.xp=100; UpdateGame(0.2f);
    Check(gameState==GameState::Won && player.level==1 && player.xp==100,"frame victory before upgrade");
    Reset(); practiceMode=false; survivalTime=4; UpdateGame(-1); Check(Near(survivalTime,4),"negative frame dt clamped");
    for (GameState s:{GameState::Won,GameState::Lost}) {
        Reset(); practiceMode=false; gameState=s; player.xp=150; player.invulnerability=0.6f; shotCooldown=0.2f;
        bullets[0]={{100,100,10,10},9,0,true,2}; enemies[0]={{200,200,40,40},EnemyKind::Grunt,1,true};
        keyDown[KEY_D]=keyDown[KEY_RIGHT]=true; keyPressed[KEY_ONE]=true;
        Player before=player; UpdateGame(1);
        Check(SamePlayer(before,player) && Near(bullets[0].rect.x,100) && Near(enemies[0].rect.x,200) && spawnTimer==0 && Near(survivalTime,0) && Near(shotCooldown,0.2f) && audioShots==0 && gameState==s,"terminal whole simulation frozen");
        Check(GameEnded(),"terminal audio bool true"); UpdateGameAudio(GameEnded()); UpdateGameAudio(GameEnded());
        Check(audioPauses==1 && audioUpdates==2,"terminal audio pauses once");
    }
    Reset(); gameState=GameState::ChoosingUpgrade; Check(!GameEnded(),"choice audio bool false");
    UpdateGameAudio(GameEnded()); Check(audioPauses==0 && audioUpdates==1,"choice music continues");
    gameState=GameState::Lost; UpdateGameAudio(GameEnded()); InitGame(); UpdateGameAudio(GameEnded());
    Check(audioResumes==1 && !GameEnded(),"restart resumes music");
}
static void Task04() {
    group=4; Reset(); Check(ExperienceNeeded()==100,"fixed threshold 100");
    enemies[0]={{100,100,48,48},EnemyKind::Heavy,3,true}; bullets[0]={{105,105,10,10},0,0,true,1}; HandleHits();
    Check(kills==0 && player.xp==0 && enemies[0].hp==2 && audioHits==1,"nonfatal hit no kill or XP");
    bullets[0]={{105,105,10,10},0,0,true,2}; HandleHits();
    Check(kills==1 && player.xp==10 && !enemies[0].active && score==30 && audioHits==2,"actual death rewards once");
    HandleHits(); Check(kills==1 && player.xp==10,"inactive corpse not rewarded twice");
    Reset(); RewardKill(EnemyKind::Elite); Check(kills==1 && player.xp==10,"reward fixed XP independent of kind");
    Reset(); player.xp=99; BeginUpgrade(); Check(player.level==1 && player.xp==99,"below threshold unchanged");
    Reset(); player.xp=100; BeginUpgrade(); Check(player.level==2 && player.xp==0,"exact threshold advances level");
    Reset(); player.xp=250; BeginUpgrade(); Check(player.level==2 && player.xp==150,"excess XP retained");
    for (GameState s:{GameState::ChoosingUpgrade,GameState::Won,GameState::Lost}) {
        Reset(); gameState=s; player.xp=250; BeginUpgrade(); Check(player.level==1 && player.xp==250 && gameState==s,"only Playing consumes XP");
    }
    Reset(); player.level=6; player.xp=250; BeginUpgrade(); Check(player.level==6 && player.xp==250 && gameState==GameState::Playing,"max level no consumption");
    RewardKill(EnemyKind::Grunt); Check(player.xp==260,"max level still earns XP");
}
static void Task05() {
    group=5; Reset(); bool unique=true; bool seen[4]{}; SetRandomSeed(1701);
    for (int i=0;i<128;++i) {
        UpgradeKind choices[3]; FillUpgradeChoices(choices);
        for (int j=0;j<3;++j) { int k=int(choices[j]); if(k<0 || k>3) unique=false; else seen[k]=true; }
        unique=unique && choices[0]!=choices[1] && choices[0]!=choices[2] && choices[1]!=choices[2];
    }
    Check(unique,"candidates in range and unique"); Check(seen[0] && seen[1] && seen[2] && seen[3],"all four candidates can appear (no probability quota)");
    Reset(); player.xp=100; BeginUpgrade(); Check(gameState==GameState::ChoosingUpgrade,"eligible upgrade enters choice");
    bool triggeredSeen[4]{}, triggeredUnique=true;
    for(int i=0;i<64;++i) { Reset(); player.xp=100; for(auto& k:upgradeChoices) k=UpgradeKind::Attack; BeginUpgrade();
        triggeredUnique=triggeredUnique && upgradeChoices[0]!=upgradeChoices[1] && upgradeChoices[0]!=upgradeChoices[2] && upgradeChoices[1]!=upgradeChoices[2];
        for(auto k:upgradeChoices) if(int(k)>=0 && int(k)<4) triggeredSeen[int(k)]=true;
    }
    Check(triggeredUnique && triggeredSeen[0] && triggeredSeen[1] && triggeredSeen[2] && triggeredSeen[3],"BeginUpgrade actually extracts candidates");
    for (UpgradeKind kind:{UpgradeKind::Attack,UpgradeKind::FireRate,UpgradeKind::MaxHp,UpgradeKind::Heal}) {
        Reset(); player.hp=60; upgradeChoices[1]=kind; gameState=GameState::ChoosingUpgrade;
        Player before=player; ApplyUpgrade(-1); ApplyUpgrade(3);
        Check(SamePlayer(before,player) && gameState==GameState::ChoosingUpgrade,"invalid indices no effect");
        ApplyUpgrade(1);
        bool effect=kind==UpgradeKind::Attack ? player.attack==2 && player.hp==60 && player.maxHp==100 && Near(player.shotInterval,0.3f) :
            kind==UpgradeKind::FireRate ? Near(player.shotInterval,0.24f) && player.attack==1 && player.hp==60 && player.maxHp==100 :
            kind==UpgradeKind::MaxHp ? player.maxHp==120 && player.hp==80 && player.attack==1 && Near(player.shotInterval,0.3f) :
            player.hp==90 && player.maxHp==100 && player.attack==1 && Near(player.shotInterval,0.3f);
        Check(effect && gameState==GameState::Playing && player.xp==0 && player.level==1,"selected effect only and resume");
        before=player; ApplyUpgrade(1); Check(SamePlayer(before,player),"cannot reapply after resume");
    }
    Reset(); player.shotInterval=0.13f; gameState=GameState::ChoosingUpgrade; upgradeChoices[0]=UpgradeKind::FireRate; ApplyUpgrade(0); Check(Near(player.shotInterval,0.12f),"fire interval lower bound");
    Reset(); player.hp=95; gameState=GameState::ChoosingUpgrade; upgradeChoices[0]=UpgradeKind::Heal; ApplyUpgrade(0); Check(player.hp==100,"heal clamps maximum");
    Reset(); gameState=GameState::Won; Player terminal=player; ApplyUpgrade(0); Check(SamePlayer(terminal,player) && gameState==GameState::Won,"terminal cannot apply");
    Reset(); practiceMode=false; gameState=GameState::ChoosingUpgrade; player.invulnerability=0.5f; shotCooldown=0.2f; spawnTimer=54; survivalTime=30;
    bullets[0]={{100,100,10,10},9,0,true,2}; enemies[0]={{200,200,40,40},EnemyKind::Grunt,1,true};
    keyDown[KEY_D]=keyDown[KEY_RIGHT]=keyDown[KEY_ONE]=true; Player before=player;
    UpdateGame(1); Check(SamePlayer(before,player) && Near(bullets[0].rect.x,100) && Near(enemies[0].rect.x,200) && spawnTimer==54 && Near(survivalTime,30) && Near(shotCooldown,0.2f) && audioShots==0 && gameState==GameState::ChoosingUpgrade,"choice freezes all simulation; held key not new press");
    upgradeChoices[0]=UpgradeKind::Attack; upgradeChoices[1]=UpgradeKind::Heal; keyPressed[KEY_ONE]=keyPressed[KEY_TWO]=keyPressed[KEY_THREE]=true;
    UpdateGame(1); Check(player.attack==2 && player.hp==100 && gameState==GameState::Playing && SameRect(before.rect,player.rect) && Near(survivalTime,30) && Near(shotCooldown,0.2f),"simultaneous keys one choice; selection frame frozen");
    Reset(); gameState=GameState::ChoosingUpgrade; upgradeChoices[2]=UpgradeKind::Heal; player.hp=60; keyPressed[KEY_THREE]=true; UpdateGame(0); Check(player.hp==90 && gameState==GameState::Playing,"third key maps index two");
    Reset(); gameState=GameState::ChoosingUpgrade; upgradeChoices[1]=UpgradeKind::Attack; keyPressed[KEY_TWO]=keyPressed[KEY_THREE]=true; UpdateGame(0); Check(player.attack==2 && gameState==GameState::Playing,"second key before third");
    // 整帧连续成长涉及04，但直接候选/效果/射击夹具不依赖04完成。
    Reset(); gameState=GameState::ChoosingUpgrade; player.level=2; player.xp=150; upgradeChoices[0]=UpgradeKind::Attack; keyPressed[KEY_ONE]=true;
    UpdateGame(0); Check(gameState==GameState::Playing && player.level==2 && player.xp==150,"selection does not consume next XP same frame");
    keyPressed[KEY_ONE]=false; UpdateGame(0); Check(gameState==GameState::ChoosingUpgrade && player.level==3 && player.xp==50,"remaining XP opens next frame");
    for(int d=0;d<4;++d) {
        Reset(); const int keys[]={KEY_RIGHT,KEY_LEFT,KEY_UP,KEY_DOWN}; for(int i=d;i<4;++i) keyDown[keys[i]]=true;
        player.attack=3; FireBullets(); const float vx[]={9,-9,0,0}, vy[]={0,0,-9,9};
        Check(bullets[0].active && Near(bullets[0].vx,vx[d]) && Near(bullets[0].vy,vy[d]) && audioShots==1 && !bullets[1].active,"held arrows priority and single axis");
        Check(bullets[0].damage==3 && Near(shotCooldown,0.3f),"new bullet snapshot and cooldown");
    }
    Reset(); keyDown[KEY_RIGHT]=true; shotCooldown=0.1f; FireBullets(); Check(!bullets[0].active && audioShots==0 && Near(shotCooldown,0.1f),"positive cooldown blocks");
    Reset(); keyDown[KEY_RIGHT]=true; player.shotInterval=0.12f; shotCooldown=0.2f; UpdateGame(0.1f); UpdateGame(0.1f);
    Check(bullets[0].active && audioShots==1 && Near(shotCooldown,0.12f),"held fire resumes on cooldown zero");
    Reset(); keyDown[KEY_RIGHT]=true; player.attack=3; FireBullets(); gameState=GameState::ChoosingUpgrade; upgradeChoices[0]=UpgradeKind::Attack; ApplyUpgrade(0);
    enemies[0]={bullets[0].rect,EnemyKind::Heavy,6,true}; HandleHits(); Check(enemies[0].hp==3 && bullets[0].damage==3,"old bullet retains creation damage after upgrade");
    Reset(); for(auto& b:bullets) b.active=true; keyDown[KEY_RIGHT]=true; FireBullets(); Check(audioShots==0 && Near(shotCooldown,0),"full pool no sound or cooldown");
}
int main(int argc, char** argv) {
    if(argc==2 && std::strcmp(argv[1],"--verbose")==0) verbose=true;
    else if(argc!=1) { std::fprintf(stderr,"Usage: test-todos [--verbose]\n"); return 2; }
    Task01(); Task02(); Task03(); Task04(); Task05();
    int sumPassed=0, sumTotal=0;
    const char* taskNames[]={"", "Health bar", "Contact feedback", "Survival state", "Kill experience", "Upgrade choices"};
    for(int i=1;i<=5;++i) {
        std::printf("Task %02d - %s: %d/%d passed\nTODO_STATUS %d %d %d\n",i,taskNames[i],passed[i],total[i],i,passed[i],total[i]);
        sumPassed+=passed[i]; sumTotal+=total[i];
    }
    std::printf("Total: %d/%d passed\nTask 06: manual teacher review (not scored)\n",sumPassed,sumTotal);
    UnloadGameAudio(); return sumPassed==sumTotal ? 0 : 1;
}
