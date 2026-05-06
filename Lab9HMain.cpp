#include <stdio.h>
#include <stdint.h>
#include <ti/devices/msp/msp.h>
#include "../inc/ST7735.h"
#include "../inc/Clock.h"
#include "../inc/LaunchPad.h"
#include "../inc/TExaS.h"
#include "../inc/Timer.h"
#include "../inc/JoyStick.h"
#include "../inc/SlidePot.h"
#include "LED.h"
#include "Switch.h"
#include "Sound.h"
#include "images/images.h"

extern "C" void __disable_irq(void);
extern "C" void __enable_irq(void);
extern "C" void TIMG12_IRQHandler(void);

#define COLOR_BLACK    0x0000
#define COLOR_WHITE    0xFFFF
#define COLOR_RED      0x00F8
#define COLOR_GREEN    0x07E0
#define COLOR_YELLOW   0x07FF
#define COLOR_GRAY     0x7BEF

#define GAME_TOP      20
#define GAME_BOT      130
#define GAME_H        (GAME_BOT - GAME_TOP)
#define COUNTER_W     14

#define CHEF_W        16
#define CHEF_H        24

#define FLOOR_X1      (COUNTER_W + 2)
#define FLOOR_Y1      (GAME_TOP + COUNTER_W)
#define FLOOR_X2      (128 - COUNTER_W - CHEF_W - 2)
#define FLOOR_Y2      (GAME_BOT - COUNTER_W - CHEF_H + 13)

#define STATION_W     14
#define STATION_H     20
#define STATION_RANGE 18
#define PICKUP_RANGE  25
#define PLATE_RANGE   18

#define ING_W         10
#define ING_H         10
#define MAX_INGREDIENTS  8

#define TXT(eng, esp) (langSpanish ? (char*)(esp) : (char*)(eng))


typedef enum { STATE_HOME, STATE_INSTRUCTIONS, STATE_PLAYING, STATE_GAMEOVER } GameState_t;
typedef enum { EASY, MEDIUM, HARD } Difficulty_t;

typedef enum {
  ING_LETTUCE = 0, ING_TOMATO, ING_CUCUMBER, ING_BREAD,
  ING_CHEESE, ING_APPLE, ING_BANANA, ING_MANGO,
  ING_TORTILLA, ING_ONION, ING_CARROT, ING_WRAP, ING_PEPPER,
  ING_BEEF_PATTY, ING_CHICKEN, ING_BROCCOLI,
  ING_EGG, ING_BUN, ING_PASTA, ING_RAMEN_NOODLES,
  ING_RICE, ING_WATER, ING_PORK, ING_GREEN_ONION, ING_CURRY_POWDER,
  ING_TYPE_COUNT
} IngType_t;

typedef enum { ACTION_NONE = 0, ACTION_CUT, ACTION_BOIL, ACTION_COOK } StationAction_t;

typedef struct {
  IngType_t type;
  uint8_t   active;
  uint8_t   cut, boiled, cooked;
  uint8_t   canCut, canBoil, canCook;
  int16_t   x, y;
} Ingredient_t;

typedef struct {
  IngType_t type;
  uint8_t   needCut;
  uint8_t   needBoiled;
  uint8_t   needCooked;
  uint8_t   placed;
} RecipeItem_t;

typedef struct {
  const char*  name;
  const char*  nameES;
  RecipeItem_t items[4];
  uint8_t      count;
  uint16_t     points;
} Recipe_t;


void PLL_Init(void);
uint8_t TExaS_LaunchPadLogicPB27PB26(void);
Difficulty_t ReadDifficulty(void);
StationAction_t NearStation(void);
uint8_t NearPlate(void);
void ApplyAction(StationAction_t action);
void PickNewRecipe(void);
void DrawHUD(void);
void DrawRecipePanel(void);
uint8_t TryPlateIngredient(void);
void DrawGameBackground(void);
void DrawStove(void);
void DrawKnife(void);
void DrawBoiler(void);
void DrawPlate(void);
void DrawIngredient(int x, int y, const uint16_t* sprite);
void DrawIngredientSprite(int x, int y, Ingredient_t& ing);
void EraseIngredientAt(int x, int y);
void DrawFloorIngredients(void);
void DrawHeldIngredient(void);
void EraseHeldIngredient(void);
void DrawChef(int x, int y);
void EraseChef(int x, int y);
void DrawHomeScreen(void);
void DrawGameOverScreen(void);
void DrawInstructionsPage(uint8_t page, Difficulty_t d);
void DrawIngPair(int16_t x, int16_t y, const uint16_t* raw, const uint16_t* processed, IngType_t type, const char* actionEN, const char* actionES);
void DrawIngSingle(int16_t x, int16_t y, const uint16_t* sprite, IngType_t type);
void SpawnIngredient(void);
void Game_Init(void);
uint32_t Random32(void);
uint32_t Random(uint32_t n);


SlidePot Sensor(1500, 0);

uint8_t langSpanish = 0;

volatile uint8_t  gameTick    = 0;
volatile uint8_t  spawnTimer  = 0;
volatile uint8_t  doSpawn     = 0;
volatile uint16_t gameSeconds = 0;
volatile uint8_t  tickCount   = 0;

uint16_t lastDrawnScore   = 0xFFFF;
uint16_t lastDrawnSeconds = 0xFFFF;

GameState_t  gameState   = STATE_HOME;
Difficulty_t difficulty  = EASY;
uint8_t      instrPage   = 0;

int16_t playerX, playerY;
int16_t stoveY, knifeY, boilerY;

Ingredient_t ingredients[MAX_INGREDIENTS];
int8_t       heldIngredient = -1;

StationAction_t swipeAction = ACTION_NONE;
uint16_t score = 0;


static Recipe_t easyRecipes[] = {
  { "Garden Salad", "Ensalada", {
      {ING_LETTUCE,  1,0,0,0},
      {ING_TOMATO,   1,0,0,0},
      {ING_CUCUMBER, 1,0,0,0},
    }, 3, 50 },
  { "Sandwich", "Sandwich", {
      {ING_BREAD,   0,0,0,0},
      {ING_TOMATO,  1,0,0,0},
      {ING_LETTUCE, 1,0,0,0},
      {ING_CHEESE,  1,0,0,0},
    }, 4, 50 },
  { "Fruit Bowl", "Ens. Frutas", {
      {ING_APPLE,  1,0,0,0},
      {ING_BANANA, 1,0,0,0},
      {ING_MANGO,  1,0,0,0},
    }, 3, 50 },
};

