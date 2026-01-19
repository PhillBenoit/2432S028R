/*
Demos fonts with buttons
touching the screen shows the next ascii sequence
free fonts can also be tested with the commented code
as of right now, only the default font uses extended ascii
*/

#include <XPT2046_Touchscreen.h>
#include <SPI.h>
#include <TFT_eSPI.h>

#define CHARS 8
#define ROWS 4

// fills screen with buttons
#define BWIDTH (TFT_HEIGHT/CHARS)
#define BHEIGHT (TFT_WIDTH/ROWS)

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

SPIClass mySpi = SPIClass(VSPI);
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ);
TFT_eSPI tft = TFT_eSPI();

void setup() {
  Serial.begin(115200);
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);
  tft.init();
  ts.setRotation(1);
  tft.setRotation(1);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.fillScreen(TFT_BLACK);
  //tft.setFreeFont(&FreeSerif12pt7b);
  tft.drawString("touch screen to start", BWIDTH, BHEIGHT, 4);
}

char next = 0;
char buffer[2] = {0, 0};

void loop() {

  if (ts.tirqTouched() && ts.touched()) {

    // clear screen when sequense starts at 0
    if (next == 0) tft.fillScreen(TFT_BLACK);
    
    // report to serial
    Serial.printf("this screen starts at 0x%02X (%d)\n", next, next);
    
    // draws the buttons
    for (int y = 0; y < ROWS; y++) {
      for (int x = 0; x < CHARS; x++) {
        buffer[0] = next;
        TFT_eSPI_Button b;
        b.initButton(&tft,
          BWIDTH * x + BWIDTH/2,
          BHEIGHT * y + BHEIGHT/2,
          BWIDTH,
          BHEIGHT,
          TFT_BLACK,
          TFT_DARKGREEN,
          TFT_DARKGREY,
          buffer,
          2);
        b.drawButton();
        next++;
      }
    }

  }

  delay(100);
}
