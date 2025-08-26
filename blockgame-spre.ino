//
//  name          : blockgame.ino
//  date/author   : 2025/08/09 Takeshi
//  update/author : 2024/08/10 Takeshi
//


#include "spreLGFXLib.hpp"
#include "spreTouchLib.hpp"
#include <SDHCI.h>
#include <Audio.h>

#define BOX_SIZE 20
#define GRID_X 16
#define GRID_Y 11

#define BEEPFREQ 1400
#define BEEPLEVEL -60
#define BEEPLENGTH_MS 50

const int winNotes[8] = {524, 588, 660, 698, 784, 880, 988, 1046};
const int startNotes[8] = {988, 880, 784, 698, 784, 880, 988, 1046};

unsigned long start, now, interval, milliseconds, seconds, minutes, lastTap;
bool measStarted, measEnded, initialized;
const unsigned long TAP_GUARD_TIME_MS = 250;
int currentPlayer;

const int colorTable[8] = {TFT_WHITE, TFT_YELLOW, TFT_BLUE, TFT_GREEN, TFT_RED, TFT_CYAN, TFT_MAGENTA, TFT_ORANGE};
int playerColorIdx[2] = {0, 4};
int playerColor[2];
bool placed[2][GRID_X][GRID_Y];
bool started, finished;
int winCount[2];

enum {
  BUTTON_NONE,
  BUTTON_RESET,
  BUTTON_START,
  BUTTON_P0,
  BUTTON_P1,  
};

struct AlignedInfo {
  int player;
  int xidx1;
  int yidx1;
  int xidx2;
  int yidx2;
  int xidx3;
  int yidx3;
  int xidx4;
  int yidx4;
} alignedInfo = {0, 0, 0, 0, 0, 0, 0, 0, 0};

SDClass theSD;
AudioClass *theAudio;

unsigned long probestart, probeend;

void drawBox( int32_t x, int32_t y, const int color) {
  int32_t startx = x - BOX_SIZE/2;
  int32_t starty = y - BOX_SIZE/2;
  spr.drawRect(startx, starty, BOX_SIZE, BOX_SIZE, color);
  spr.drawLine(startx, starty, startx + BOX_SIZE - 1, starty + BOX_SIZE - 1, color);
  spr.drawLine(startx, starty - 1 + BOX_SIZE, startx - 1 + BOX_SIZE, starty, color);
}

void fillBox( int32_t x, int32_t y, const int color) {
  int32_t startx = x - BOX_SIZE/2;
  int32_t starty = y - BOX_SIZE/2;
  spr.fillRect(startx, starty, BOX_SIZE, BOX_SIZE, color);
}

int32_t quantizePos(int32_t val) {
  return (int32_t)(val / BOX_SIZE) * BOX_SIZE + BOX_SIZE/2;
}

int idxToQpos(int idx) {
  return idx * BOX_SIZE + BOX_SIZE/2;
}

int seekY(int xidx) {
  int availableY = -1;
  for (int y = GRID_Y - 1; y>=0; y--) {
    if (!placed[0][xidx][y] && !placed[1][xidx][y]) {
      availableY = y;
      break;
    }
  }
  return availableY;
}
void changePlayer() {
  if (currentPlayer == 0) {
    currentPlayer = 1;
  } else if (currentPlayer == 1) {
    currentPlayer = 0;
  } else {
    Serial.printf("Unextected!!\n");
  }
}

bool isPlaced(int player, int xidx, int yidx) {
  return (xidx >= 0) && (yidx >= 0) && (xidx < GRID_X) && (yidx < GRID_Y) && placed[player][xidx][yidx];
}

