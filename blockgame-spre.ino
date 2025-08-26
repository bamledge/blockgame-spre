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

//const int notes[8] = {262, 294, 330, 349, 392, 440, 494, 523};
const int notes[8] = {524, 588, 660, 698, 784, 880, 988, 1046};

unsigned long start, now, interval, milliseconds, seconds, minutes, lastTap;
bool measStarted, measEnded, initialized;
const unsigned long TAP_GUARD_TIME_MS = 250;
int currentPlayer;
int playerColor[2] = {TFT_WHITE, TFT_RED};
bool placed[2][GRID_X][GRID_Y];
bool finished;

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

void highlightAlignment(int xidx1, int yidx1, int xidx2, int yidx2, int xidx3, int yidx3, int xidx4, int yidx4, int player) {
  fillBox(idxToQpos(xidx1), idxToQpos(yidx1), playerColor[player]);
  fillBox(idxToQpos(xidx2), idxToQpos(yidx2), playerColor[player]);
  fillBox(idxToQpos(xidx3), idxToQpos(yidx3), playerColor[player]);
  fillBox(idxToQpos(xidx4), idxToQpos(yidx4), playerColor[player]);
}

bool judgeHorizontal(int player, int xidx, int yidx) {
  // Now placed[xidx][yidx] is true
  bool aligned = false;

  if ((xidx - 1 >= 0) && placed[player][xidx - 1][yidx]) {
    if ((xidx - 2 >= 0) && placed[player][xidx - 2][yidx]) {
      if ((xidx - 3 >= 0) && placed[player][xidx - 3][yidx]) {
        aligned = true;
        highlightAlignment(xidx, yidx, xidx - 1, yidx, xidx - 2, yidx, xidx - 3, yidx, player);
      } else if ((xidx + 1 < GRID_X) && placed[player][xidx + 1][yidx]) {
        aligned = true;
        highlightAlignment(xidx, yidx, xidx - 1, yidx, xidx - 2, yidx, xidx + 3, yidx, player);
      }
    } else if ((xidx + 1 < GRID_X) && placed[player][xidx + 1][yidx]) {
      if ((xidx + 2 < GRID_X) && placed[player][xidx + 2][yidx]) {
        aligned = true;
        highlightAlignment(xidx, yidx, xidx - 1, yidx, xidx + 1, yidx, xidx + 2, yidx, player);
      }
    }
  } else if ((xidx + 1 < GRID_X) && placed[player][xidx + 1][yidx]) {
    if ((xidx + 2 < GRID_X) && placed[player][xidx + 2][yidx]) {
      if ((xidx + 3 < GRID_X) && placed[player][xidx + 3][yidx]) {
        aligned = true;
        highlightAlignment(xidx, yidx, xidx + 1, yidx, xidx + 2, yidx, xidx + 3, yidx, player);
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

  if ((yidx + 1 < GRID_Y) && placed[player][xidx][yidx + 1]) {
    if ((yidx + 2 < GRID_Y) && placed[player][xidx][yidx + 2]) {
      if ((yidx + 3 < GRID_Y) && placed[player][xidx][yidx + 3]) {
        aligned = true;
        highlightAlignment(xidx, yidx, xidx, yidx + 1, xidx, yidx + 2, xidx, yidx + 3, player);
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

  if ((xidx - 1 >= 0) && (yidx - 1 >= 0) && placed[player][xidx - 1][yidx - 1]) {
    if ((xidx - 2 >= 0) && (yidx - 2 >= 0) && placed[player][xidx - 2][yidx - 2]) {
      if ((xidx - 3 >= 0) && (yidx - 3 >= 0) && placed[player][xidx - 3][yidx - 3]) {
        aligned = true;
        highlightAlignment(xidx, yidx, xidx - 1, yidx - 1, xidx - 2, yidx - 2, xidx - 3, yidx - 3, player);
      } else if ((xidx + 1 < GRID_X) && (yidx + 1 < GRID_Y) && placed[player][xidx + 1][yidx + 1]) {
        aligned = true;
        highlightAlignment(xidx, yidx, xidx - 1, yidx - 1, xidx - 2, yidx - 2, xidx + 1, yidx + 1, player);
      }
    } else if ((xidx + 1 < GRID_X) && (yidx + 1 < GRID_Y)  && placed[player][xidx + 1][yidx + 1]) {
      if ((xidx + 2 < GRID_X) && (yidx + 2 < GRID_Y) && placed[player][xidx + 2][yidx + 2]) {
        aligned = true;
        highlightAlignment(xidx, yidx, xidx - 1, yidx - 1, xidx + 1, yidx + 1, xidx + 2, yidx + 2, player);
      }
    }
  } else if ((xidx + 1 < GRID_X) && (yidx + 1 < GRID_Y) && placed[player][xidx + 1][yidx + 1]) {
    if ((xidx + 2 < GRID_X) && (yidx + 2 < GRID_Y)  && placed[player][xidx + 2][yidx + 2]) {
      if ((xidx + 3 < GRID_X) && (yidx + 3 < GRID_Y)  && placed[player][xidx + 3][yidx + 3]) {
        aligned = true;
        highlightAlignment(xidx, yidx, xidx + 1, yidx + 1, xidx + 2, yidx + 2, xidx + 3, yidx + 3, player);
      }
    }
  }

  if ((xidx - 1 >= 0) && (yidx + 1 < GRID_Y) && placed[player][xidx - 1][yidx + 1]) {
    if ((xidx - 2 >= 0) && (yidx + 2 < GRID_Y) && placed[player][xidx - 2][yidx + 2]) {
      if ((xidx - 3 >= 0) && (yidx + 3 < GRID_Y) && placed[player][xidx - 3][yidx] + 3) {
        aligned = true;
        highlightAlignment(xidx, yidx, xidx - 1, yidx + 1, xidx - 2, yidx + 2, xidx - 3, yidx + 3, player);
      } else if ((xidx + 1 < GRID_X) &&  (yidx - 1 >= 0) && placed[player][xidx + 1][yidx - 1]) {
        aligned = true;
        highlightAlignment(xidx, yidx, xidx - 1, yidx + 1, xidx - 2, yidx + 2, xidx + 1, yidx - 1, player);
      }
    } else if ((xidx + 1 < GRID_X) && (yidx - 1 >= 0)  && placed[player][xidx + 1][yidx - 1]) {
      if ((xidx + 2 < GRID_X) && (yidx - 2 >= 0) && placed[player][xidx + 2][yidx - 2]) {
        aligned = true;
        highlightAlignment(xidx, yidx, xidx - 1, yidx + 1, xidx + 1, yidx - 1, xidx + 2, yidx - 2, player);
      }
    }
  } else if ((xidx + 1 < GRID_X) && (yidx - 1 >= 0) && placed[player][xidx + 1][yidx - 1]) {
    if ((xidx + 2 < GRID_X) && (yidx - 2 >= 0)  && placed[player][xidx + 2][yidx - 2]) {
      if ((xidx + 3 < GRID_X) && (yidx - 3 >= 0)  && placed[player][xidx + 3][yidx - 3]) {
        aligned = true;
        highlightAlignment(xidx, yidx, xidx + 1, yidx - 1, xidx + 2, yidx - 2, xidx + 3, yidx - 3, player);
      }
    }
  }

  if (aligned) {
    Serial.printf("Diagonal aligned!\n");
  }
  return aligned;
}

void gameInit() {
  currentPlayer = 0;
  finished = false;
  for (int p=0; p<2; p++) {
    for (int i=0; i<GRID_X; i++) {
      for (int j=0; j<GRID_Y; j++) {
        placed[p][i][j] = false;
      }
    }
  }
  spr.fillSprite(TFT_BLACK);
  spr.drawLine(0, 220, 320, 220, TFT_DARKGRAY);
}

void placeSound() {
  theAudio->setBeep(1, BEEPLEVEL, BEEPFREQ);
  usleep(BEEPLENGTH_MS * 1000);
  theAudio->setBeep(1, BEEPLEVEL, (int)BEEPFREQ/2);
  usleep(BEEPLENGTH_MS * 1000);
  theAudio->setBeep(0, 0, 0);
}

void winSound() {
  for (int i=0; i< 8; i++) {
    theAudio->setBeep(1, BEEPLEVEL, notes[i]);
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
        gameInit();
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
            Serial.printf("Player %d win!\n", currentPlayer + 1);
            finished = true;
          }
          if(finished){
            winSound();
          }
          changePlayer();
        }
      }
    }
    lastTap = millis();
  }
  spr.pushSprite(&lcd, 0, 0);
}