static Recipe_t mediumRecipes[] = {
  { "Burger", "Hamburguesa", {
      {ING_BUN,        0,0,0,0},
      {ING_BEEF_PATTY, 0,0,1,0},
      {ING_LETTUCE,    1,0,0,0},
      {ING_TOMATO,     1,0,0,0},
    }, 4, 100 },
  { "Omelette", "Tortilla", {
      {ING_EGG,    0,0,1,0},
      {ING_ONION,  1,0,0,0},
      {ING_PEPPER, 1,0,0,0},
    }, 3, 100 },
  { "Stir Fry", "Salteado", {
      {ING_CHICKEN,  1,0,1,0},
      {ING_BROCCOLI, 1,0,0,0},
      {ING_CARROT,   1,0,0,0},
    }, 3, 100 },
};

static Recipe_t hardRecipes[] = {
  { "Pasta", "Pasta", {
      {ING_PASTA,      0,1,0,0},
      {ING_TOMATO,     1,0,0,0},
      {ING_BEEF_PATTY, 0,0,1,0},
    }, 3, 150 },
  { "Ramen", "Ramen", {
      {ING_RAMEN_NOODLES, 0,1,0,0},
      {ING_EGG,           0,0,1,0},
      {ING_PORK,          0,0,1,0},
      {ING_GREEN_ONION,   1,0,0,0},
    }, 4, 150 },
  { "Curry", "Curry", {
      {ING_RICE,         0,1,0,0},
      {ING_CHICKEN,      0,0,1,0},
      {ING_ONION,        1,0,0,0},
      {ING_CURRY_POWDER, 0,0,0,0},
    }, 4, 150 },
};

Recipe_t currentRecipe;
int8_t   lastRecipeIdx = -1;

static uint8_t gCanCut[ING_TYPE_COUNT]  = {1,1,1,0,1,1,1,1,0,1,1,0,1,0,1,1,0,0,0,0,0,0,0,1,0};
static uint8_t gCanBoil[ING_TYPE_COUNT] = {0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,1,0,0,0,0};
static uint8_t gCanCook[ING_TYPE_COUNT] = {0,0,0,0,0,0,0,0,0,0,0,0,0,1,1,0,1,0,0,0,0,0,1,0,0};

static const char* ingNamesEN[ING_TYPE_COUNT] = {
  "Let","Tom","Cuc","Brd","Che","App","Ban","Man","Tor","Oni","Car","Wrp","Pep",
  "Bef","Chk","Brc","Egg","Bun","Pst","Rmn","Rce","Wtr","Prk","GOn","Spc"
};
static const char* ingNamesES[ING_TYPE_COUNT] = {
  "Lec","Tom","Pep","Pan","Que","Mzn","Pla","Man","Tor","Ceb","Zah","Env","Pim",
  "Res","Pol","Bro","Hue","Pan","Pas","Ram","Arr","Agu","Cer","CbV","Esp"
};
#define ingName(i) (langSpanish ? ingNamesES[i] : ingNamesEN[i])

static const uint16_t* ingCutSprite[ING_TYPE_COUNT] = {
  cut_lettuce, cut_tomato, cut_cucumber, bread,
  cut_cheese, cut_apple, banana, cut_mango,
  tortilla, onion, cut_carrots, wrap, cut_pepper,
  cooked_beef_patty, cooked_cut_chicken, cut_brocolli,
  cooked_egg, bun, boiled_pasta, boiled_ramen_noodles,
  boiled_rice, boiled_water, cooked_pork, cut_green_onions, curry_powder
};
static const uint16_t* ingRawSprite[ING_TYPE_COUNT] = {
  uncut_lettuce, uncut_tomato, uncut_cucumber, bread,
  cheese, apple, uncut_banana, mango,
  tortilla, uncut_onion, carrot, wrap, pepper,
  raw_beef_patty, uncut_chicken, uncut_brocolli,
  raw_egg, bun, raw_pasta, raw_ramen_noodles,
  raw_rice, water, raw_pork, uncut_green_onion, curry_powder
};


uint32_t M = 1;
uint32_t Random32(void){ M = 1664525*M + 1013904223; return M; }
uint32_t Random(uint32_t n){ return (Random32() >> 16) % n; }

void PLL_Init(void){ Clock_Init80MHz(0); }

uint8_t TExaS_LaunchPadLogicPB27PB26(void){
  return (0x80 | ((GPIOB->DOUT31_0 >> 26) & 0x03));
}

void TIMG12_IRQHandler(void){
  if((TIMG12->CPU_INT.IIDX) == 1){
    GPIOB->DOUTTGL31_0 = GREEN;
    if(gameState == STATE_PLAYING){
      spawnTimer++;
      if(spawnTimer >= 60){ spawnTimer = 0; doSpawn = 1; }
      tickCount++;
      if(tickCount >= 30){ tickCount = 0; if(gameSeconds > 0) gameSeconds--; }
    }
    gameTick = 1;
    GPIOB->DOUTTGL31_0 = GREEN;
  }
}

Difficulty_t ReadDifficulty(void){
  uint32_t raw = Sensor.In();
  if(raw > 2730){
    LED_Off(17); LED_Off(16); LED_On(15);  return EASY;
  } else if(raw > 1365){
    LED_Off(17); LED_On(16);  LED_Off(15); return MEDIUM;
  } else {
    LED_On(17);  LED_Off(16); LED_Off(15); return HARD;
  }
}

StationAction_t NearStation(void){
  int16_t chefLeft  = playerX;
  int16_t chefRight = playerX + CHEF_W;
  int16_t chefTop   = playerY;
  int16_t chefBot   = playerY + CHEF_H;

  if(difficulty >= MEDIUM &&
     chefLeft <= COUNTER_W + STATION_RANGE &&
     chefBot >= stoveY && chefTop <= stoveY + STATION_H)
    return ACTION_COOK;

  if(difficulty >= HARD &&
     chefLeft <= COUNTER_W + STATION_RANGE &&
     chefBot >= boilerY && chefTop <= boilerY + STATION_H)
    return ACTION_BOIL;

  if(chefRight >= 128 - COUNTER_W - STATION_RANGE &&
     chefBot >= knifeY && chefTop <= knifeY + STATION_H)
    return ACTION_CUT;

  return ACTION_NONE;
}

