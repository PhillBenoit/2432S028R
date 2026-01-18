#include <SPI.h>

#include <XPT2046_Touchscreen.h>

#include <TFT_eSPI.h>

// ----------------------------
// Touch Screen pins
// ----------------------------

// The CYD touch uses some non default
// SPI pins

#define XPT2046_IRQ 36
#define XPT2046_MOSI 32
#define XPT2046_MISO 39
#define XPT2046_CLK 25
#define XPT2046_CS 33

// ----------------------------


// button grid dimensions
#define ROWS 3
#define COLUMNS 10
#define BUTTONS (ROWS*COLUMNS)

SPIClass mySpi = SPIClass(VSPI); // input interface
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ); // input
TFT_eSPI tft = TFT_eSPI(); // output
TFT_eSPI_Button key[BUTTONS]; // button grid
int last = INT_MAX; // index of last button pressed

void setup() {
  Serial.begin(115200);

  // Start the SPI for the touch screen and init the TS library
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);

  // Start the tft display and set it to black
  tft.init();

  // set input and output rotation
  ts.setRotation(1);
  tft.setRotation(1);

  // Clear the screen before writing to it
  tft.fillScreen(TFT_BLACK);

  drawButtons();
}

/* translates touch input to display dimension x
 manual calculations based on testing screen limits
 todo: programatic calibration
 */
int xpoint(int x) {
  float a = (x-230)/3620.0; // (x-min)/(max-min)
  a *= 320;
  return (int)a;
}

/* translates touch input to display dimension y
 manual calculations based on testing screen limits
 todo: programatic calibration
 */
int ypoint(int y) {
  float a = (y-350)/3450.0; // (y-min)/(max-min)
  a *= 240;
  return (int)a;
}

// draws the buttons to the tft
void drawButtons() {
  
  // fit grid to fill the screen
  // (with rorated dimensions)
  uint16_t bWidth = TFT_HEIGHT/COLUMNS;
  uint16_t bHeight = TFT_WIDTH/ROWS;
  
  for (int i = 0; i < BUTTONS; i++) {
    key[i].initButton(&tft,
                      bWidth * (i%COLUMNS) + bWidth/2, //middle of the button x
                      bHeight * (i/COLUMNS) + bHeight/2, //middle of the button y
                      bWidth,
                      bHeight,
                      TFT_BLACK, // Outline
                      TFT_DARKGREEN, // Fill
                      TFT_DARKGREY, // Text
                      "",
                      2);

    key[i].drawButton(false, String(i+1));  // set button label and draw
  }
}

// actions to take when the button is released
void release() {
  Serial.printf("Button %d released\n", last+1); // report to serial
  key[last].drawButton(false, String(last+1)); // redraw button
  last = INT_MAX; // clear index
}

// actions to take when the button is pressed (b = button index)
void press(int b) {
  Serial.printf("Button %d pressed\n", b+1); // report to serial
  key[b].drawButton(true, String(b+1)); // invert button
  if (last != INT_MAX) release(); // release previous button
  last = b; // set index of the most revently pressed button
}

// checks array of buttons for a screen press
int findButton(int x, int y) {
  // check state of each button
  for (uint8_t b = 0; b < BUTTONS; b++) {
    if (key[b].contains(x, y)) {
      return b;
    }
  }

  // no button found
  return INT_MAX;
}

void loop() {
  
  if (ts.tirqTouched() && ts.touched()) { // check for touch input
    
    TS_Point p = ts.getPoint(); // get touch info

    // find button in the set using converted coordinates
    int button = findButton(xpoint(p.x), ypoint(p.y)); 

    // make sure button isn't already pressed and that it has a match
    if (button != last && button != INT_MAX) press(button); 

  }
  else if (last != INT_MAX) release(); // release when no other button is pressed
}