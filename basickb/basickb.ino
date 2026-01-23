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
#define ROWS 3
#define COLUMNS 10
#define BUTTONS (ROWS*COLUMNS)

// number of buttons that do not change
#define CONST_BUTTONS 4

// number of dynamic buttons
#define DYN_BUTTONS (BUTTONS-CONST_BUTTONS)

// number of character input pads
#define PADS 4

// shift button index
#define SHIFT_INDEX 20

// fit grid to fill the screen with space for scratchpad
// (with rorated dimensions)
#define BWIDTH (TFT_HEIGHT/COLUMNS)
#define BHEIGHT (TFT_WIDTH/(ROWS+1))

// button offsets
#define BHALFW (BWIDTH/2)
#define BHALFH (BHEIGHT/2)
#define BHOFFSET (BHALFH+BHEIGHT)

// dimensions to hold the buffer for the return string
#define RETURN_BUFFER_SIZE 27
#define RETURN_STRING_MAX (RETURN_BUFFER_SIZE-1)

// ----------------------------

SPIClass mySpi = SPIClass(VSPI); // input interface
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ); // input
TFT_eSPI tft = TFT_eSPI(); // output
TFT_eSPI_Button key[BUTTONS]; // button grid
TFT_eSPI_Button scratchpad; // white space for return buffer

// ----------------------------


// return buffer variables
char return_buffer[RETURN_BUFFER_SIZE];
uint8_t return_buffer_index = 0;

// list of constant button indicies
// MUST BE IN ASCENDING ORDER
const uint8_t const_buttons[CONST_BUTTONS] = {19,20,28,29};

// labels for buttons that do not change
// MUST BE IN SAME ORDER AS THE INDEX LIST
// using ncurses character set
const char pad_const_lbl[CONST_BUTTONS] = {20,' ',174,175};

// index of last button pressed
int last = INT_MAX;

// ----------------------------
// character input pads
// buttons marked as \0 will be inverted and be disabled in the UI

const char pad_lower[DYN_BUTTONS] = {'q','w','e','r','t','y','u','i','o','p',
  'a','s','d','f','g','h','j','k','l',
  'z','x','c','v','b','n','m'
};

const char pad_upper[DYN_BUTTONS] = {'Q','W','E','R','T','Y','U','I','O','P',
  'A','S','D','F','G','H','J','K','L',
  'Z','X','C','V','B','N','M'
};

const char pad_numeric[DYN_BUTTONS] = {'1','2','3','4','5','6','7','8','9','0',
  '`','-','=','\[','\]','\\',';','\'','\0',
  ',','.','/','\0','\0','\0','\0'
};

const char pad_symbols[DYN_BUTTONS] = {'!','@','#','$','%','^','&','*','(',')',
  '~','_','+','{','}','|',':','"','\0',
  '<','>','?','\0','\0','\0','\0'
};

// labels to apply to shift
const String pad_labels[PADS] = {"abc","ABC","123","!@#"};

// array of pads, order must be same as labels
const char* pads[PADS] = { pad_lower, pad_upper, pad_numeric, pad_symbols };

// current pad index
uint8_t pad_index = 0;

// quick access to characters on the current pad
char pad_current[BUTTONS];

// returns the next pad index
uint8_t nextPad() {
  uint8_t next = pad_index;
  next++;
  next %= PADS;
  return next;
}

// ----------------------------
// constant button actions

void btn_return() {
  Serial.println(String(return_buffer)); // prints the buffer
  return_buffer_index = 0; // resets the buffer
  scratchpad.drawButton(); // clears the scratchpad
}

void btn_shift() {
  pad_index = nextPad();
  drawPad();
}

void btn_back() {
  if (return_buffer_index > 0) { // make sure something is in the buffer
    return_buffer_index--; // shrink the used buffer
    scratchpad.drawButton(); // clear the scratchpad
  }
}

void btn_space() {
  if (return_buffer_index < RETURN_STRING_MAX) { // make sure there is space to add
    return_buffer[return_buffer_index++] = ' ';
  }
}

// ----------------------------

// array of constant button actions for dynamic calls
void (*constant_actions[CONST_BUTTONS])() = {btn_return, btn_shift, btn_back, btn_space};

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
    if (key[b].contains(x, y)) {
      return b;
    }
  }
  return INT_MAX;
}