uint8_t NearPlate(void){
  int16_t plateX = 128 - STATION_W;
  int16_t plateY = GAME_BOT - STATION_W;
  int16_t dx = (playerX + CHEF_W/2) - (plateX + STATION_W/2); if(dx<0) dx=-dx;
  int16_t dy = (playerY + CHEF_H/2) - (plateY + STATION_W/2); if(dy<0) dy=-dy;
  return (dx < PLATE_RANGE && dy < PLATE_RANGE);
}

void ApplyAction(StationAction_t action){
  if(heldIngredient < 0) return;
  Ingredient_t& ing = ingredients[heldIngredient];
  if(action == ACTION_CUT  && ing.canCut)  ing.cut    = 1;
  if(action == ACTION_BOIL && ing.canBoil) ing.boiled = 1;
  if(action == ACTION_COOK && ing.canCook) ing.cooked = 1;
}

void PickNewRecipe(void){
  uint8_t idx;
  if(difficulty == EASY){
    do { idx = Random(3); } while(idx == lastRecipeIdx);
    lastRecipeIdx = idx; currentRecipe = easyRecipes[idx];
  } else if(difficulty == MEDIUM){
    do { idx = Random(3); } while(idx == lastRecipeIdx);
    lastRecipeIdx = idx; currentRecipe = mediumRecipes[idx];
  } else {
    do { idx = Random(3); } while(idx == lastRecipeIdx);
    lastRecipeIdx = idx; currentRecipe = hardRecipes[idx];
  }
  for(int i = 0; i < currentRecipe.count; i++)
    currentRecipe.items[i].placed = 0;
}

void DrawHUD(void){
  if(gameSeconds == lastDrawnSeconds && score == lastDrawnScore) return;
  lastDrawnSeconds = gameSeconds;
  lastDrawnScore   = score;
  for(int y = 0; y < GAME_TOP; y++)
    for(int x = 0; x < 128; x++)
      ST7735_DrawPixel(x, y, Kitchen[y*128+x]);
  ST7735_SetTextColor(COLOR_BLACK);
  ST7735_SetCursor(0, 0);
  ST7735_OutString(TXT("Score:", "Pts:"));
  ST7735_OutUDec(score);
  uint8_t mins = gameSeconds / 60;
  uint8_t secs = gameSeconds % 60;
  ST7735_SetCursor(16, 0);
  ST7735_OutUDec(mins);
  ST7735_OutChar(':');
  if(secs < 10) ST7735_OutChar('0');
  ST7735_OutUDec(secs);
}

void DrawRecipePanel(void){
  for(int y = GAME_BOT; y < 160; y++)
    for(int x = 0; x < 128; x++)
      ST7735_DrawPixel(x, y, Kitchen[y*128+x]);
  ST7735_SetTextColor(COLOR_BLACK);
  ST7735_SetCursor(0, 13);
  ST7735_OutString((char*)(langSpanish ? currentRecipe.nameES : currentRecipe.name));
  int16_t slotW = 128 / currentRecipe.count;
  for(int i = 0; i < currentRecipe.count; i++){
    RecipeItem_t& item = currentRecipe.items[i];
    int16_t cx = i * slotW + slotW/2;
    int16_t sx = cx - ING_W/2;
    int16_t sy = GAME_BOT + 12;
    const uint16_t* spr;
    if(item.needCooked || item.needBoiled) spr = ingCutSprite[item.type];
    else if(item.needCut)                  spr = ingCutSprite[item.type];
    else                                   spr = ingRawSprite[item.type];
    if(item.placed){
      ST7735_FillRect(sx, sy, ING_W, ING_H, COLOR_GRAY);
    } else {
      for(int row = 0; row < ING_H; row++)
        for(int col = 0; col < ING_W; col++){
          uint16_t c = spr[row * ING_W + col];
          if(c == 0x0840 || c == 0x0820) continue;
          ST7735_DrawPixel(sx + col, sy + row, c);
        }
    }
    ST7735_SetCursor(cx/6 - 1, 15);
    ST7735_SetTextColor(item.placed ? COLOR_GREEN : COLOR_BLACK);
    ST7735_OutString((char*)ingName(item.type));
  }
}

uint8_t TryPlateIngredient(void){
  if(heldIngredient < 0) return 0;
  Ingredient_t& ing = ingredients[heldIngredient];
  for(int i = 0; i < currentRecipe.count; i++){
    RecipeItem_t& item = currentRecipe.items[i];
    if(item.placed) continue;
    if(item.type != ing.type) continue;
    if(item.needCut    && !ing.cut)    continue;
    if(item.needBoiled && !ing.boiled) continue;
    if(item.needCooked && !ing.cooked) continue;
    item.placed = 1;
    EraseHeldIngredient();
    ingredients[heldIngredient].active = 0;
    heldIngredient = -1;
    DrawChef(playerX, playerY);
    DrawRecipePanel();
    uint8_t done = 1;
    for(int j = 0; j < currentRecipe.count; j++)
      if(!currentRecipe.items[j].placed){ done = 0; break; }
    if(done){
      Sound_Ding();
      for(int f = 0; f < 3; f++){
        LED_On(17); LED_On(16); LED_On(15);
        for(volatile int d = 0; d < 400000; d++){}
        LED_Off(17); LED_Off(16); LED_Off(15);
        for(volatile int d = 0; d < 400000; d++){}
      }
      if(difficulty == EASY)        LED_On(15);
      else if(difficulty == MEDIUM) LED_On(16);
      else                          LED_On(17);
      score += currentRecipe.points;
      lastDrawnScore = 0xFFFF;
      DrawHUD();
      PickNewRecipe();
      DrawRecipePanel();
      return 1;
    }
    return 0;
  }
  return 0;
}

void DrawGameBackground(void){
  for(int y = 0; y < 160; y++)
    for(int x = 0; x < 128; x++)
      ST7735_DrawPixel(x, y, Kitchen[y * 128 + x]);
}

void DrawStove(void){
  for(int row = 0; row < STATION_H; row++)
    for(int col = 0; col < STATION_W; col++)
      ST7735_DrawPixel(col, stoveY + row, Stove[row * STATION_W + col]);
}

void DrawKnife(void){
  for(int row = 0; row < STATION_H; row++)
    for(int col = 0; col < STATION_W; col++)
      ST7735_DrawPixel(128 - STATION_W + col, knifeY + row, Knife[row * STATION_W + col]);
}

void DrawBoiler(void){
  for(int row = 0; row < STATION_H; row++)
    for(int col = 0; col < STATION_W; col++){
      uint16_t c = Boiler[row * STATION_W + col];
      if(c == 0x0000) continue;
      ST7735_DrawPixel(col, boilerY + row, c);
    }
}

