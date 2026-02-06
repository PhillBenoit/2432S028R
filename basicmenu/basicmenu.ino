/* basic keyboard interface for CYD */

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
#define ROWS 6
// multiple columns are dedicated to options
// single for each navigation button
#define COLUMNS 10
// options and 2 navigation buttons
#define BUTTONS (ROWS+2)
// max number of characters to show in each option
#define MAX_DISP_CHARS 20

// fit grid to fill the screen
// (with rorated dimensions)
#define BWIDTH (TFT_HEIGHT/COLUMNS)
#define BHEIGHT (TFT_WIDTH/ROWS)

// button offsets
#define BHALFW (BWIDTH/2)
#define BHALFH (BHEIGHT/2)

// ----------------------------

// labels for navigation buttons
// ncurses font
char back_label[2] = {174, 0};
char forward_label[2] = {175, 0};

// ----------------------------

SPIClass mySpi = SPIClass(VSPI); // input interface
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ); // input
TFT_eSPI tft = TFT_eSPI(); // output

TFT_eSPI_Button uibuttons[BUTTONS]; // button grid
bool button_enabled[BUTTONS]; // flag for UI interactions

char **options; // options
int options_count; // number of options
int *opt_lengths; // length of all the options strings
int last = INT_MAX; // most recently pressed button
int menu_index; // first option to display in the menu
int row_offsets[ROWS]; // map to button for drawstring
int row_scroll[ROWS]; // index for scrolling through the text of each option
int row_lengths[ROWS]; // string length for each row
unsigned long nextframe; // timer to keep track of the scrolling

// ----------------------------


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

// checks array of buttons for a screen press
// returns INT_MAX if no buttons found
int findButton(int x, int y) {
  // check state of each button
  for (uint8_t b = 0; b < BUTTONS; b++) {
    if (uibuttons[b].contains(x, y)) {
      return b;
    }
  }
  return INT_MAX;
}

// init menu user interface
void initMNU(char **opt, int opt_count) {

  // Clear the screen before writing to it
  tft.fillScreen(TFT_BLACK);

  // save the options to memory
  options_count = opt_count;
  options = (char**)malloc(options_count * sizeof(char*));
  opt_lengths = (int*)malloc(options_count * sizeof(int));
  for (int x = 0; x < options_count; x++) {
    opt_lengths[x] = strlen(opt[x]);
    options[x] = (char*)malloc(opt_lengths[x]+1 * sizeof(char));
    strcpy(options[x], opt[x]);
  }
  menu_index = 0;

  // forward and back buttons
  uibuttons[0].initButton(&tft, BHALFW, TFT_WIDTH/2, BWIDTH, TFT_WIDTH,
    TFT_BLACK, TFT_DARKGREEN, TFT_DARKGREY, back_label, 2);
  uibuttons[BUTTONS-1].initButton(&tft, ((COLUMNS-1)*BWIDTH)+BHALFW, TFT_WIDTH/2, BWIDTH, TFT_WIDTH,
    TFT_BLACK, TFT_DARKGREEN, TFT_DARKGREY, forward_label, 2);
  
  // options
  for (int x = 0; x < ROWS; x++) {
    row_offsets[x] = (x*BHEIGHT)+BHALFH;
    uibuttons[x+1].initButton(&tft, TFT_HEIGHT/2, row_offsets[x], BWIDTH*(COLUMNS-2), BHEIGHT,
      TFT_BLACK, TFT_DARKGREEN, TFT_DARKGREY, "", 2);
  }
  
  // center text in buttons
  tft.setTextDatum(MC_DATUM);
}

// draws text for the options
void drawText(int row, char* label, bool invert) {
  
  // final display buffer
  char buffer[MAX_DISP_CHARS+1];
  buffer[MAX_DISP_CHARS] = '\0';

  // set text color
  if (invert) tft.setTextColor(TFT_DARKGREEN, TFT_DARKGREY);
  else tft.setTextColor(TFT_DARKGREY, TFT_DARKGREEN);
  
  // fill buffer using scroll offset
  strncpy(buffer, label+row_scroll[row], MAX_DISP_CHARS);
  
  // display
  tft.drawString(buffer, TFT_HEIGHT/2, row_offsets[row]);
}