// init keyboard buttons
void initKB() {

  // Clear the screen before writing to it
  tft.fillScreen(TFT_BLACK);

  // init the white space far the scratchpad
  scratchpad.initButton(&tft, TFT_HEIGHT/2, BHALFH, TFT_HEIGHT, BHEIGHT,
    TFT_BLACK, TFT_WHITE, TFT_BLACK, "", 2);
  scratchpad.drawButton();
  tft.setTextDatum(ML_DATUM);
  
  // display limits demo
  //tft.drawString("12345678901",5,BHALFH,4);
  //tft.drawString("1234567890123456789",5,BHALFH,2);
  //tft.drawString("12345678901234567890123456",5,BHALFH,1);
  
  // kb buttons
  for (uint8_t i = 0; i < BUTTONS; i++) {
    key[i].initButton(&tft,
                      BWIDTH * (i%COLUMNS) + BHALFW, //middle of the button x
                      //middle of the button y, also leaves space for the scratchpad
                      BHEIGHT * (i/COLUMNS) + BHOFFSET,
                      BWIDTH,
                      BHEIGHT,
                      TFT_BLACK, // Outline
                      TFT_DARKGREEN, // Fill
                      TFT_DARKGREY, // Text
                      "",
                      2);
  }
  
  // special init for shift
  key[SHIFT_INDEX].initButton(&tft,
                      BWIDTH * (SHIFT_INDEX%COLUMNS) + BHALFW,
                      BHEIGHT * (SHIFT_INDEX/COLUMNS) + BHOFFSET,
                      BWIDTH,
                      BHEIGHT,
                      TFT_BLACK,
                      TFT_DARKGREEN,
                      TFT_DARKGREY,
                      "",
                      1); // smallest font
}

// draws current pad index
void drawPad() {
  uint8_t skip_index = 0;
  uint8_t next_skip = const_buttons[skip_index];
  uint8_t dyn_index = 0;
  char label[4] = {0, 0, 0, 0};

  for (uint8_t i = 0; i < BUTTONS; i++) {

    if (i == next_skip) { // finds constant buttons
      label[0] = pad_const_lbl[skip_index++];
      // prevent future checks from being true once all buttons have been found
      next_skip = skip_index == CONST_BUTTONS ? CHAR_MAX : const_buttons[skip_index];
    }
    else {
      label[0] = pads[pad_index][dyn_index++];
    }
    
    pad_current[i] = label[0]; // backup pad info

    key[i].drawButton(pad_current[i] == '\0', label); // draw inverted if blank
  }
  
  // labels shift button
  pad_labels[nextPad()].toCharArray(label, 4);
  key[SHIFT_INDEX].drawButton(last == SHIFT_INDEX, label);
}

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

  initKB();
  drawPad();
}

// actions to take when a button is released
void release() {
  
  // load label from current pad info
  char label[4] = {0, 0, 0, 0};
  if (last == SHIFT_INDEX) pad_labels[nextPad()].toCharArray(label, 4);
  else label[0] = pad_current[last];
  
  Serial.printf("Button (%s) released\n", label); // report to serial
  key[last].drawButton(pad_current[last] == '\0', label); // redraw button
  last = INT_MAX; // clear index
}

// handles constant button actions
// returns true if button was handled
bool const_handler(int b) {
  for (int x = 0; x < CONST_BUTTONS; x++) { // loop through constant buttons
    if (b == const_buttons[x]) { // match
      (*constant_actions[x])(); // take action
      return true;
    }
  }
  return false;
}

// actions to take when a button is pressed (b = button index)
void press(int b) {

  if (last != INT_MAX) release(); // release previous button
  last = b; // set index of the most revently pressed button
  if (pad_current[b] == '\0') return; // reject disabled buttons

  // load label from pad info
  char label[4] = {0, 0, 0, 0};
  if (b == SHIFT_INDEX) pad_labels[nextPad()].toCharArray(label, 4);
  else label[0] = pad_current[b];
  
  Serial.printf("Button (%s) pressed\n", label); // report to serial
  key[b].drawButton(true, label); // invert button
  if (!const_handler(b)) { // attempt constant handler
    // make sure there is space in the string
    if (return_buffer_index < RETURN_STRING_MAX) {
      // set character and increment cursor
      return_buffer[return_buffer_index++] = pad_current[b];
    }
  }
  return_buffer[return_buffer_index] = '\0'; // terminate string
  tft.setTextColor(TFT_BLACK, TFT_WHITE);
  tft.drawString(String(return_buffer),5,BHALFH,1); // draw to scratchpad
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
}