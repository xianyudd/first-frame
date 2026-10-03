// Headless behavior tests: real collision math, intercepted drawing, fake input/audio.
// Drawing records API calls only; it is NOT a real-window or GPU validation.
#include "raylib.h"
#include <vector>
#include <string>
struct DrawCall { std::string kind, text; Rectangle rect; int depth; };
static std::vector<DrawCall> drawCalls;
static int modeDepth=0, modeBegins=0, modeEnds=0, modeErrors=0;
static Camera2D recordedCamera;
static void Record(const char* kind, Rectangle rect={}, const char* text="") {
    drawCalls.push_back({kind,text,rect,modeDepth});
}
static void TestBeginDrawing() { Record("begin"); }
static void TestEndDrawing() { if(modeDepth) ++modeErrors; Record("end"); }
static void TestClearBackground(Color) { Record("clear"); }
[[maybe_unused]] static void TestBeginMode2D(Camera2D value) {
    if(modeDepth) ++modeErrors;
    ++modeDepth; ++modeBegins; recordedCamera=value;
}
[[maybe_unused]] static void TestEndMode2D() { if(modeDepth!=1) ++modeErrors; --modeDepth; ++modeEnds; }
static void TestDrawRectangleRec(Rectangle r,Color) { Record("sprite",r); }
static void TestDrawTexturePro(Texture2D,Rectangle,Rectangle r,Vector2,float,Color) { Record("sprite",r); }
static void TestDrawRectangleLinesEx(Rectangle r,float,Color) { Record("outline",r); }
static void TestDrawRectangle(int x,int y,int w,int h,Color) { Record("panel",{float(x),float(y),float(w),float(h)}); }
static void TestDrawLine(int x,int y,int ex,int ey,Color) { Record("grid",{float(x),float(y),float(ex),float(ey)}); }
static void TestDrawText(const char* t,int x,int y,int size,Color) { Record("text",{float(x),float(y),0,float(size)},t); }
#define BeginDrawing TestBeginDrawing
#define EndDrawing TestEndDrawing
#define ClearBackground TestClearBackground
#define BeginMode2D TestBeginMode2D
#define EndMode2D TestEndMode2D
#define DrawRectangleRec TestDrawRectangleRec
#define DrawTexturePro TestDrawTexturePro
#define DrawRectangleLinesEx TestDrawRectangleLinesEx
#define DrawRectangle TestDrawRectangle
#define DrawLine TestDrawLine
#define DrawText TestDrawText
#include <cstdio>
#include <cmath>
#include <cstring>
#include "fake_audio_backend.h"
static bool keyDown[512]={}, keyPressed[512]={};
static bool TestKeyDown(int key) { return key>=0 && key<512 && keyDown[key]; }
static bool TestKeyPressed(int key) { return key>=0 && key<512 && keyPressed[key]; }
#define IsKeyDown TestKeyDown
#define IsKeyPressed TestKeyPressed
#define L3_AUDIO_TEST
#define UNIT_TEST
#ifndef SRC_MAIN
#define SRC_MAIN "../src/main.cpp"
#endif
#include SRC_MAIN
#undef IsKeyDown
#undef IsKeyPressed
static int passed=0, failed=0, sectionPassed=0, sectionTotal=0, currentTask=0, blocked=0;
static bool completed[10]={};
static bool verbose=false;
static const char* taskNames[]={"", "World map configuration", "Tile coordinates", "Camera initialization",
    "Camera following", "World and HUD drawing", "Wall collision", "World bounds and axis movement",
    "Bullet allocation and shot sound", "Enemy hits and hit sound"};