void DrawPlate(void){
  for(int row = 0; row < STATION_W; row++)
    for(int col = 0; col < STATION_W; col++)
      ST7735_DrawPixel(128 - STATION_W + col, GAME_BOT - STATION_W + row, Plate[row * STATION_W + col]);
}

void DrawIngredient(int x, int y, const uint16_t* sprite){
  for(int row = 0; row < ING_H; row++)
    for(int col = 0; col < ING_W; col++){
      uint16_t c = sprite[row * ING_W + col];
      if(c == 0x0840 || c == 0x0820 || c == 0x0000) continue;
      ST7735_DrawPixel(x + col, y + row, c);
    }
}

void DrawIngredientSprite(int x, int y, Ingredient_t& ing){
  switch(ing.type){
    case ING_LETTUCE:       ing.cut    ? DrawIngredient(x,y,cut_lettuce)          : DrawIngredient(x,y,uncut_lettuce);     break;
    case ING_TOMATO:        ing.cut    ? DrawIngredient(x,y,cut_tomato)           : DrawIngredient(x,y,uncut_tomato);      break;
    case ING_CUCUMBER:      ing.cut    ? DrawIngredient(x,y,cut_cucumber)         : DrawIngredient(x,y,uncut_cucumber);    break;
    case ING_BREAD:         DrawIngredient(x,y,bread);                                                                      break;
    case ING_CHEESE:        ing.cut    ? DrawIngredient(x,y,cut_cheese)           : DrawIngredient(x,y,cheese);            break;
    case ING_APPLE:         ing.cut    ? DrawIngredient(x,y,cut_apple)            : DrawIngredient(x,y,apple);             break;
    case ING_BANANA:        ing.cut    ? DrawIngredient(x,y,banana)               : DrawIngredient(x,y,uncut_banana);      break;
    case ING_MANGO:         ing.cut    ? DrawIngredient(x,y,cut_mango)            : DrawIngredient(x,y,mango);             break;
    case ING_TORTILLA:      DrawIngredient(x,y,tortilla);                                                                   break;
    case ING_ONION:         ing.cut    ? DrawIngredient(x,y,onion)                : DrawIngredient(x,y,uncut_onion);       break;
    case ING_CARROT:        ing.cut    ? DrawIngredient(x,y,cut_carrots)          : DrawIngredient(x,y,carrot);            break;
    case ING_WRAP:          DrawIngredient(x,y,wrap);                                                                       break;
    case ING_PEPPER:        ing.cut    ? DrawIngredient(x,y,cut_pepper)           : DrawIngredient(x,y,pepper);            break;
    case ING_BEEF_PATTY:    ing.cooked ? DrawIngredient(x,y,cooked_beef_patty)    : DrawIngredient(x,y,raw_beef_patty);    break;
    case ING_CHICKEN:       ing.cooked ? DrawIngredient(x,y,cooked_cut_chicken)   : (ing.cut ? DrawIngredient(x,y,cut_raw_chicken) : DrawIngredient(x,y,uncut_chicken)); break;
    case ING_BROCCOLI:      ing.cut    ? DrawIngredient(x,y,cut_brocolli)         : DrawIngredient(x,y,uncut_brocolli);    break;
    case ING_EGG:           ing.cooked ? DrawIngredient(x,y,cooked_egg)           : DrawIngredient(x,y,raw_egg);           break;
    case ING_BUN:           DrawIngredient(x,y,bun);                                                                        break;
    case ING_PASTA:         ing.boiled ? DrawIngredient(x,y,boiled_pasta)         : DrawIngredient(x,y,raw_pasta);         break;
    case ING_RAMEN_NOODLES: ing.boiled ? DrawIngredient(x,y,boiled_ramen_noodles) : DrawIngredient(x,y,raw_ramen_noodles); break;
    case ING_RICE:          ing.boiled ? DrawIngredient(x,y,boiled_rice)          : DrawIngredient(x,y,raw_rice);          break;
    case ING_WATER:         ing.boiled ? DrawIngredient(x,y,boiled_water)         : DrawIngredient(x,y,water);             break;
    case ING_PORK:          ing.cooked ? DrawIngredient(x,y,cooked_pork)          : DrawIngredient(x,y,raw_pork);          break;
    case ING_GREEN_ONION:   ing.cut    ? DrawIngredient(x,y,cut_green_onions)     : DrawIngredient(x,y,uncut_green_onion); break;
    case ING_CURRY_POWDER:  DrawIngredient(x,y,curry_powder);                                                               break;
    default: break;
  }
}

void EraseIngredientAt(int x, int y){
  for(int row = 0; row < ING_H; row++)
    for(int col = 0; col < ING_W; col++){
      int sx = x+col, sy = y+row;
      ST7735_DrawPixel(sx, sy, Kitchen[sy*128+sx]);
    }
}

void DrawFloorIngredients(void){
  for(int i = 0; i < MAX_INGREDIENTS; i++)
    if(ingredients[i].active && i != heldIngredient)
      DrawIngredientSprite(ingredients[i].x, ingredients[i].y, ingredients[i]);
}

void DrawHeldIngredient(void){
  if(heldIngredient < 0) return;
  int hx = playerX + CHEF_W/2 - ING_W/2;
  int hy = playerY - ING_H - 1;
  if(hy < GAME_TOP) hy = GAME_TOP;
  DrawIngredientSprite(hx, hy, ingredients[heldIngredient]);
}

void EraseHeldIngredient(void){
  if(heldIngredient < 0) return;
  int hx = playerX + CHEF_W/2 - ING_W/2;
  int hy = playerY - ING_H - 1;
  if(hy < GAME_TOP) hy = GAME_TOP;
  EraseIngredientAt(hx, hy);
}

void DrawChef(int x, int y){
  for(int row = 0; row < 24; row++)
    for(int col = 0; col < 16; col++){
      uint16_t c = Chef[row*16+col];
      if(c == 0x0000) continue;
      ST7735_DrawPixel(x+col, y+row, c);
    }
}

void EraseChef(int x, int y){
  for(int row = 0; row < 24; row++)
    for(int col = 0; col < 16; col++){
      int sx = x+col, sy = y+row;
      ST7735_DrawPixel(sx, sy, Kitchen[sy*128+sx]);
    }
}

