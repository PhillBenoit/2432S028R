/*
  Basic clock program for CYD
  Updates time over the network
  Screen is black unless pressed 
*/

/* make sure User_Setup.h has been installed 
   https://github.com/witnessmenow/ESP32-Cheap-Yellow-Display/blob/main/DisplayConfig/User_Setup.h
   overwite default with this file for CYD
*/
#include <TFT_eSPI.h>
#include <XPT2046_Touchscreen.h>

#include <WiFi.h>
#include "time.h"
#include "esp_sntp.h"

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

// Wifi Credentials
const char *ssid = "********";
const char *password = "********";


// Time servers
const char *ntpServer1 = "pool.ntp.org";
const char *ntpServer2 = "time.nist.gov";

// West Coast US time
const char *time_zone = "PST8PDT,M3.2.0,M11.1.0";
// other time zone strings
// https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv?plain=1

// tft string display buffer
char buffer[50];

//output constants
const char* no_time = "No time available (yet)";
const char* format_wifi = "Connecting to %s ";
const char* connected = " CONNECTED";

// display object
TFT_eSPI tft = TFT_eSPI();

// input objects
SPIClass mySpi = SPIClass(VSPI); // input interface
XPT2046_Touchscreen ts(XPT2046_CS, XPT2046_IRQ); // input


// screen should be blanked 4 times per day to prevent artifacting
char ampm = '\0', // flag to blank screen at am / pm changes
  hour = '\0'; // flag to blank the screen when 12 changes to 1

// flag for last time the display was updated
time_t last_time = 0;

// loads the display string buffer
void loadBuffer(const char * pattern, const struct tm time) {
  strftime(buffer, sizeof buffer, pattern, &time);
}


// display function
void printLocalTime() {
  
  // try to get the time
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // show error messages if time is not available
    Serial.println(no_time);
    tft.setTextDatum(TL_DATUM);
    tft.drawString(no_time, 5, 10, 4);
    delay(1000);
    return;
  }
  
  // test for am / pm change
  loadBuffer("%p", timeinfo);
  if (ampm != buffer[0]) {
    last_time = 0;
    ampm = buffer[0];
    tft.fillScreen(TFT_BLACK);

    // day of the week
    loadBuffer("%A", timeinfo);
    tft.setTextDatum(TC_DATUM);
    tft.drawString(buffer, TFT_HEIGHT/2, 0, 4);
  
    // date
    loadBuffer("%B %d  %Y", timeinfo);
    tft.setTextDatum(BC_DATUM);
    tft.drawString(buffer, TFT_HEIGHT/2, TFT_WIDTH, 4);
  }

  time_t now = mktime(&timeinfo);
  if (now > last_time) {
    // parse time
    loadBuffer("%l:%M:%S %p", timeinfo);
    int len = strlen(buffer);
  
    // convert case on am / pm for font compatibility
    buffer[len-1] = tolower(buffer[len-1]);
    buffer[len-2] = tolower(buffer[len-2]);
  
    // black the screen when the hour changes from 12 to 1
    if (hour == '2' && buffer[1] == '1') {
      hour = buffer[1];
      ampm = '\0';  // flag for screen clear
      printLocalTime(); // start over to display day of the week and date
    }
    hour = buffer[1];
  
    // display time
    tft.setTextDatum(MC_DATUM);
    tft.drawString(buffer, TFT_HEIGHT/2, TFT_WIDTH/2, 6);

    last_time = now;
  }
}

// Callback function (gets called via backgroumd thread when time adjusts via NTP)
void timeavailable(struct timeval *t) {
  WiFi.disconnect(true, true);
}

void setup() {
  Serial.begin(115200);

  // Start the tft display
  tft.init();
  tft.setRotation(1); //This is the display in landscape
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(TL_DATUM);

  // Start the SPI for the touch screen and init the TS library
  mySpi.begin(XPT2046_CLK, XPT2046_MISO, XPT2046_MOSI, XPT2046_CS);
  ts.begin(mySpi);

  // tft string screen cursor
  int x = 5;
  int y = 5;

  /**
   * NTP server address could be acquired via DHCP,
   *
   * NOTE: This call should be made BEFORE esp32 acquires IP address via DHCP,
   * otherwise SNTP option 42 would be rejected by default.
   * NOTE: configTime() function call if made AFTER DHCP-client run
   * will OVERRIDE acquired NTP server address
   */
  //esp_sntp_servermode_dhcp(1);  // (optional)
  //sntp_servermode_dhcp(1);

  // connect to WiFi
  sprintf(buffer, format_wifi, ssid);
  Serial.print(buffer);
  tft.drawString(buffer, x, y, 4);
  WiFi.begin(ssid, password);

  // wait for network
  y += chr_hgt_f32; // tft string LF
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    tft.drawString(".", x, y, 4);
    x += widtbl_f32['.'];
  }
  Serial.println(connected);
  tft.drawString(connected, x, y, 4);

  // set notification call-back function
  sntp_set_time_sync_notification_cb(timeavailable);

  // get time
  configTzTime(time_zone, ntpServer1, ntpServer2);
  
  //manual time setting for debug
  //-------------------
  //struct timeval now;
  //now.tv_sec = 1771030789;
  //settimeofday(&now, NULL);
  //-------------------
}

// backlight state
bool backlight = HIGH;

// toggle screen backlight
void light() {
  backlight = !backlight;
  digitalWrite(TFT_BL, backlight);
}

void loop() {
  
  // if screen is touched
  if (ts.tirqTouched() && ts.touched()) {
    
    // toggle light if off
    if (backlight == LOW) light();
    
    // it will take some time to sync time :)
    printLocalTime();  
  }
  
  // toggle light if on
  else if (backlight == HIGH) light();
}