void highlightAlignment(struct AlignedInfo &info) {
  fillBox(idxToQpos(info.xidx1), idxToQpos(info.yidx1), playerColor[info.player]);
  fillBox(idxToQpos(info.xidx2), idxToQpos(info.yidx2), playerColor[info.player]);
  fillBox(idxToQpos(info.xidx3), idxToQpos(info.yidx3), playerColor[info.player]);
  fillBox(idxToQpos(info.xidx4), idxToQpos(info.yidx4), playerColor[info.player]);
}
bool judgeHorizontal(int player, int xidx, int yidx) {
  // Now placed[xidx][yidx] is true
  bool aligned = false;

  if (isPlaced(player, xidx - 1, yidx)) {
    if (isPlaced(player, xidx - 2, yidx)) {
      if (isPlaced(player, xidx - 3, yidx)) {
        aligned = true;
        alignedInfo = {player, xidx, yidx, xidx - 1, yidx, xidx - 2, yidx, xidx - 3, yidx};
      } else if (isPlaced(player, xidx + 1, yidx)) {
        aligned = true;
        alignedInfo = {player, xidx, yidx, xidx - 1, yidx, xidx - 2, yidx, xidx + 3, yidx};
      }
    } else if (isPlaced(player, xidx + 1, yidx)) {
      if (isPlaced(player, xidx + 2, yidx)) {
        aligned = true;
        alignedInfo = {player, xidx, yidx, xidx - 1, yidx, xidx + 1, yidx, xidx + 2, yidx};
      }
    }
  } else if (isPlaced(player, xidx + 1, yidx)) {
    if (isPlaced(player, xidx + 2, yidx)) {
      if (isPlaced(player, xidx + 3, yidx)) {
        aligned = true;
        alignedInfo = {player, xidx, yidx, xidx + 1, yidx, xidx + 2, yidx, xidx + 3, yidx};
      }
    }
  }
  if (aligned) {
    Serial.printf("Horizontal aligned!\n");
  }
  return aligned;
}

bool judgeVertical(int player, int xidx, int yidx) {
  // Now placed[xidx][yidx] is true
  bool aligned = false;

  if (isPlaced(player, xidx, yidx + 1)) {
    if (isPlaced(player, xidx, yidx + 2)) {
      if (isPlaced(player, xidx, yidx + 3)) {
        aligned = true;
        alignedInfo = {player, xidx, yidx, xidx, yidx + 1, xidx, yidx + 2, xidx, yidx + 3};
      }
    }
  }
  if (aligned) {
    Serial.printf("Vertical aligned!\n");
  }
  return aligned;
}

bool judgeDiagonal(int player, int xidx, int yidx) {
  // Now placed[xidx][yidx] is true
  bool aligned = false;

  if (isPlaced(player, xidx - 1, yidx - 1)) {
    if (isPlaced(player, xidx - 2, yidx - 2)) {
      if (isPlaced(player, xidx - 3, yidx - 3)) {
        aligned = true;
        alignedInfo = {player, xidx, yidx, xidx - 1, yidx - 1, xidx - 2, yidx - 2, xidx - 3, yidx - 3};
      } else if (isPlaced(player, xidx + 1, yidx + 1)) {
        aligned = true;
        alignedInfo = {player, xidx, yidx, xidx - 1, yidx - 1, xidx - 2, yidx - 2, xidx + 1, yidx + 1};
      }
    } else if (isPlaced(player, xidx + 1, yidx + 1)) {
      if (isPlaced(player, xidx + 2, yidx + 2)) {
        aligned = true;
        alignedInfo = {player, xidx, yidx, xidx - 1, yidx - 1, xidx + 1, yidx + 1, xidx + 2, yidx + 2};
      }
    }
  } else if (isPlaced(player, xidx + 1, yidx + 1)) {
    if (isPlaced(player, xidx + 2, yidx + 2)) {
      if (isPlaced(player, xidx + 3, yidx + 3)) {
        aligned = true;
        alignedInfo = {player, xidx, yidx, xidx + 1, yidx + 1, xidx + 2, yidx + 2, xidx + 3, yidx + 3};
      }
    }
  }

  if (isPlaced(player, xidx - 1, yidx + 1)) {
    if (isPlaced(player, xidx - 2, yidx + 2)) {
      if (isPlaced(player, xidx - 3, yidx + 3)) {
        aligned = true;
        alignedInfo = {player, xidx, yidx, xidx - 1, yidx + 1, xidx - 2, yidx + 2, xidx - 3, yidx + 3};
      } else if (isPlaced(player, xidx + 1, yidx - 1)) {
        aligned = true;
        alignedInfo = {player, xidx, yidx, xidx - 1, yidx + 1, xidx - 2, yidx + 2, xidx + 1, yidx - 1};
      }
    } else if (isPlaced(player, xidx + 1, yidx - 1)) {
      if (isPlaced(player, xidx + 2, yidx - 2)) {
        aligned = true;
        alignedInfo = {player, xidx, yidx, xidx - 1, yidx + 1, xidx + 1, yidx - 1, xidx + 2, yidx - 2};
      }
    }
  } else if (isPlaced(player, xidx + 1, yidx - 1)) {
    if (isPlaced(player, xidx + 2, yidx - 2)) {
      if (isPlaced(player, xidx + 3, yidx - 3)) {
        aligned = true;
        alignedInfo = {player, xidx, yidx, xidx + 1, yidx - 1, xidx + 2, yidx - 2, xidx + 3, yidx - 3};
      }
    }
  }

  if (aligned) {
    Serial.printf("Diagonal aligned!\n");
  }
  return aligned;
}

