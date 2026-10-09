#include "test_support.h"
#include "game.h"
#include "drawing.h"
#include "audio.h"
#include "my_experiment.h"
#ifdef L4_TEACHER_EXPERIMENT
#include "../teacher/my_experiment.h"
#endif
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

static bool verbose = false;
// Index zero is the independent integration suite, never a TODO score.
static int group = 0, passed[14]{}, total[14]{}, blocked[14]{};
static bool Near(float a, float b) { return std::isfinite(a) && std::isfinite(b) && std::fabs(a - b) < 0.0001f; }
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
#define Check(condition, name) RecordCheck((condition), (name), __LINE__, #condition)
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
    DrawGame(); Check(Bar(100),"bar remains in screen coordinates");
}
static void Task02() {
    group=2; Reset(); player.invulnerability=0.6f; UpdateProtection(0.1f);
    Check(Near(player.invulnerability,0.5f),"protection decrements once"); UpdateProtection(0.5f);
    Check(Near(player.invulnerability,0),"protection exact zero"); UpdateProtection(1);
    Check(Near(player.invulnerability,0),"protection clamps zero");
}
static void Task03() {
    group=3; Reset(); player.hp=50; Contact(); Contact(1); UpdateEnemies(0.1f);
    Check(player.hp==40 && audioHurts==1,"multiple contacts one damage and hurt");
    Check(enemies[0].active && enemies[1].active,"contact does not consume enemies");
    Check(Near(player.invulnerability,0.8f),"contact starts 0.8s protection");
    UpdateEnemies(0.1f); Check(player.hp==40 && audioHurts==1,"protected contact silent");
    // Direct controlled fixture: this check must not depend on protection countdown (ID 2).
    player.invulnerability=0; UpdateEnemies(0); Check(player.hp==30 && audioHurts==2,"zero protection permits next contact");
    Reset(); player.hp=3; Contact(); enemies[1]={{100,100,40,40},EnemyKind::Grunt,1,true}; UpdateEnemies(0);
    Check(player.hp==0 && gameState==GameState::Lost && audioHurts==1,"fatal damage clamps HP and loses");
    Check(Near(enemies[1].rect.x,100),"fatal contact stops later enemies");
    for (GameState s:{GameState::ChoosingUpgrade,GameState::Won,GameState::Lost}) {
        Reset(); gameState=s; Player before=player; TakeContactDamage();
        Check(SamePlayer(before,player) && gameState==s && audioHurts==0,"non-Playing contact ignored");
    }
}
static void Task04() {
    group=4; Reset(); player.invulnerability=0; Check(PlayerVisible(),"unprotected visible");
    player.invulnerability=0.15f; Check(!PlayerVisible(),"odd flash phase hidden");
    player.invulnerability=0.25f; Check(PlayerVisible(),"even flash phase visible");
}
static void Task05() {
    group=5; Reset(); UpdateSurvival(200); Check(Near(survivalTime,0),"practice does not accumulate");
    Reset(); practiceMode=false; UpdateSurvival(2.5f); Check(Near(survivalTime,2.5f),"combat uses seconds");
    survivalTime=179.9f; UpdateSurvival(1); Check(Near(survivalTime,180),"survival capped at 180");
}
static void Task06() {
    group=6; Reset(); survivalTime=180; CheckWin(); Check(gameState==GameState::Playing,"practice cannot win");
    Reset(); practiceMode=false; survivalTime=179.9f; CheckWin(); Check(gameState==GameState::Playing,"not won before deadline");
    survivalTime=180; CheckWin(); Check(gameState==GameState::Won,"deadline wins");
    for (GameState s:{GameState::ChoosingUpgrade,GameState::Won,GameState::Lost}) {
        gameState=s; CheckWin(); Check(gameState==s,"win only changes Playing");
    }
}
static void Task07() {
    group=7; Reset();
    enemies[0]={{100,100,48,48},EnemyKind::Heavy,3,true}; bullets[0]={{105,105,10,10},0,0,true,1}; HandleHits();
    Check(kills==0 && player.xp==0 && enemies[0].hp==2 && audioHits==1,"nonfatal hit no kill or XP");
    bullets[0]={{105,105,10,10},0,0,true,2}; HandleHits();
    Check(kills==1 && player.xp==10 && !enemies[0].active && score==30 && audioHits==2,"actual death rewards once");
    HandleHits(); Check(kills==1 && player.xp==10,"inactive corpse not rewarded twice");
    Reset(); RewardKill(EnemyKind::Elite); Check(kills==1 && player.xp==10,"reward fixed XP independent of kind");
    Reset(); player.level=6; player.xp=250; RewardKill(EnemyKind::Grunt); Check(player.xp==260 && kills==1,"max level still earns XP");
}
static bool ChoicesUnchanged(const UpgradeKind (&before)[3]) {
    return std::equal(before,before+3,upgradeChoices);
}
static void Task08() {
    group=8; Reset(); Check(ExperienceNeeded()==100,"fixed threshold 100");
    const UpgradeKind sentinel[]={UpgradeKind::Heal,UpgradeKind::Heal,UpgradeKind::Heal};
    std::copy_n(sentinel,3,upgradeChoices);
    player.xp=99; Player below=player; BeginUpgrade();
    Check(SamePlayer(below,player) && gameState==GameState::Playing && ChoicesUnchanged(sentinel),"below threshold leaves player/state/candidates unchanged");
    Reset(); player.xp=100; BeginUpgrade(); Check(player.level==2 && player.xp==0,"exact threshold advances level");
    Reset(); player.xp=250; BeginUpgrade(); Check(player.level==2 && player.xp==150,"excess XP retained");
    for (GameState s:{GameState::ChoosingUpgrade,GameState::Won,GameState::Lost}) {
        Reset(); gameState=s; player.xp=250; std::copy_n(sentinel,3,upgradeChoices); Player before=player; BeginUpgrade();
        Check(SamePlayer(before,player) && gameState==s && ChoicesUnchanged(sentinel),"non-Playing leaves player/state/candidates unchanged");
    }
    Reset(); player.level=6; player.xp=250; std::copy_n(sentinel,3,upgradeChoices); Player before=player; BeginUpgrade();
    Check(SamePlayer(before,player) && gameState==GameState::Playing && ChoicesUnchanged(sentinel),"max level leaves player/state/candidates unchanged");
}
static void Task09() {
    group=9;
    // Probe the necessary ID 8 success path, not all of its boundary assertions.
    Reset(); player.xp=100; BeginUpgrade();
    if (player.level!=2 || player.xp!=0) { blocked[9]=8; return; }
    Reset(); player.xp=100; BeginUpgrade(); Check(gameState==GameState::ChoosingUpgrade,"eligible upgrade enters choice");
    bool seen[4]{}, unique=true;
    SetRandomSeed(1701);
    for(int i=0;i<128;++i) {
        Reset(); player.xp=100; for(auto& k:upgradeChoices) k=UpgradeKind::Attack; BeginUpgrade();
        for(auto k:upgradeChoices) { if(int(k)<0 || int(k)>3) unique=false; else seen[int(k)]=true; }
        unique=unique && upgradeChoices[0]!=upgradeChoices[1] && upgradeChoices[0]!=upgradeChoices[2] && upgradeChoices[1]!=upgradeChoices[2];
    }
    Check(unique,"BeginUpgrade extracts three in-range different candidates");
    Check(seen[0] && seen[1] && seen[2] && seen[3],"BeginUpgrade can include all four kinds (no probability quota)");
}
static void Task10() {
    group=10;
    for (UpgradeKind kind:{UpgradeKind::Attack,UpgradeKind::FireRate,UpgradeKind::MaxHp,UpgradeKind::Heal}) {
        Reset(); player.hp=60; upgradeChoices[1]=kind; gameState=GameState::ChoosingUpgrade;
        Player before=player; ApplyUpgrade(-1); ApplyUpgrade(3);
        Check(SamePlayer(before,player) && gameState==GameState::ChoosingUpgrade,"invalid indices no effect");
        ApplyUpgrade(1);
        Player expected=before;
        if(kind==UpgradeKind::Attack) ++expected.attack;
        else if(kind==UpgradeKind::FireRate) expected.shotInterval=0.24f;
        else if(kind==UpgradeKind::MaxHp) { expected.maxHp=120; expected.hp=80; }
        else expected.hp=90;
        Check(SamePlayer(expected,player) && gameState==GameState::Playing,"selected effect only and resume");
        before=player; ApplyUpgrade(1); Check(SamePlayer(before,player) && gameState==GameState::Playing,"cannot reapply after resume");
    }
    Reset(); player.shotInterval=0.13f; gameState=GameState::ChoosingUpgrade; upgradeChoices[0]=UpgradeKind::FireRate; ApplyUpgrade(0); Check(Near(player.shotInterval,0.12f),"fire interval lower bound");
    Reset(); player.hp=95; gameState=GameState::ChoosingUpgrade; upgradeChoices[0]=UpgradeKind::Heal; ApplyUpgrade(0); Check(player.hp==100,"heal clamps maximum");
    Reset(); player.hp=100; gameState=GameState::ChoosingUpgrade; upgradeChoices[0]=UpgradeKind::Heal; ApplyUpgrade(0); Check(player.hp==100 && gameState==GameState::Playing,"heal at full HP still resumes");
    Reset(); player.hp=110; gameState=GameState::ChoosingUpgrade; upgradeChoices[0]=UpgradeKind::MaxHp; ApplyUpgrade(0);
    Check(player.maxHp==120 && player.hp==120 && gameState==GameState::Playing,"MaxHp current HP clamps to new maximum");
    Reset(); player.shotInterval=0.13f; upgradeChoices[0]=UpgradeKind::FireRate;
    for(int i=0;i<4;++i) {
        gameState=GameState::ChoosingUpgrade; ApplyUpgrade(0);
        Check(Near(player.shotInterval,0.12f) && gameState==GameState::Playing,"repeated fire upgrades stay at floor");
    }
    Reset(); upgradeChoices[2]=UpgradeKind::Attack; gameState=GameState::ChoosingUpgrade; ApplyUpgrade(2);
    Check(player.attack==2 && gameState==GameState::Playing,"last valid index applies selected choice");
    for (GameState s:{GameState::Playing,GameState::Won,GameState::Lost}) {
        Reset(); gameState=s; Player before=player; ApplyUpgrade(0); Check(SamePlayer(before,player) && gameState==s,"only choice state can apply");
    }
}
static void Task11() {
    group=11;
    for(float cooldown:{0.1f,0.00001f}) {
        Reset(); keyDown[KEY_RIGHT]=true; shotCooldown=cooldown; FireBullets();
        Check(!bullets[0].active && audioShots==0 && Near(shotCooldown,cooldown),"positive cooldown blocks without side effects");
    }
}
static void Task12() {
    group=12;
    for(int d=0;d<4;++d) {
        Reset(); const int keys[]={KEY_RIGHT,KEY_LEFT,KEY_UP,KEY_DOWN}; for(int i=d;i<4;++i) keyDown[keys[i]]=true;
        player.attack=3; player.shotInterval=0.22f; FireBullets(); const float vx[]={9,-9,0,0}, vy[]={0,0,-9,9};
        Check(bullets[0].active && Near(bullets[0].vx,vx[d]) && Near(bullets[0].vy,vy[d]) && audioShots==1 && !bullets[1].active,"held arrows priority and single axis");
        Check(bullets[0].damage==3 && Near(shotCooldown,0.22f),"new bullet snapshot and player's interval cooldown");
    }
    Reset(); keyDown[KEY_RIGHT]=true; player.attack=3; FireBullets(); player.attack=4;
    enemies[0]={bullets[0].rect,EnemyKind::Heavy,6,true}; HandleHits(); Check(enemies[0].hp==3 && bullets[0].damage==3,"old bullet retains creation damage after direct attack change");
    Reset(); for(auto& b:bullets) b.active=true; keyDown[KEY_RIGHT]=true; FireBullets(); Check(audioShots==0 && Near(shotCooldown,0),"full pool no sound or cooldown");
    Reset(); FireBullets(); Check(!bullets[0].active && audioShots==0 && Near(shotCooldown,0),"no direction no bullet or cooldown");
}