// draws the UI buttons
void drawButton(int b, char* label) {
  bool invert = b == last || !button_enabled[b];
  uibuttons[b].drawButton(invert);
  if (b == 0 || b == BUTTONS-1) return;
  drawText(b-1, label, invert);
}

// draws the whole UI
// called while navigating through the list of options
void drawMNU() {
  
  // disable back if at the beginning of the list
  button_enabled[0] = menu_index > 0;
  
  // disable forward navigation when at the end of the list
  int options_remaining = options_count - menu_index;
  button_enabled[BUTTONS-1] = options_remaining > ROWS;
  
  // draw the navigation buttons
  drawButton(0, back_label);
  drawButton(BUTTONS-1, forward_label);
  
  // set up the options
  for (int x = 0; x < ROWS; x++) {
    row_scroll[x] = 0;
    
    // make sure enough options exist to fill the screen
    button_enabled[x+1] = x < options_remaining;
    
    // default behavior
    if (button_enabled[x+1]) {
      drawButton(x+1, options[menu_index+x]);
      row_lengths[x] = opt_lengths[menu_index+x];
    }

    // disable buttons with no options at the end of navigation scrolling
    else {
      drawButton(x+1, &forward_label[1]);
      row_lengths[x] = 0;
    }
  }
}


void setup() {
  Serial.begin(115200);

  // Start the SPI for the touch screen and init the TS library
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);

  // Start the tft display
  tft.init();

  // set input and output rotation
  ts.setRotation(1);
  tft.setRotation(1);

  // hard coded options for demonstration
  char *opt1 = "1234567890";
  char *opt2 = "1234567890abcdefghij";
  char *opt3 = "1234567890abcdefghijABCDEF";
  char *opt4 = "1234567890abcdefghijABCDEFGHIJ";
  char *opt5 = "1234567890a";
  char *opt6 = "1234567890abcdefghijABCD";
  char *opt7 = "1234567890abcdefghijABCDEFGHI";
  char *opt8 = "1234567890abcdefghijABCDEFGHIJKL";
  char *opts[] = {opt1, opt2, opt3, opt4, opt5, opt6, opt7, opt8};

  initMNU(opts, 8);
  drawMNU();
  
  // pause for 3 seconds on start
  nextframe = millis() + 3000;
}

// actions to take when a button is released
void release() {
  int tmp = last;
  last = INT_MAX;
  Serial.printf("Button (%d) released\n", tmp); // report to serial
  if (tmp == 0 || tmp == BUTTONS - 1) drawMNU(); // redo whole menu for navigation
  else drawButton(tmp, options[menu_index+tmp-1]); // redraw button for single toggle
}

// actions to take when a button is pressed (b = button index)
void press(int b) {

  if (last != INT_MAX) release(); // release previous button
  last = b; // set index of the most revently pressed button

  if (!button_enabled[b]) return; // reject diabled buttons

  char* label;
  
  // navagate back through options
  if (b == 0) {
    menu_index -= ROWS;
    label = back_label;
  }
  
  // navigate forward through options
  else if (b == (BUTTONS-1)) {
    menu_index += ROWS;
    label = forward_label;
  }
  
  // select option
  else label = options[menu_index+b-1];
  
  // report to serial
  Serial.printf("Button (%d) pressed\n", b);
  Serial.printf("%s\n", label);
  
  // invert button in UI
  drawButton(b, label);
}

// advance the text scrolling for the displayed options
void advanceScroll() {
  for (int x = 0; x < ROWS; x++) {
    // don't scroll disabled buttons
    if (!button_enabled[x+1]) return;
    
    // determine the next scroll position
    bool advance = row_lengths[x] - row_scroll[x] > MAX_DISP_CHARS;
    row_scroll[x] = advance ? row_scroll[x]+1 : 0;
    
    // draw the text
    drawText(x, options[menu_index+x], x+1 == last);
  }
}

void loop() {
  if (ts.tirqTouched() && ts.touched()) { // check for touch input
    TS_Point p = ts.getPoint(); // get touch info

    // find button in the set using converted coordinates
    int button = findButton(xpoint(p.x), ypoint(p.y)); 
    
    if (button != last && // make sure button isn't already pressed and
      button != INT_MAX) // that it has a match
      press(button); 
  }
  else if (last != INT_MAX) release(); // release when no button is pressed
  
  // process scrolling
  unsigned long now = millis();
  if (now > nextframe) {
    advanceScroll();
    nextframe = now + 1000;
  }
}