void updateBottomInfo(int highlightedPlayer = -1) {
  spr.setTextSize(1);
  spr.fillRect(0, 221, 320, 19, TFT_BLACK);
  spr.drawRoundRect( 2, 222, 36, 16, 2, TFT_WHITE);
  spr.drawRoundRect(42, 222, 36, 16, 2, TFT_WHITE);
  spr.setCursor( 5,227);spr.setTextColor(TFT_WHITE);spr.printf("Reset");
  spr.setCursor(45,227);spr.setTextColor(TFT_WHITE);spr.printf("Start");
  spr.setCursor(120,227);spr.setTextColor(TFT_WHITE);spr.printf("WINs->");
  spr.setCursor(160,227);spr.setTextColor(playerColor[0]);spr.printf(" Player1:%3d", winCount[0]);
  spr.setCursor(240,227);spr.setTextColor(playerColor[1]);spr.printf(" Player2:%3d", winCount[1]);
  if (highlightedPlayer == 0) {
    spr.fillRect(160, 221, 80, 19, playerColor[0]);spr.setCursor(160,227);spr.setTextColor(TFT_BLACK);spr.printf(" Player1:%3d", winCount[0]);
  } else if (highlightedPlayer == 1) {
    spr.fillRect(240, 221, 80, 19, playerColor[1]);spr.setCursor(240,227);spr.setTextColor(TFT_BLACK);spr.printf(" Player2:%3d", winCount[1]);
  }
}

void gameInit() {
  currentPlayer = 0;
  for (int p=0; p<2; p++) {
    for (int i=0; i<GRID_X; i++) {
      for (int j=0; j<GRID_Y; j++) {
        placed[p][i][j] = false;
      }
    }
  }
  playerColor[0] = colorTable[playerColorIdx[0]];
  playerColor[1] = colorTable[playerColorIdx[1]];
  alignedInfo = {0, 0, 0, 0, 0, 0, 0, 0, 0};
  spr.fillSprite(TFT_BLACK);
  spr.drawLine(0, 220, 320, 220, TFT_DARKGRAY);
  updateBottomInfo();
}

void placeSound() {
  theAudio->setBeep(1, BEEPLEVEL, BEEPFREQ);
  usleep(BEEPLENGTH_MS * 1000);
  theAudio->setBeep(1, BEEPLEVEL, (int)BEEPFREQ/2);
  usleep(BEEPLENGTH_MS * 1000);
  theAudio->setBeep(0, 0, 0);
}

void startSound() {
  for (int i=0; i< 8; i++) {
    theAudio->setBeep(1, BEEPLEVEL, startNotes[i]);
    usleep(BEEPLENGTH_MS * 1000);
  }
  theAudio->setBeep(0, 0, 0);
}