void DrawIngPair(int16_t x, int16_t y,
                 const uint16_t* raw, const uint16_t* processed,
                 IngType_t type, const char* actionEN, const char* actionES){
  for(int row = 0; row < ING_H; row++)
    for(int col = 0; col < ING_W; col++){
      uint16_t c = raw[row*ING_W+col];
      if(c == 0x0840 || c == 0x0820 || c == 0x0000) continue;
      ST7735_DrawPixel(x+col, y+row, c);
    }
  ST7735_DrawChar(x+11, y+2, '>', COLOR_WHITE, COLOR_BLACK, 1);
  for(int row = 0; row < ING_H; row++)
    for(int col = 0; col < ING_W; col++){
      uint16_t c = processed[row*ING_W+col];
      if(c == 0x0840 || c == 0x0820 || c == 0x0000) continue;
      ST7735_DrawPixel(x+20+col, y+row, c);
    }
  int16_t textCol = x / 6;
  int16_t textRow = (y + 12) / 10;
  ST7735_SetTextColor(COLOR_WHITE);
  ST7735_SetCursor(textCol, textRow);
  ST7735_OutString((char*)ingName(type));
  ST7735_SetCursor(textCol + 4, textRow);
  ST7735_OutString((char*)(langSpanish ? actionES : actionEN));
}

void DrawIngSingle(int16_t x, int16_t y, const uint16_t* sprite, IngType_t type){
  for(int row = 0; row < ING_H; row++)
    for(int col = 0; col < ING_W; col++){
      uint16_t c = sprite[row*ING_W+col];
      if(c == 0x0840 || c == 0x0820 || c == 0x0000) continue;
      ST7735_DrawPixel(x+col, y+row, c);
    }
  int16_t textCol = x / 6;
  int16_t textRow = (y + 12) / 10;
  ST7735_SetTextColor(COLOR_WHITE);
  ST7735_SetCursor(textCol, textRow);
  ST7735_OutString((char*)ingName(type));
}