// Only experiment calls use a temporary parameter override. Save and restore the
// gameplay fixture too, even if an unfinished ApplyUpgrade mutates unrelated state.
struct FixtureScope {
    Player savedPlayer=player; Camera2D savedCamera=camera;
    Bullet savedBullets[MAX_BULLETS]; Enemy savedEnemies[MAX_ENEMIES];
    int savedWall[WORLD_ROWS][WORLD_COLS]; UpgradeKind savedChoices[3];
    int savedScore=score, savedKills=kills, savedTimer=spawnTimer, savedSequence=spawnSequence;
    float savedTime=survivalTime, savedCooldown=shotCooldown;
    GameState savedState=gameState; bool savedPractice=practiceMode, savedDebug=debugMode;
    FixtureScope() {
        std::copy_n(bullets,MAX_BULLETS,savedBullets); std::copy_n(enemies,MAX_ENEMIES,savedEnemies);
        std::memcpy(savedWall,wall,sizeof wall); std::copy_n(upgradeChoices,3,savedChoices);
    }
    ~FixtureScope() {
        player=savedPlayer; camera=savedCamera;
        std::copy_n(savedBullets,MAX_BULLETS,bullets); std::copy_n(savedEnemies,MAX_ENEMIES,enemies);
        std::memcpy(wall,savedWall,sizeof wall); std::copy_n(savedChoices,3,upgradeChoices);
        score=savedScore; kills=savedKills; spawnTimer=savedTimer; spawnSequence=savedSequence;
        survivalTime=savedTime; shotCooldown=savedCooldown; gameState=savedState;
        practiceMode=savedPractice; debugMode=savedDebug;
    }
};
struct Trial { float value; bool correct; };
static float ExpectedValue(const ExperimentScenario& s, ExperimentVariable variable, UpgradeParameters p) {
    if(variable==ExperimentVariable::HealAmount) return float(std::min(s.maxHp,s.hp+p.healAmount));
    return std::max(MIN_SHOT_INTERVAL,s.shotInterval*p.fireRateMultiplier);
}
static Trial RunTrial(const ExperimentScenario& s, ExperimentVariable variable, UpgradeParameters p) {
    FixtureScope restoreFixture;
    ScopedUpgradeParameters restoreParameters(p);
    player.hp=s.hp; player.maxHp=s.maxHp; player.shotInterval=s.shotInterval;
    gameState=GameState::ChoosingUpgrade;
    upgradeChoices[1]=variable==ExperimentVariable::HealAmount ? UpgradeKind::Heal : UpgradeKind::FireRate;
    Player expected=player;
    float value=ExpectedValue(s,variable,p);
    if(variable==ExperimentVariable::HealAmount) expected.hp=int(value); else expected.shotInterval=value;
    ApplyUpgrade(1); // The measured value always comes from the student's production function.
    return {variable==ExperimentVariable::HealAmount ? float(player.hp) : player.shotInterval,
        SamePlayer(expected,player) && gameState==GameState::Playing};
}
static bool ApplyReady(ExperimentVariable variable) {
    // Test only the needed ordinary success path. A bad Attack or boundary case
    // in ID 10 must not block a working Heal/FireRate experiment.
    ExperimentScenario s{}; s.hp=60; s.maxHp=100; s.shotInterval=0.3f;
    if(variable==ExperimentVariable::HealAmount || variable==ExperimentVariable::FireRateMultiplier)
        return RunTrial(s,variable,DEFAULT_UPGRADE_PARAMETERS).correct;
    return RunTrial(s,ExperimentVariable::HealAmount,DEFAULT_UPGRADE_PARAMETERS).correct &&
        RunTrial(s,ExperimentVariable::FireRateMultiplier,DEFAULT_UPGRADE_PARAMETERS).correct;
}
static bool LegalInitial(const ExperimentScenario& s) {
    return s.maxHp>0 && s.maxHp<=10000 && s.hp>0 && s.hp<=s.maxHp &&
        std::isfinite(s.shotInterval) && s.shotInterval>=MIN_SHOT_INTERVAL && s.shotInterval<=10;
}
static bool FiniteRecords(const ExperimentScenario& s) {
    return std::isfinite(s.predictedBaseline) && std::isfinite(s.predictedExperiment) &&
        std::isfinite(s.observedBaseline) && std::isfinite(s.observedExperiment) &&
        std::isfinite(s.actualChange) && std::isfinite(s.baselineError) && std::isfinite(s.experimentError);
}
static void Task13() {
    group=13; Reset();
#ifdef L4_TEACHER_EXPERIMENT
    const ExperimentSpec& spec=teacherExperiment;
#else
    const ExperimentSpec& spec=myExperiment;
#endif
    if(!ApplyReady(spec.variable)) { blocked[13]=10; return; }
    Check(spec.submitted,"experiment submitted");
    const bool chosen=spec.variable==ExperimentVariable::HealAmount || spec.variable==ExperimentVariable::FireRateMultiplier;
    Check(chosen,"select exactly one experiment variable");
    if(!chosen) return;
    const bool parameterValid=spec.variable==ExperimentVariable::HealAmount ?
        spec.healAmount>0 && spec.healAmount<=100 && spec.healAmount!=30 && Near(spec.fireRateMultiplier,0.8f) :
        spec.healAmount==30 && std::isfinite(spec.fireRateMultiplier) && spec.fireRateMultiplier>0 &&
        spec.fireRateMultiplier<1 && !Near(spec.fireRateMultiplier,0.8f);
    Check(parameterValid,"legal non-default chosen parameter; other parameter unchanged");
    if(!parameterValid) return;
    const UpgradeParameters experimental{spec.healAmount,spec.fireRateMultiplier};
    const ExperimentScenario scenarios[]={spec.normal,spec.boundary};
    const char* names[]={"normal","boundary"};
    for(int i=0;i<2;++i) {
        const auto& s=scenarios[i];
        const bool legal=LegalInitial(s);
        Check(legal,"legal experiment initial state (HP 1..max<=10000; interval 0.12..10)");
        if(!legal) continue;
        const float baselineExpected=ExpectedValue(s,spec.variable,DEFAULT_UPGRADE_PARAMETERS);
        const float experimentExpected=ExpectedValue(s,spec.variable,experimental);
        bool normal=false, boundary=false;
        if(spec.variable==ExperimentVariable::HealAmount) {
            normal=s.hp+std::max(30,spec.healAmount)<s.maxHp;
            boundary=s.hp<s.maxHp && s.hp+std::min(30,spec.healAmount)>=s.maxHp;
        } else {
            normal=s.shotInterval*std::min(0.8f,spec.fireRateMultiplier)>MIN_SHOT_INTERVAL;
            boundary=s.shotInterval>MIN_SHOT_INTERVAL && s.shotInterval*std::max(0.8f,spec.fireRateMultiplier)<=MIN_SHOT_INTERVAL;
        }
        Check(i==0 ? normal && !Near(baselineExpected,experimentExpected) : boundary,
            "normal exposes change without clamp; boundary exercises both clamps");
        Player before=player; GameState beforeState=gameState; UpgradeParameters beforeParams=GetUpgradeParameters();
        UpgradeKind beforeChoices[3]; std::copy_n(upgradeChoices,3,beforeChoices);
        Trial baseline=RunTrial(s,spec.variable,DEFAULT_UPGRADE_PARAMETERS);
        Trial experiment=RunTrial(s,spec.variable,experimental);
        std::printf("[EXPERIMENT] %s baseline=%.6f experiment=%.6f change=%.6f expectedBaseline=%.6f expectedExperiment=%.6f baselineError=%.6f experimentError=%.6f\n",
            names[i],baseline.value,experiment.value,experiment.value-baseline.value,baselineExpected,experimentExpected,
            baseline.value-s.predictedBaseline,experiment.value-s.predictedExperiment);
        Check(baseline.correct && experiment.correct,"real ApplyUpgrade matches independent rule; other attributes unchanged and Playing restored");
        Check(SamePlayer(before,player) && gameState==beforeState &&
            std::equal(beforeChoices,beforeChoices+3,upgradeChoices) && GetUpgradeParameters().healAmount==beforeParams.healAmount &&
            Near(GetUpgradeParameters().fireRateMultiplier,beforeParams.fireRateMultiplier),"experiment parameter and fixture scopes restored");
        Check(s.recorded,"scenario observations recorded (measurement available before recording)");
        const bool finite=FiniteRecords(s);
        Check(finite,"all predictions observations and numeric analysis finite");
        if(!finite) continue;
        Check(Near(s.observedBaseline,baseline.value) && Near(s.observedExperiment,experiment.value),"observations match real measured outputs");
        const bool matched=Near(s.predictedBaseline,baseline.value) && Near(s.predictedExperiment,experiment.value);
        Check(s.predictionMatched==matched && Near(s.actualChange,experiment.value-baseline.value) &&
            Near(s.baselineError,baseline.value-s.predictedBaseline) && Near(s.experimentError,experiment.value-s.predictedExperiment),
            "prediction match flag and signed corrected analysis match measurements (wrong predictions allowed)");
    }
}
static void Integration() {
    group=0;
    Reset(); player.hp=50; camera.target={5000,3000}; gameState=GameState::ChoosingUpgrade;
    DrawGame(); Check(Layering(),"world/HUD/bar/choice overlay order");
    Reset(); practiceMode=false; survivalTime=179.9f; player.hp=10; player.xp=100; Contact();
    enemies[1]={{100,100,40,40},EnemyKind::Grunt,1,true}; bullets[0]={{100,100,10,10},0,0,true,1};
    UpdateGame(0.2f); Check(gameState==GameState::Lost && player.level==1 && kills==0 && bullets[0].active,"fatal frame stops hits/win/upgrade");
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
    Reset(); practiceMode=false; gameState=GameState::ChoosingUpgrade; player.invulnerability=0.5f; shotCooldown=0.2f; spawnTimer=54; survivalTime=30;
    bullets[0]={{100,100,10,10},9,0,true,2}; enemies[0]={{200,200,40,40},EnemyKind::Grunt,1,true};
    keyDown[KEY_D]=keyDown[KEY_RIGHT]=keyDown[KEY_ONE]=true; Player before=player;
    UpdateGame(1); Check(SamePlayer(before,player) && Near(bullets[0].rect.x,100) && Near(enemies[0].rect.x,200) && spawnTimer==54 && Near(survivalTime,30) && Near(shotCooldown,0.2f) && audioShots==0 && gameState==GameState::ChoosingUpgrade,"choice freezes all simulation; held key not new press");
    upgradeChoices[0]=UpgradeKind::Attack; upgradeChoices[1]=UpgradeKind::Heal; keyPressed[KEY_ONE]=keyPressed[KEY_TWO]=keyPressed[KEY_THREE]=true;
    UpdateGame(1); Check(player.attack==2 && player.hp==100 && gameState==GameState::Playing && SameRect(before.rect,player.rect) && Near(survivalTime,30) && Near(shotCooldown,0.2f),"simultaneous keys one choice; selection frame frozen");
    Reset(); gameState=GameState::ChoosingUpgrade; upgradeChoices[2]=UpgradeKind::Heal; player.hp=60; keyPressed[KEY_THREE]=true; UpdateGame(0); Check(player.hp==90 && gameState==GameState::Playing,"third key maps index two");
    Reset(); gameState=GameState::ChoosingUpgrade; upgradeChoices[1]=UpgradeKind::Attack; keyPressed[KEY_TWO]=keyPressed[KEY_THREE]=true; UpdateGame(0); Check(player.attack==2 && gameState==GameState::Playing,"second key before third");
    Reset(); gameState=GameState::ChoosingUpgrade; player.level=2; player.xp=150; upgradeChoices[0]=UpgradeKind::Attack; keyPressed[KEY_ONE]=true;
    UpdateGame(0); Check(gameState==GameState::Playing && player.level==2 && player.xp==150,"selection does not consume next XP same frame");
    keyPressed[KEY_ONE]=false; UpdateGame(0); Check(gameState==GameState::ChoosingUpgrade && player.level==3 && player.xp==50,"remaining XP opens next frame");
    Reset(); keyDown[KEY_RIGHT]=true; player.shotInterval=0.12f; shotCooldown=0.2f; UpdateGame(0.1f); UpdateGame(0.1f);
    Check(bullets[0].active && audioShots==1 && Near(shotCooldown,0.12f),"held fire resumes on cooldown zero");
    Reset(); keyDown[KEY_RIGHT]=true; player.attack=3; FireBullets(); gameState=GameState::ChoosingUpgrade; upgradeChoices[0]=UpgradeKind::Attack; ApplyUpgrade(0);
    enemies[0]={bullets[0].rect,EnemyKind::Heavy,6,true}; HandleHits(); Check(player.attack==4 && enemies[0].hp==3 && bullets[0].damage==3,"old bullet snapshot survives actual upgrade");
    Check(GetUpgradeParameters().healAmount==30 && Near(GetUpgradeParameters().fireRateMultiplier,0.8f),"ordinary game parameters fixed after experiment");
}
int main(int argc, char** argv) {
    if(argc==2 && std::strcmp(argv[1],"--verbose")==0) verbose=true;
    else if(argc!=1) { std::fprintf(stderr,"Usage: test-todos [--verbose]\n"); return 2; }
    std::printf("TODO_PROTOCOL L4-v2\n");
    Task01(); Task02(); Task03(); Task04(); Task05(); Task06(); Task07();
    Task08(); Task09(); Task10(); Task11(); Task12(); Task13(); Integration();
    int sumPassed=passed[0], sumTotal=total[0]; bool anyBlocked=false;
    const char* taskNames[]={"", "L4-01 Health bar", "L4-02-A Protection countdown", "L4-02-B Contact damage", "L4-02-C Flash visibility",
        "L4-03-A Survival timer", "L4-03-B Win state", "L4-04-A Kill rewards", "L4-04-B Upgrade eligibility",
        "L4-05-A Upgrade candidates", "L4-05-B Upgrade effects", "L4-05-C Cooldown gate", "L4-05-D Bullet snapshot", "L4-06 Numeric experiment"};
    for(int i=1;i<=13;++i) {
        std::printf("Task %02d - %s: %d/%d passed\nTODO_STATUS %d %d %d\n",i,taskNames[i],passed[i],total[i],i,passed[i],total[i]);
        if(blocked[i]) { std::printf("TODO_BLOCKED %d %d\n",i,blocked[i]); anyBlocked=true; }
        sumPassed+=passed[i]; sumTotal+=total[i];
    }
    std::printf("Integration: %d/%d passed\nINTEGRATION_STATUS %d %d\nTotal: %d/%d passed\n",passed[0],total[0],passed[0],total[0],sumPassed,sumTotal);
    UnloadGameAudio(); return !anyBlocked && sumPassed==sumTotal ? 0 : 1;
}