void winSound() {
  for (int i=0; i< 8; i++) {
    theAudio->setBeep(1, BEEPLEVEL, winNotes[i]);
    usleep(BEEPLENGTH_MS * 1000);
  }
  theAudio->setBeep(0, 0, 0);
}
//===================================
// setup
//===================================
void setup(void) {
  Serial.begin(115200);
  setupLGFX(DEPTH_16BIT, ROT90);
  setupTouch(_w, _h, ROT90, false);
  int rtn = 0;
  uint32_t sz = _w * _h;
  
  theAudio = AudioClass::getInstance();
  theAudio->begin();
  puts("initialization Audio Library");
  theAudio->setPlayerMode(AS_SETPLAYER_OUTPUTDEVICE_SPHP, 0, 0);

  spr.setColorDepth(DEPTH_8BIT);
  if (!spr.createSprite(_w, _h)) {
    Serial.printf("ERROR: malloc error (tmpspr:%dByte)\n", sz);
    rtn = -1;
  }
  winCount[0] = 0;
  winCount[1] = 0;
  gameInit();
  finished = true;
}

int getTouchedInfoArea(int xidx, int yidx) {
  if (xidx >= 0 && xidx <= 1 && yidx == 11) {
    return BUTTON_RESET;
  }
  if (xidx >= 2 && xidx <= 3 && yidx == 11) {
    return BUTTON_START;
  }
  if (xidx >= 8 && xidx <= 11 && yidx == 11) {
    return BUTTON_P0;
  }
  if (xidx >= 12 && xidx <= 15 && yidx == 11) {
    return BUTTON_P1;
  }
  return BUTTON_NONE;
}

void changePlayerColor(int player) {
  playerColorIdx[player] = (playerColorIdx[player] + 1) % 8;
  playerColor[player] = colorTable[playerColorIdx[player]];
  updateBottomInfo();
}

void allReset() {
  winCount[0] = 0;
  winCount[1] = 0;
  updateBottomInfo();
  gameInit();
}

//===================================
// loop
//===================================
void loop(void) {

  int tx, ty, tz, qx, qy, xidx, yidx;
  if (isTouch(&tx, &ty, &tz)) {
    now = millis();
    if (now - lastTap > TAP_GUARD_TIME_MS) {
      if (finished) {
        qx = quantizePos(tx);
        xidx = qx / BOX_SIZE;
        qy = quantizePos(ty);
        yidx = qy / BOX_SIZE;
        int button = getTouchedInfoArea(xidx, yidx);
        Serial.printf("button =%d, xidx=%d, yidx=%d \n", button, xidx, yidx);
        if (button == BUTTON_RESET) {
          allReset();
        }
        if (button == BUTTON_P0) {
          changePlayerColor(0);
        }
        if (button == BUTTON_P1) {
          changePlayerColor(1);
        }
        if (button == BUTTON_START) {
          gameInit();
          startSound();
          finished = false;
        }
      } else {
        qx = quantizePos(tx);
        xidx = min(qx / BOX_SIZE, GRID_X - 1);
        yidx = seekY(xidx);
        if (yidx == -1) {
          Serial.printf("Cannot place in this colmun! Skipped\n");
        } else {
          qy = idxToQpos(yidx);
          drawBox(qx, qy, playerColor[currentPlayer]);
          placed[currentPlayer][xidx][yidx] = true;
          placeSound();
          //Serial.printf("Placed. Xidx=%d, Yidx=%d\n", xidx, yidx);
          if (judgeHorizontal(currentPlayer, xidx, yidx) || judgeVertical(currentPlayer, xidx, yidx) || judgeDiagonal(currentPlayer, xidx, yidx)) {
            winCount[currentPlayer]++;
            Serial.printf("Player %d win! count = %d\n", currentPlayer + 1, winCount[currentPlayer]);
            updateBottomInfo(currentPlayer);
            highlightAlignment(alignedInfo);
            spr.pushSprite(&lcd, 0, 0);
            winSound();
            finished = true;
          }
          changePlayer();
        }
      }
    }
    lastTap = millis();
  }
  spr.pushSprite(&lcd, 0, 0);
}