void DrawInstructionsPage(uint8_t page, Difficulty_t d){
  ST7735_FillScreen(COLOR_BLACK);
  ST7735_SetTextColor(COLOR_GRAY);
  ST7735_SetCursor(0, 15);  ST7735_OutString((char*)"<L      R>");
  ST7735_SetCursor(16, 15); ST7735_OutUDec(page + 1);
  ST7735_OutChar('/');
  ST7735_OutUDec(9);

  switch(page){
    case 0:
      ST7735_SetTextColor(COLOR_WHITE);
      ST7735_SetCursor(3, 0);
      ST7735_OutString(TXT("DIFFICULTY", "DIFICULTAD"));
      ST7735_FillRect(0, 11, 128, 1, COLOR_GRAY);
      {
        uint16_t dColor; const char* dName; const char* dTime; uint16_t dPts;
        switch(d){
          case EASY:   dColor=COLOR_GREEN;  dName=TXT("EASY",  "FACIL");  dTime="2:00"; dPts=50;  break;
          case MEDIUM: dColor=COLOR_YELLOW; dName=TXT("MEDIUM","MEDIO");  dTime="1:30"; dPts=100; break;
          default:     dColor=COLOR_RED;    dName=TXT("HARD",  "DIFICIL");dTime="1:00"; dPts=150; break;
        }
        ST7735_SetTextColor(dColor);
        ST7735_SetCursor(6, 2); ST7735_OutString((char*)dName);
        ST7735_SetTextColor(COLOR_WHITE);
        ST7735_SetCursor(0, 4); ST7735_OutString(TXT("Time:   ", "Tiempo: "));
        ST7735_OutString((char*)dTime);
        ST7735_SetCursor(0, 5); ST7735_OutString(TXT("Points: ", "Puntos: "));
        ST7735_OutUDec(dPts);
        ST7735_OutString(TXT(" per dish", " por plato"));
      }
      ST7735_SetTextColor(COLOR_GRAY);
      ST7735_SetCursor(0, 7); ST7735_OutString(TXT("Slide pot to change", "Desliza para cambiar"));
      ST7735_SetTextColor(COLOR_WHITE);
      ST7735_SetCursor(0, 10); ST7735_OutString(TXT("Bot btn: Home",    "Btn abajo: Inicio"));
      ST7735_SetCursor(0, 11); ST7735_OutString(TXT("Top btn: START!",  "Btn arriba: INICIAR"));
      ST7735_SetCursor(0, 13); ST7735_OutString(TXT("Instructions ->", "Instrucciones ->"));
      break;

    case 1:
      ST7735_SetTextColor(COLOR_WHITE);
      ST7735_SetCursor(langSpanish ? 3 : 4, 0);
      ST7735_OutString(TXT("HOW TO PLAY", "COMO JUGAR"));
      ST7735_FillRect(0, 11, 128, 1, COLOR_GRAY);
      ST7735_SetCursor(0, 2);  ST7735_OutString(TXT("Complete recipes to",  "Completa recetas"));
      ST7735_SetCursor(0, 3);  ST7735_OutString(TXT("earn points!",         "para ganar puntos!"));
      ST7735_SetCursor(0, 5);  ST7735_OutString(TXT("Move:    Joystick",    "Mover:   Joystick"));
      ST7735_SetCursor(0, 6);  ST7735_OutString(TXT("Pick/Put: Top btn",    "Agarrar: Btn arriba"));
      ST7735_SetCursor(0, 7);  ST7735_OutString(TXT("Trash:   Bot btn",     "Tirar:   Btn abajo"));
      ST7735_SetCursor(0, 8);  ST7735_OutString(TXT("Station: Right btn",   "Estacion:Btn derecha"));
      ST7735_SetCursor(0, 10); ST7735_OutString(TXT("Max 8 items on floor", "Max 8 items en piso"));
      ST7735_SetCursor(0, 11); ST7735_OutString(TXT("Trash unwanted ones!", "Tira los no usados!"));
      ST7735_SetCursor(0, 12); ST7735_OutString(TXT("Near plate + Right",   "Cerca del plato+Der"));
      ST7735_SetCursor(0, 13); ST7735_OutString(TXT("to submit ingredient", "para entregar item"));
      break;

    case 2:
      ST7735_SetTextColor(COLOR_WHITE);
      ST7735_SetCursor(3, 0); ST7735_OutString(TXT("CUT ITEMS  1/3", "CORTAR  1/3"));
      ST7735_FillRect(0, 11, 128, 1, COLOR_GRAY);
      DrawIngPair(4,  18, uncut_lettuce, cut_lettuce,   ING_LETTUCE,  "Cut","Cor");
      DrawIngPair(68, 18, uncut_tomato,  cut_tomato,    ING_TOMATO,   "Cut","Cor");
      DrawIngPair(4,  60, uncut_cucumber,cut_cucumber,  ING_CUCUMBER, "Cut","Cor");
      DrawIngPair(68, 60, cheese,        cut_cheese,    ING_CHEESE,   "Cut","Cor");
      ST7735_DrawBitmap(57, 105, Knife, STATION_W, STATION_H);
      break;

    case 3:
      ST7735_SetTextColor(COLOR_WHITE);
      ST7735_SetCursor(3, 0); ST7735_OutString(TXT("CUT ITEMS  2/3", "CORTAR  2/3"));
      ST7735_FillRect(0, 11, 128, 1, COLOR_GRAY);
      DrawIngPair(4,  18, apple,        cut_apple,     ING_APPLE,  "Cut","Cor");
      DrawIngPair(68, 18, uncut_banana, banana,        ING_BANANA, "Cut","Cor");
      DrawIngPair(4,  60, mango,        cut_mango,     ING_MANGO,  "Cut","Cor");
      DrawIngPair(68, 60, uncut_onion,  onion,         ING_ONION,  "Cut","Cor");
      ST7735_DrawBitmap(57, 105, Knife, STATION_W, STATION_H);
      break;

    case 4:
      ST7735_SetTextColor(COLOR_WHITE);
      ST7735_SetCursor(3, 0); ST7735_OutString(TXT("CUT ITEMS  3/3", "CORTAR  3/3"));
      ST7735_FillRect(0, 11, 128, 1, COLOR_GRAY);
      DrawIngPair(4,  18, carrot,            cut_carrots,      ING_CARROT,      "Cut","Cor");
      DrawIngPair(68, 18, pepper,            cut_pepper,       ING_PEPPER,      "Cut","Cor");
      DrawIngPair(4,  60, uncut_brocolli,    cut_brocolli,     ING_BROCCOLI,    "Cut","Cor");
      DrawIngPair(68, 60, uncut_green_onion, cut_green_onions, ING_GREEN_ONION, "Cut","Cor");
      ST7735_DrawBitmap(57, 105, Knife, STATION_W, STATION_H);
      break;

    case 5:
      ST7735_SetTextColor(COLOR_WHITE);
      ST7735_SetCursor(3, 0); ST7735_OutString(TXT("COOK ITEMS", "COCINAR"));
      ST7735_FillRect(0, 11, 128, 1, COLOR_GRAY);
      DrawIngPair(4,  18, raw_beef_patty, cooked_beef_patty,  ING_BEEF_PATTY, "Cook","Coc");
      DrawIngPair(68, 18, raw_egg,        cooked_egg,         ING_EGG,        "Cook","Coc");
      DrawIngPair(4,  60, raw_pork,       cooked_pork,        ING_PORK,       "Cook","Coc");
      DrawIngPair(68, 60, uncut_chicken,  cooked_cut_chicken, ING_CHICKEN,    "Cook","Coc");
      ST7735_DrawBitmap(57, 105, Stove, STATION_W, STATION_H);
      break;

    case 6:
      ST7735_SetTextColor(COLOR_WHITE);
      ST7735_SetCursor(3, 0); ST7735_OutString(TXT("BOIL ITEMS", "HERVIR"));
      ST7735_FillRect(0, 11, 128, 1, COLOR_GRAY);
      DrawIngPair(4,  18, raw_pasta,         boiled_pasta,         ING_PASTA,         "Boil","Her");
      DrawIngPair(68, 18, raw_ramen_noodles, boiled_ramen_noodles, ING_RAMEN_NOODLES, "Boil","Her");
      DrawIngPair(4,  60, raw_rice,          boiled_rice,          ING_RICE,          "Boil","Her");
      ST7735_DrawBitmap(57, 105, Boiler, STATION_W, STATION_H);
      break;

    case 7:
      ST7735_SetTextColor(COLOR_WHITE);
      ST7735_SetCursor(2, 0); ST7735_OutString(TXT("NO ACTION NEEDED", "SIN ACCION"));
      ST7735_FillRect(0, 11, 128, 1, COLOR_GRAY);
      DrawIngSingle(4,  18, bread,        ING_BREAD);
      DrawIngSingle(68, 18, bun,          ING_BUN);
      DrawIngSingle(4,  60, tortilla,     ING_TORTILLA);
      DrawIngSingle(68, 60, curry_powder, ING_CURRY_POWDER);
      break;

    case 8:
      ST7735_SetTextColor(COLOR_WHITE);
      ST7735_SetCursor(5, 0); ST7735_OutString(TXT("RECIPES", "RECETAS"));
      ST7735_FillRect(0, 11, 128, 1, COLOR_GRAY);
      ST7735_SetTextColor(COLOR_GREEN);
      ST7735_SetCursor(0, 2); ST7735_OutString(TXT("EASY (50pts):", "FACIL (50pts):"));
      ST7735_SetTextColor(COLOR_WHITE);
      ST7735_SetCursor(0, 3); ST7735_OutString(TXT("Garden Salad",        "Ensalada"));
      ST7735_SetCursor(0, 4); ST7735_OutString(TXT("Sandwich, Fruit Bowl","Sandwich, Ens.Frutas"));
      ST7735_SetTextColor(COLOR_YELLOW);
      ST7735_SetCursor(0, 6); ST7735_OutString(TXT("MEDIUM (100pts):", "MEDIO (100pts):"));
      ST7735_SetTextColor(COLOR_WHITE);
      ST7735_SetCursor(0, 7); ST7735_OutString(TXT("Burger, Omelette",    "Hamburguesa, Tortilla"));
      ST7735_SetCursor(0, 8); ST7735_OutString(TXT("Stir Fry",            "Salteado"));
      ST7735_SetTextColor(COLOR_RED);
      ST7735_SetCursor(0, 10); ST7735_OutString(TXT("HARD (150pts):", "DIFICIL (150pts):"));
      ST7735_SetTextColor(COLOR_WHITE);
      ST7735_SetCursor(0, 11); ST7735_OutString(TXT("Pasta, Ramen", "Pasta, Ramen"));
      ST7735_SetCursor(0, 12); ST7735_OutString(TXT("Curry",        "Curry"));
      break;
  }
}

void DrawHomeScreen(void){
  for(int y = 0; y < 160; y++)
    for(int x = 0; x < 128; x++)
      ST7735_DrawPixel(x, y, HomeScreen[y*128+x]);
  ST7735_SetTextColor(COLOR_WHITE);
  ST7735_SetCursor(0, 15);
  ST7735_OutString(langSpanish ? (char*)"ESP | Left:ENG" : (char*)"ENG | Left:ESP");
}