static void check(const char* name, double expected, double actual, int line) {
    const bool ok=std::fabs(expected-actual)<0.0001;
    ++sectionTotal; if(ok) { ++passed; ++sectionPassed; } else ++failed;
    if(verbose || !ok)
        std::printf("[%s] Task %02d: %s (expected %.6g, got %.6g) (test_todos.cpp:%d)\n",
                    ok?"PASS":"FAIL",currentTask,name,expected,actual,line);
}
#define CHECK(n,e,a) check(n,e,a,__LINE__)
static void begin(int id) { currentTask=id; sectionPassed=sectionTotal=0; std::printf("\nTask %02d: %s\n",id,taskNames[id]); }
static void end(int id) {
    completed[id]=sectionTotal>0 && sectionPassed==sectionTotal;
    std::printf("Task %02d: %s - %d/%d %s\n",id,taskNames[id],sectionPassed,sectionTotal,completed[id]?"PASS":"FAIL");
    std::printf("TODO_STATUS %d %d %d\n",id,sectionPassed,sectionTotal);
}
static bool prerequisite(int id,int prior) {
    if(completed[prior]) return true;
    ++blocked;
    std::printf("\nTask %02d: %s - 0/0 BLOCKED; first complete Task %02d\n",id,taskNames[id],prior);
    std::printf("TODO_BLOCKED %d %d\nTODO_STATUS %d 0 0\n",id,prior,id);
    return false;
}
static void resetFixture() {
    std::memset(keyDown,0,sizeof(keyDown)); std::memset(keyPressed,0,sizeof(keyPressed));
    std::memset(wall,0,sizeof(wall));
    player={{200,200,40,40},5.0f,100};
    for(auto& b:bullets) b={{0,0,10,10},0,0,false};
    for(auto& e:enemies) e={{0,0,0,0},EnemyKind::Grunt,0,false};
    score=spawnTimer=spawnSequence=0; gameOver=false; practiceMode=false;
    UnloadGameAudio(); audioLoadSucceeds=true; LoadGameAudio(); ResetAudioCounts();
}
static void mapConfiguration() {
    begin(1); resetFixture(); InitGame();
    // Check the real spawn body independently of student collision functions.
    const Rectangle spawn=player.rect;
    CHECK("window width remains fixed",1280,SCREEN_W);
    CHECK("window height remains fixed",720,SCREEN_H); CHECK("tile size",40,TILE);
    CHECK("world width derived from columns",WORLD_COLS*TILE,WORLD_W);
    CHECK("world height derived from rows",WORLD_ROWS*TILE,WORLD_H);
    CHECK("world extends beyond window X",true,WORLD_W>SCREEN_W);
    CHECK("world extends beyond window Y",true,WORLD_H>SCREEN_H);
    bool farX=false,farY=false,spawnBlocked=false;
    for(int r=0;r<WORLD_ROWS;++r) for(int c=0;c<WORLD_COLS;++c) if(wall[r][c]) {
        farX |= c*TILE>=SCREEN_W; farY |= r*TILE>=SCREEN_H;
        const Rectangle tile={float(c*TILE),float(r*TILE),float(TILE),float(TILE)};
        spawnBlocked |= CheckCollisionRecs(spawn,tile);
    }
    CHECK("at least one wall beyond original screen",true,farX || farY);
    CHECK("initial player body is clear of every wall",false,spawnBlocked); end(1);
}
static void coordinates() {
    begin(2); resetFixture();
    const int cells[][2]={{0,0},{2,7},{13,3},{WORLD_ROWS-1,WORLD_COLS-1}};
    for(const auto& cell:cells) {
        Rectangle r=CellRect(cell[0],cell[1]);
        CHECK("cell x is column times TILE",cell[1]*TILE,r.x);
        CHECK("cell y is row times TILE",cell[0]*TILE,r.y);
        CHECK("cell width",TILE,r.width); CHECK("cell height",TILE,r.height);
    } end(2);
}
static void cameraValues(const char* stage) {
    CHECK(stage,SCREEN_W/2.0f,camera.offset.x); CHECK("offset Y is screen center",SCREEN_H/2.0f,camera.offset.y);
    CHECK("target X is body center",player.rect.x+player.rect.width/2,camera.target.x);
    CHECK("target Y is body center",player.rect.y+player.rect.height/2,camera.target.y);
    CHECK("zoom",1,camera.zoom); CHECK("rotation",0,camera.rotation);
}
static void cameraInit() {
    begin(3); resetFixture(); player.rect={870,530,23,57};
    camera={{-3,-4},{-5,-6},78,2}; InitCamera(); cameraValues("InitCamera offset X"); end(3);
}
static void cameraFollow() {
    if(!prerequisite(4,3)) return;
    begin(4); resetFixture(); InitCamera();
    player.rect={float(SCREEN_W+75),float(SCREEN_H+35),23,57}; UpdateCamera();
    cameraValues("follow offset X");
    player.rect={150,170,64,28}; UpdateCamera(); cameraValues("second movement offset X");
    camera={{-8,-9},{-10,-11},20,4}; InitGame(); cameraValues("restart resets camera offset X"); end(4);
}
static bool sameRect(Rectangle a,Rectangle b) {
    return std::fabs(a.x-b.x)<0.001 && std::fabs(a.y-b.y)<0.001 &&
        std::fabs(a.width-b.width)<0.001 && std::fabs(a.height-b.height)<0.001;
}
static void drawing() {
    begin(5); resetFixture(); InitMap();
    // Task 05 has its own VALID camera, independent of unfinished Task 03.
    camera={{SCREEN_W/2.0f,SCREEN_H/2.0f},{1500,900},0,1};
    player.rect={1450,850,23,57}; bullets[0]={{1510,950,10,10},0,0,true};
    enemies[0]={{1600,1000,34,34},EnemyKind::Runner,1,true};
    const bool oldDebug=debugMode; const CollisionCase oldCase=myCase;
    myCase={"scope probe",{1700,920,27,31},false,true}; debugMode=true; gameOver=true;
    drawCalls.clear(); modeDepth=modeBegins=modeEnds=modeErrors=0; DrawGame();
    CHECK("one world camera begins",1,modeBegins); CHECK("one world camera ends",1,modeEnds);
    CHECK("balanced camera at EndDrawing",0,modeDepth); CHECK("no nested or unmatched camera",0,modeErrors);
    CHECK("actual camera target X passed to renderer",camera.target.x,recordedCamera.target.x);
    CHECK("actual camera target Y passed to renderer",camera.target.y,recordedCamera.target.y);
    CHECK("actual camera offset X passed to renderer",camera.offset.x,recordedCamera.offset.x);
    CHECK("actual camera offset Y passed to renderer",camera.offset.y,recordedCamera.offset.y);
    CHECK("actual camera zoom passed to renderer",camera.zoom,recordedCamera.zoom);
    CHECK("actual camera rotation passed to renderer",camera.rotation,recordedCamera.rotation);
    bool sprites=true,grids=true,outlines=true,text=true,panels=true;
    int spriteCount=0,gridCount=0,outlineCount=0,textCount=0,panelCount=0;
    bool sawPlayer=false,sawBullet=false,sawEnemy=false,sawCase=false,sawEnd=false;
    bool sawCaseText=false,sawHP=false,sawWorldExtentX=false,sawWorldExtentY=false;
    int lastWorld=-1,firstHUD=int(drawCalls.size());
    for(int i=0;i<(int)drawCalls.size();++i) {
        const auto& call=drawCalls[i];
        if(call.kind=="sprite") {
            ++spriteCount; sprites &= call.depth==1; lastWorld=i;
            sawPlayer |= sameRect(call.rect,player.rect); sawBullet |= sameRect(call.rect,bullets[0].rect);
            sawEnemy |= sameRect(call.rect,enemies[0].rect);
        } else if(call.kind=="grid") {
            ++gridCount; grids &= call.depth==1; lastWorld=i;
            sawWorldExtentX |= call.rect.width==WORLD_W; sawWorldExtentY |= call.rect.height==WORLD_H;
        } else if(call.kind=="outline") {
            ++outlineCount; outlines &= call.depth==1; lastWorld=i; sawCase |= sameRect(call.rect,myCase.rect);
        } else if(call.kind=="text") {
            ++textCount; text &= call.depth==0; if(i<firstHUD) firstHUD=i;
            sawEnd |= call.text=="GAME OVER"; sawHP |= call.text.find("HP:")!=std::string::npos;
            sawCaseText |= call.text.find("scope probe")!=std::string::npos;
        } else if(call.kind=="panel") {
            ++panelCount; panels &= call.depth==0; if(i<firstHUD) firstHUD=i;
        }
    }
    CHECK("floor/walls/entities actually drawn in world scope",true,spriteCount>=WORLD_ROWS*WORLD_COLS+3 && sprites);
    CHECK("player drawn",true,sawPlayer); CHECK("bullet drawn",true,sawBullet); CHECK("enemy drawn",true,sawEnemy);
    CHECK("grid drawn in world scope",true,gridCount>0 && grids);
    CHECK("grid spans world X",true,sawWorldExtentX); CHECK("grid spans world Y",true,sawWorldExtentY);
    CHECK("debug bodies and cases in world scope",true,outlineCount>=6 && outlines && sawCase);
    CHECK("HUD and case text outside camera",true,textCount>=5 && text && sawHP && sawCaseText);
    CHECK("panels outside camera",true,panelCount>=3 && panels);
    CHECK("game-over text outside camera",true,sawEnd && text);
    CHECK("HUD comes after world",true,firstHUD>lastWorld);
    debugMode=oldDebug; myCase=oldCase; end(5);
}
static void walls() {
    if(!prerequisite(6,2)) return;
    begin(6); resetFixture(); CHECK("empty map",false,HitsWall({0,0,float(WORLD_W),float(WORLD_H)}));
    wall[5][7]=1; // x=280..320, y=200..240
    struct Case { const char* name; Rectangle rect; bool hit; };
    const Case cases[]={
        {"inside wall",{285,205,10,10},true},
        {"body overlaps with top left outside",{260,190,30,20},true},
        {"large non-square spanning wall",{100,150,400,120},true},
        {"touch left edge",{260,205,20,10},false},
        {"touch right edge",{320,205,20,10},false},
        {"touch upper edge",{285,180,10,20},false},
        {"touch lower edge",{285,240,10,20},false},
        {"corner only",{260,180,20,20},false},
        {"fractional overlap 0.25 pixel",{270.25f,205,10,10},true},
        {"fractional bottom overlap",{285,190.25f,10,10},true},
        {"zero width",{285,205,0,10},false},
        {"zero height",{285,205,10,0},false},
        {"empty nearby cell",{330,205,10,10},false},
    };
    for(const auto& c:cases) CHECK(c.name,c.hit,HitsWall(c.rect));
    wall[WORLD_ROWS-1][WORLD_COLS-1]=1;
    CHECK("last row/column wall",true,HitsWall({float(WORLD_W-10),float(WORLD_H-10),10,10}));
    std::memset(wall,0,sizeof(wall));
    CHECK("outside world is not an invented wall",false,HitsWall({-5,-5,2,2})); end(6);
}
static void moveCase(const char* name,Rectangle start,int key,int row,int col,float ex,float ey,int second=0) {
    resetFixture(); player.rect=start; if(row>=0) wall[row][col]=1;
    keyDown[key]=true; if(second) keyDown[second]=true; UpdatePlayer();
    CHECK(name,ex,player.rect.x); CHECK(name,ey,player.rect.y);
}
static void movement() {
    if(!prerequisite(7,6)) return;
    begin(7); resetFixture();
    CHECK("world accepts entire world",true,InsideWorld({0,0,float(WORLD_W),float(WORLD_H)}));
    CHECK("world rejects left",false,InsideWorld({-0.25f,100,20,20}));
    CHECK("world rejects top",false,InsideWorld({100,-0.25f,20,20}));
    CHECK("world rejects right body",false,InsideWorld({float(WORLD_W-10),100,10.25f,20}));
    CHECK("world rejects bottom body",false,InsideWorld({100,float(WORLD_H-10),20,10.25f}));
    CHECK("negative width invalid",false,InsideWorld({100,100,-1,20}));
    CHECK("negative height invalid",false,InsideWorld({100,100,20,-1}));
    const float remoteX=float(SCREEN_W+1), remoteY=float(SCREEN_H+1);
    const float roomX=float(WORLD_W)-remoteX, roomY=float(WORLD_H)-remoteY;
    // Shrink the probe to fit any expanded world without creating negative sizes.
    const float probeWidth=roomX>0?std::fmin(23.0f,roomX):1.0f;
    const float probeHeight=roomY>0?std::fmin(57.0f,roomY):1.0f;
    CHECK("beyond original screen matches world extent",roomX>0 && roomY>0,
          InsideWorld({remoteX,remoteY,probeWidth,probeHeight}));
    moveCase("right blocked",{240,200,40,40},KEY_D,5,7,240,200);
    moveCase("left blocked",{320,200,40,40},KEY_A,5,7,320,200);
    moveCase("down blocked",{280,160,40,40},KEY_S,5,7,280,160);
    moveCase("up blocked",{280,240,40,40},KEY_W,5,7,280,240);
    moveCase("X blocked Y slides",{240,200,40,40},KEY_D,5,7,240,205,KEY_S);
    moveCase("Y blocked X slides",{280,160,40,40},KEY_D,5,7,285,160,KEY_S);
    moveCase("right blocked up slides",{240,200,40,40},KEY_D,5,7,240,195,KEY_W);
    moveCase("left blocked down slides",{320,200,40,40},KEY_A,5,7,320,205,KEY_S);
    moveCase("left blocked up slides",{320,200,40,40},KEY_A,5,7,320,195,KEY_W);
    moveCase("up blocked right slides",{280,240,40,40},KEY_W,5,7,285,240,KEY_D);
    moveCase("down blocked left slides",{280,160,40,40},KEY_S,5,7,275,160,KEY_A);
    // X moves out of wall's horizontal band; stale X would reject Y.
    moveCase("Y uses newly accepted X",{315,160,5,40},KEY_D,5,7,320,165,KEY_S);
    // X moves into wall's horizontal band; stale X would accept Y.
    moveCase("updated X now makes Y blocked",{240,160,40,40},KEY_D,5,7,245,160,KEY_S);
    moveCase("empty right",{200,200,40,40},KEY_D,-1,0,205,200);
    moveCase("empty left",{200,200,40,40},KEY_A,-1,0,195,200);
    moveCase("empty down",{200,200,40,40},KEY_S,-1,0,200,205);
    moveCase("empty up",{200,200,40,40},KEY_W,-1,0,200,195);
    moveCase("left world limit",{0,100,40,40},KEY_A,-1,0,0,100);
    moveCase("right world limit",{float(WORLD_W-40),100,40,40},KEY_D,-1,0,WORLD_W-40,100);
    moveCase("top world limit",{100,0,40,40},KEY_W,-1,0,100,0);
    moveCase("bottom world limit",{100,float(WORLD_H-40),40,40},KEY_S,-1,0,100,WORLD_H-40);
    moveCase("cross original screen X",{float(SCREEN_W-40),100,40,40},KEY_D,-1,0,SCREEN_W-35,100);
    moveCase("cross original screen Y",{100,float(SCREEN_H-40),40,40},KEY_S,-1,0,100,SCREEN_H-35);
    resetFixture(); keyDown[KEY_A]=keyDown[KEY_D]=keyDown[KEY_W]=keyDown[KEY_S]=true;
    UpdatePlayer(); CHECK("opposite X cancels",200,player.rect.x); CHECK("opposite Y cancels",200,player.rect.y);
    for(float speed:{2.5f,7.0f}) {
        const int keys[]={KEY_D,KEY_A,KEY_S,KEY_W};
        for(int k:keys) {
            resetFixture(); player.speed=speed; keyDown[k]=true; UpdatePlayer();
            CHECK("configured speed X",200+(k==KEY_D?speed:k==KEY_A?-speed:0),player.rect.x);
            CHECK("configured speed Y",200+(k==KEY_S?speed:k==KEY_W?-speed:0),player.rect.y);
        }
    }
    moveCase("non-square left boundary",{0,100,23,57},KEY_A,-1,0,0,100);
    moveCase("non-square right boundary",{float(WORLD_W-23),100,23,57},KEY_D,-1,0,WORLD_W-23,100);
    moveCase("non-square top boundary",{100,0,23,57},KEY_W,-1,0,100,0);
    moveCase("non-square bottom boundary",{100,float(WORLD_H-57),23,57},KEY_S,-1,0,100,WORLD_H-57);
    resetFixture(); UpdatePlayer(); CHECK("no keys X",200,player.rect.x); CHECK("no keys Y",200,player.rect.y); end(7);
}
static int bulletCount() { int n=0; for(const auto& b:bullets) n+=b.active; return n; }
static void shots() {
    begin(8);
    const int keys[]={KEY_RIGHT,KEY_LEFT,KEY_UP,KEY_DOWN};
    for(int k:keys) {
        resetFixture(); keyPressed[k]=true; FireBullets();
        CHECK("one press allocates one bullet",1,bulletCount()); CHECK("one shot one sound",1,audioShots);
        CHECK("bullet points in requested direction",true,
            k==KEY_RIGHT?bullets[0].vx>0:k==KEY_LEFT?bullets[0].vx<0:k==KEY_UP?bullets[0].vy<0:bullets[0].vy>0);
        CHECK("shot starts at player center X",player.rect.x+player.rect.width/2,bullets[0].rect.x+bullets[0].rect.width/2);
        CHECK("shot starts at player center Y",player.rect.y+player.rect.height/2,bullets[0].rect.y+bullets[0].rect.height/2);
    }
    resetFixture(); for(int k:keys) keyPressed[k]=true; FireBullets();
    CHECK("combined presses still one bullet",1,bulletCount()); CHECK("combined presses still one sound",1,audioShots);
    resetFixture(); FireBullets(); CHECK("no press no sound",0,audioShots); CHECK("no press no bullet",0,bulletCount());
    resetFixture(); keyDown[KEY_RIGHT]=true; FireBullets(); CHECK("held without new press no sound",0,audioShots);
    resetFixture(); for(auto& b:bullets) b.active=true;
    keyPressed[KEY_RIGHT]=true; FireBullets(); CHECK("full pool no sound",0,audioShots); CHECK("full pool unchanged",MAX_BULLETS,bulletCount());
    resetFixture(); for(auto& b:bullets) b.active=true; bullets[7].active=false;
    keyPressed[KEY_UP]=true; FireBullets(); CHECK("nonfirst free slot allocated",true,bullets[7].active);
    CHECK("nonfirst slot direction",true,bullets[7].vy<0); CHECK("nonfirst slot one sound",1,audioShots); end(8);
}
static void placeEnemy(int slot,EnemyKind kind,int hp,bool active=true) {
    float size=ConfigOf(kind).size; enemies[slot]={{300,300,size,size},kind,hp,active};
}
static void hitOnce() { bullets[0]={{305,305,10,10},0,0,true}; HandleHits(); }
static void hits() {
    begin(9); resetFixture(); auto cfg=ConfigOf(EnemyKind::Heavy);
    placeEnemy(0,EnemyKind::Heavy,cfg.maxHp); hitOnce();
    CHECK("nonfatal hit sound",1,audioHits); CHECK("nonfatal loses one HP",cfg.maxHp-1,enemies[0].hp);
    CHECK("nonfatal remains active",true,enemies[0].active); CHECK("nonfatal no score",0,score);
    CHECK("hit consumes bullet",false,bullets[0].active); HandleHits(); CHECK("consumed bullet no repeat sound",1,audioHits);
    for(EnemyKind kind:{EnemyKind::Grunt,EnemyKind::Runner,EnemyKind::Heavy,EnemyKind::Elite}) {
        resetFixture(); auto c=ConfigOf(kind); placeEnemy(0,kind,c.maxHp);
        for(int n=0;n<c.maxHp;++n) hitOnce();
        CHECK("one sound per actual hit",c.maxHp,audioHits); CHECK("dies at configured HP",false,enemies[0].active);
        CHECK("death score from own config",c.score,score);
    }
    resetFixture(); placeEnemy(0,EnemyKind::Heavy,cfg.maxHp); bullets[0]={{10,10,10,10},0,0,true}; HandleHits();
    CHECK("no overlap no sound",0,audioHits); CHECK("no overlap unchanged HP",cfg.maxHp,enemies[0].hp);
    resetFixture(); placeEnemy(0,EnemyKind::Heavy,cfg.maxHp); bullets[0]={{305,305,10,10},0,0,false}; HandleHits();
    CHECK("inactive bullet no sound",0,audioHits); CHECK("inactive bullet unchanged HP",cfg.maxHp,enemies[0].hp);
    resetFixture(); placeEnemy(0,EnemyKind::Heavy,cfg.maxHp,false); hitOnce();
    CHECK("inactive enemy no sound",0,audioHits); CHECK("inactive enemy leaves bullet",true,bullets[0].active);
    resetFixture(); placeEnemy(0,EnemyKind::Heavy,cfg.maxHp); placeEnemy(1,EnemyKind::Heavy,cfg.maxHp); hitOnce();
    CHECK("overlapping enemies only one sound",1,audioHits);
    CHECK("overlapping enemies only one HP lost",2*cfg.maxHp-1,enemies[0].hp+enemies[1].hp); end(9);
}
int main(int argc, char** argv) {
    for(int i=1;i<argc;++i) {
        if(std::strcmp(argv[i],"--verbose")==0) verbose=true;
        else { std::fprintf(stderr,"Unknown argument: %s\nUsage: test-todos [--verbose]\n",argv[i]); return 2; }
    }
    mapConfiguration(); coordinates(); cameraInit(); cameraFollow(); drawing(); walls(); movement(); shots(); hits(); UnloadGameAudio();
    std::printf("\nTotal: %d passed, %d failed, %d blocked; Task 10 requires human review\n",passed,failed,blocked);
    return (failed || blocked)?1:0;
}