void DrawGameOverScreen(void){
  for(int y = 0; y < 160; y++)
    for(int x = 0; x < 128; x++)
      ST7735_DrawPixel(x, y, Kitchen[y*128+x]);
  ST7735_SetTextColor(COLOR_BLACK);
  ST7735_SetCursor(langSpanish ? 3 : 6, 4);
  ST7735_OutString(TXT("GAME OVER", "FIN DEL JUEGO"));
  ST7735_SetCursor(langSpanish ? 3 : 5, 6);
  ST7735_OutString(TXT("Your Score:", "Tu Puntuacion:"));
  ST7735_SetCursor(10, 8);
  ST7735_OutUDec(score);
  ST7735_FillRect(19, 95, 90, 1, COLOR_BLACK);
  ST7735_SetCursor(langSpanish ? 1 : 3, 10);
  ST7735_OutString(TXT("Press any button", "Presiona un boton"));
  ST7735_SetCursor(langSpanish ? 2 : 4, 11);
  ST7735_OutString(TXT("to play again", "para jugar de nuevo"));
}

void SpawnIngredient(void){
  uint8_t activeCount = 0;
  for(int i = 0; i < MAX_INGREDIENTS; i++)
    if(ingredients[i].active) activeCount++;
  if(activeCount >= MAX_INGREDIENTS) return;

  int slot = -1;
  for(int i = 0; i < MAX_INGREDIENTS; i++)
    if(!ingredients[i].active && i != heldIngredient){ slot = i; break; }
  if(slot < 0) return;

  IngType_t t;
  uint8_t biased = 0;
  if(Random(10) < 6){
    uint8_t unplacedCount = 0;
    for(int i = 0; i < currentRecipe.count; i++)
      if(!currentRecipe.items[i].placed) unplacedCount++;
    if(unplacedCount > 0){
      uint8_t pick = Random(unplacedCount);
      uint8_t seen = 0;
      for(int i = 0; i < currentRecipe.count; i++){
        if(!currentRecipe.items[i].placed){
          if(seen == pick){ t = currentRecipe.items[i].type; biased = 1; break; }
          seen++;
        }
      }
    }
  }
  if(!biased) t = (IngType_t)Random(ING_TYPE_COUNT);

  ingredients[slot].type    = t;
  ingredients[slot].active  = 1;
  ingredients[slot].cut     = 0;
  ingredients[slot].boiled  = 0;
  ingredients[slot].cooked  = 0;
  ingredients[slot].canCut  = gCanCut[t];
  ingredients[slot].canBoil = gCanBoil[t];
  ingredients[slot].canCook = gCanCook[t];

  int16_t floorW = FLOOR_X2 - FLOOR_X1;
  int16_t floorH = FLOOR_Y2 - FLOOR_Y1;
  ingredients[slot].x = FLOOR_X1 + Random(floorW - ING_W);
  ingredients[slot].y = FLOOR_Y1 + Random(floorH - ING_H);
  for(int attempt = 0; attempt < 50; attempt++){
    int16_t tx = FLOOR_X1 + Random(floorW - ING_W);
    int16_t ty = FLOOR_Y1 + Random(floorH - ING_H);
    uint8_t ok = 1;
    for(int j = 0; j < MAX_INGREDIENTS; j++){
      if(j == slot || !ingredients[j].active) continue;
      int16_t dx = tx - ingredients[j].x; if(dx<0) dx=-dx;
      int16_t dy = ty - ingredients[j].y; if(dy<0) dy=-dy;
      if(dx < 20 && dy < 20){ ok = 0; break; }
    }
    if(ok){ ingredients[slot].x = tx; ingredients[slot].y = ty; break; }
  }
  DrawIngredientSprite(ingredients[slot].x, ingredients[slot].y, ingredients[slot]);
}

void Game_Init(void){
  playerX = (FLOOR_X1 + FLOOR_X2) / 2;
  playerY = (FLOOR_Y1 + FLOOR_Y2) / 2;

  stoveY  = GAME_TOP + 20;
  boilerY = GAME_TOP + (GAME_H / 2) + 10;
  knifeY  = GAME_TOP + 30;

  heldIngredient   = -1;
  swipeAction      = ACTION_NONE;
  spawnTimer       = 0;
  doSpawn          = 0;
  score            = 0;
  lastRecipeIdx    = -1;
  lastDrawnScore   = 0xFFFF;
  lastDrawnSeconds = 0xFFFF;
  gameSeconds      = (difficulty == EASY) ? 120 : (difficulty == MEDIUM) ? 90 : 60;
  tickCount        = 0;

  for(int i = 0; i < MAX_INGREDIENTS; i++) ingredients[i].active = 0;

  M = TIMG12->COUNTERREGS.CTR;

  PickNewRecipe();

  int16_t floorW = FLOOR_X2 - FLOOR_X1;
  int16_t floorH = FLOOR_Y2 - FLOOR_Y1;

  for(int i = 0; i < 4; i++){
    IngType_t t = (IngType_t)Random(ING_TYPE_COUNT);
    ingredients[i].type    = t;
    ingredients[i].active  = 1;
    ingredients[i].cut     = 0;
    ingredients[i].boiled  = 0;
    ingredients[i].cooked  = 0;
    ingredients[i].canCut  = gCanCut[t];
    ingredients[i].canBoil = gCanBoil[t];
    ingredients[i].canCook = gCanCook[t];

    uint8_t placed = 0;
    ingredients[i].x = FLOOR_X1 + Random(floorW - ING_W);
    ingredients[i].y = FLOOR_Y1 + Random(floorH - ING_H);
    for(int attempt = 0; attempt < 50 && !placed; attempt++){
      int16_t tx = FLOOR_X1 + Random(floorW - ING_W);
      int16_t ty = FLOOR_Y1 + Random(floorH - ING_H);
      uint8_t ok = 1;
      for(int j = 0; j < i; j++){
        int16_t dx = tx - ingredients[j].x; if(dx<0) dx=-dx;
        int16_t dy = ty - ingredients[j].y; if(dy<0) dy=-dy;
        if(dx < 20 && dy < 20){ ok = 0; break; }
      }
      if(ok){ ingredients[i].x = tx; ingredients[i].y = ty; placed = 1; }
    }
  }
}

int main(void){
  __disable_irq();
  PLL_Init();
  LaunchPad_Init();
  ST7735_InitPrintf(INITR_REDTAB);
  Switch_Init();
  LED_Init();
  Sensor.Init();
  JoyStick_Init();
  Sound_Init();
  TExaS_Init(0, 0, &TExaS_LaunchPadLogicPB27PB26);
  TimerG12_IntArm(2666667, 2);
  __enable_irq();

  DrawHomeScreen();

  uint32_t lastSw = 0;
  Difficulty_t lastDiff = EASY;

  while(1){
    while(gameTick == 0){}
    gameTick = 0;

    uint32_t sw      = Switch_In();
    uint32_t pressed = sw & ~lastSw;
    lastSw = sw;

    switch(gameState){

      case STATE_HOME:
        if(pressed & 0x04){
          langSpanish = !langSpanish;
          DrawHomeScreen();
        }
        if(pressed & (0x08 | 0x01 | 0x02)){
          gameState  = STATE_INSTRUCTIONS;
          instrPage  = 0;
          difficulty = ReadDifficulty();
          lastDiff   = difficulty;
          DrawInstructionsPage(instrPage, difficulty);
        }
        break;

      case STATE_INSTRUCTIONS:
        if(instrPage == 0){
          difficulty = ReadDifficulty();
          if(difficulty != lastDiff){
            DrawInstructionsPage(instrPage, difficulty);
            lastDiff = difficulty;
          }
        }
        if(pressed & 0x01){
          if(instrPage < 8){
            instrPage++;
            DrawInstructionsPage(instrPage, difficulty);
          }
        }
        if(pressed & 0x04){
          if(instrPage > 0){
            instrPage--;
            DrawInstructionsPage(instrPage, difficulty);
          }
        }
        if(pressed & 0x02){
          gameState = STATE_HOME;
          DrawHomeScreen();
        }
        if(pressed & 0x08){
          Game_Init();
          gameState = STATE_PLAYING;
          ST7735_FillScreen(COLOR_BLACK);
          DrawGameBackground();
          DrawKnife();
          if(difficulty >= MEDIUM) DrawStove();
          if(difficulty >= HARD)   DrawBoiler();
          DrawPlate();
          DrawFloorIngredients();
          DrawChef(playerX, playerY);
          DrawHUD();
          DrawRecipePanel();
        }
        break;

      case STATE_PLAYING:
        {
          if(gameSeconds == 0){
            Sound_GameOver();
            gameState = STATE_GAMEOVER;
            DrawGameOverScreen();
            break;
          }

          uint32_t jx = JoyStick_InX();
          uint32_t jy = JoyStick_InY();
          int16_t nx = playerX, ny = playerY;

          if(jx < 1448)      nx += 2;
          else if(jx > 2648) nx -= 2;
          if(jy < 1448)      ny += 2;
          else if(jy > 2648) ny -= 2;

          if(nx < FLOOR_X1) nx = FLOOR_X1;
          if(nx > FLOOR_X2) nx = FLOOR_X2;
          if(ny < FLOOR_Y1) ny = FLOOR_Y1;
          if(ny > FLOOR_Y2) ny = FLOOR_Y2;

          if(nx != playerX || ny != playerY){
            EraseHeldIngredient();
            EraseChef(playerX, playerY);
            for(int i = 0; i < MAX_INGREDIENTS; i++){
              if(!ingredients[i].active || i == heldIngredient) continue;
              int16_t dx = ingredients[i].x - playerX; if(dx<0) dx=-dx;
              int16_t dy = ingredients[i].y - playerY; if(dy<0) dy=-dy;
              if(dx < CHEF_W + ING_W && dy < CHEF_H + ING_H)
                DrawIngredientSprite(ingredients[i].x, ingredients[i].y, ingredients[i]);
            }
            playerX = nx;
            playerY = ny;
            DrawChef(playerX, playerY);
            DrawHeldIngredient();
          }

          if(pressed & 0x08){
            if(heldIngredient >= 0){
              EraseHeldIngredient();
              ingredients[heldIngredient].x = playerX + CHEF_W/2 - ING_W/2;
              ingredients[heldIngredient].y = playerY + CHEF_H/2 - ING_H/2;
              ingredients[heldIngredient].active = 1;
              DrawIngredientSprite(ingredients[heldIngredient].x,
                                   ingredients[heldIngredient].y,
                                   ingredients[heldIngredient]);
              heldIngredient = -1;
            } else {
              int16_t chefCX = playerX + CHEF_W/2;
              int16_t chefCY = playerY + CHEF_H/2;
              int8_t  best   = -1;
              int16_t bestD  = PICKUP_RANGE;
              for(int i = 0; i < MAX_INGREDIENTS; i++){
                if(!ingredients[i].active) continue;
                int16_t dx = chefCX - (ingredients[i].x + ING_W/2); if(dx<0) dx=-dx;
                int16_t dy = chefCY - (ingredients[i].y + ING_H/2); if(dy<0) dy=-dy;
                if(dx + dy < bestD){ bestD = dx+dy; best = i; }
              }
              if(best >= 0){
                EraseIngredientAt(ingredients[best].x, ingredients[best].y);
                ingredients[best].active = 0;
                heldIngredient = best;
                DrawChef(playerX, playerY);
                DrawHeldIngredient();
              }
            }
          }

          if(pressed & 0x02){
            if(heldIngredient >= 0){
              EraseHeldIngredient();
              ingredients[heldIngredient].active = 0;
              heldIngredient = -1;
            }
          }

          if(pressed & 0x01){
            if(heldIngredient >= 0){
              if(NearPlate()){
                TryPlateIngredient();
              } else {
                StationAction_t action = NearStation();
                if(action != ACTION_NONE){
                  Ingredient_t& ing = ingredients[heldIngredient];
                  uint8_t allowed = (action == ACTION_CUT  && ing.canCut)  ||
                                    (action == ACTION_BOIL && ing.canBoil) ||
                                    (action == ACTION_COOK && ing.canCook);
                  if(allowed){
                    ApplyAction(action);
                    EraseHeldIngredient();
                    DrawChef(playerX, playerY);
                    DrawHeldIngredient();
                  }
                }
              }
            }
          }

          DrawHUD();
          if(doSpawn){ doSpawn = 0; SpawnIngredient(); }
        }
        break;

      case STATE_GAMEOVER:
        if(pressed & 0x0F){
          gameState = STATE_HOME;
          DrawHomeScreen();
        }
        break;
    }
  }
}