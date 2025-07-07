/******************************************************************
 *  6-digit WS2811 24-hour clock — Robert Silvers, July 2025
 *
 *  • Wi-Fi via captive portal  (SSID CLOCK_xxxxxx)
 *  • Hourly NTP sync
 *  • Push-button (pin 27) cycles 36 colour modes (saved to EEPROM)
 *        0-6   = solid colours   (Red → White)
 *        7-31  = patterned modes (C/R/C … Y/R/Y)
 *        32    = Rainbow spectrum per digit
 *        33    = Auto-1 s   (cycle 0-32 once per second)
 *        34    = Auto-1 m   (cycle 0-32 once per minute)
 *        35    = Auto-1 h   (cycle 0-32 once per hour)
 *  • Second button (pin 14) cycles 5 brightness levels, incl. OFF
 *  • Centre LED shows Wi-Fi status (purple → yellow → white / red / blue)
 *  • Displays real HH:MM:SS
 ******************************************************************/

// ───── libraries ─────
#include <WiFiManager.h>        // 2.0.17
#include <Adafruit_NeoPixel.h>  // 1.15.1
#include <EEPROM.h>
#include <time.h>

// ───── hardware setup ─────
#define LED_PIN      13
#define BUTTON_PIN   27      // mode
#define BRIGHT_PIN   14      // brightness
#define NUM_LEDS     126
#define EEPROM_SIZE  8
#define MODE_ADDR    0
#define BRIGHT_ADDR  4

Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

// ───── brightness table ─────
int brightnessLevels[] = {255,128,64,16,0};
const int NUM_BRIGHTNESS = sizeof(brightnessLevels)/sizeof(brightnessLevels[0]);
int currentBrightnessIndex = 0;

// ───── digit / segment map ─────
const uint8_t seg[7][3] = {
  {11,12,13},{ 8, 9,10},{ 3, 4, 5},
  { 0, 1, 2},{18,19,20},{14,15,16},{17, 6, 7}
};
const uint8_t CENTRE_LED = seg[6][1];

// segment masks 0-9
const bool digitMask[10][7] = {
  {1,1,1,1,1,1,0},{0,1,1,0,0,0,0},{1,1,0,1,1,0,1},{1,1,1,1,0,0,1},
  {0,1,1,0,0,1,1},{1,0,1,1,0,1,1},{1,0,1,1,1,1,1},{1,1,1,0,0,0,0},
  {1,1,1,1,1,1,1},{1,1,1,1,0,1,1}
};

// ───── colour tables ─────
const uint32_t baseColors[] = {
  strip.Color(  0,255,  0), // Red   (GRB)
  strip.Color(255,  0,  0), // Green
  strip.Color(  0,  0,255), // Blue
  strip.Color(255,255,  0), // Yellow
  strip.Color(255,  0,255), // Magenta
  strip.Color(  0,255,255), // Cyan
  strip.Color(255,255,255)  // White
};
const char* baseNames[] = {"Red","Green","Blue","Yellow","Magenta","Cyan","White"};
const int numBase = sizeof(baseColors)/sizeof(baseColors[0]);

struct Pattern { const char* name; uint32_t L,C,R; };
const Pattern patternModes[] = {
  {"C/R/C", strip.Color(  0,255,255), strip.Color(  0,255,  0), strip.Color(  0,255,255)},
  {"R/C/R", strip.Color(  0,255,  0), strip.Color(  0,255,255), strip.Color(  0,255,  0)},
  {"G/M/G", strip.Color(255,  0,255), strip.Color(255,  0,  0), strip.Color(255,  0,255)},
  {"M/G/M", strip.Color(255,  0,  0), strip.Color(255,  0,255), strip.Color(255,  0,  0)},
  {"O/B/O", strip.Color(  0,  0,255), strip.Color(255,128,  0), strip.Color(  0,  0,255)},
  {"B/O/B", strip.Color(255,128,  0), strip.Color(  0,  0,255), strip.Color(255,128,  0)},
  {"DB/LB/DB",strip.Color(  0,  0,128), strip.Color(128,128,255), strip.Color(  0,  0,128)},
  {"LB/DB/LB",strip.Color(128,128,255), strip.Color(  0,  0,128), strip.Color(128,128,255)},
  {"DR/O/DR",strip.Color(128,  0,  0), strip.Color(255,128,  0), strip.Color(128,  0,  0)},
  {"O/DR/O", strip.Color(255,128,  0), strip.Color(128,  0,  0), strip.Color(255,128,  0)},
  {"DG/LG/DG",strip.Color(  0,128,  0), strip.Color(128,255,128), strip.Color(  0,128,  0)},
  {"LG/DG/LG",strip.Color(128,255,128), strip.Color(  0,128,  0), strip.Color(128,255,128)},
  {"P/C/P", strip.Color(255,  0,255), strip.Color(  0,255,255), strip.Color(255,  0,255)},
  {"C/P/C", strip.Color(  0,255,255), strip.Color(255,  0,255), strip.Color(  0,255,255)},
  {"W/B/W", strip.Color(255,255,255), strip.Color(  0,  0,255), strip.Color(255,255,255)},
  {"B/W/B", strip.Color(  0,  0,255), strip.Color(255,255,255), strip.Color(  0,  0,255)},
  {"Y/P/Y", strip.Color(255,255,  0), strip.Color(255,  0,255), strip.Color(255,255,  0)},
  {"P/Y/P", strip.Color(255,  0,255), strip.Color(255,255,  0), strip.Color(255,  0,255)},
  {"B/R/B", strip.Color(  0,  0,255), strip.Color(  0,255,  0), strip.Color(  0,  0,255)},
  {"R/B/R", strip.Color(  0,255,  0), strip.Color(  0,  0,255), strip.Color(  0,255,  0)},
  {"B/R/B", strip.Color(  0,  0,255), strip.Color(  0,255,  0), strip.Color(  0,  0,255)},
  {"Y/B/Y", strip.Color(255,255,  0), strip.Color(  0,  0,255), strip.Color(255,255,  0)},
  {"B/Y/B", strip.Color(  0,  0,255), strip.Color(255,255,  0), strip.Color(  0,  0,255)},
  {"R/Y/R", strip.Color(  0,255,  0), strip.Color(255,255,  0), strip.Color(  0,255,  0)},
  {"Y/R/Y", strip.Color(255,255,  0), strip.Color(  0,255,  0), strip.Color(255,255,  0)}
};
const int numPatterns = sizeof(patternModes)/sizeof(patternModes[0]);

// ───── mode indices ─────
const int RAINBOW_MODE = numBase + numPatterns;  // 32
const int AUTO_1S_MODE = RAINBOW_MODE + 1;       // 33
const int AUTO_1M_MODE = AUTO_1S_MODE + 1;       // 34
const int AUTO_1H_MODE = AUTO_1M_MODE + 1;       // 35
const int TOTAL_MODES  = AUTO_1H_MODE + 1;       // 36

// ───── Wi-Fi colours ─────
const uint32_t PURPLE = strip.Color(  0,255,255);
const uint32_t YELLOW = strip.Color(255,255,  0);
const uint32_t WHITE  = strip.Color(255,150,150);
const uint32_t REDLED = strip.Color(  0,255,  0);
const uint32_t BLUELED= strip.Color(  0,  0,255);

// ───── time / Wi-Fi ─────
const char* TZ   = "EST5EDT,M3.2.0/2,M11.1.0/2";
time_t lastSync  = 0;
int    currentMode=0;

// ───── helpers ─────
void setStatusLED(uint32_t c){ strip.setPixelColor(CENTRE_LED,c); strip.show(); }
uint32_t rainbowColor(uint8_t i){ uint16_t h=(uint32_t)i*65535/21; return strip.ColorHSV(h,255,255); }

// ───── Wi-Fi ─────
void connectWiFi(){
  setStatusLED(PURPLE); delay(500);
  WiFiManager wm;
  String id = String((uint32_t)(ESP.getEfuseMac()>>32),HEX);
  if(!wm.autoConnect(("CLOCK_"+id).c_str())){
    setStatusLED(BLUELED); wm.startConfigPortal(("CLOCK_"+id).c_str());
  }
  setStatusLED(WHITE);
}
void syncTime(){ configTzTime(TZ,"pool.ntp.org","time.nist.gov"); while(time(nullptr)<1609459200) delay(200); lastSync=time(nullptr); }

// ───── drawing ─────
void drawDigit(uint8_t pos,uint8_t val,int mode,int sec){
  uint8_t off=pos*21;
  for(uint8_t s=0;s<7;++s){
    if(!digitMask[val][s]) continue;
    if(mode==RAINBOW_MODE){
      for(uint8_t j=0;j<3;++j) strip.setPixelColor(off+seg[s][j], rainbowColor(seg[s][j]));
      continue;
    }
    uint32_t l,c,r;
    if     (mode < numBase)       l=c=r=baseColors[mode];
    else if(mode < RAINBOW_MODE){ const Pattern& p=patternModes[mode-numBase]; l=p.L; c=p.C; r=p.R; }
    else                          l=c=r=baseColors[(sec/10)%numBase];
    strip.setPixelColor(off+seg[s][0],l);
    strip.setPixelColor(off+seg[s][1],c);
    strip.setPixelColor(off+seg[s][2],r);
  }
}
void showTime(struct tm& t,int mode){
  strip.clear();
  drawDigit(0,t.tm_hour/10,mode,t.tm_sec);
  drawDigit(1,t.tm_hour%10,mode,t.tm_sec);
  drawDigit(2,t.tm_min /10,mode,t.tm_sec);
  drawDigit(3,t.tm_min %10,mode,t.tm_sec);
  drawDigit(4,t.tm_sec /10,mode,t.tm_sec);
  drawDigit(5,t.tm_sec %10,mode,t.tm_sec);
  strip.show();
}
String modeName(int m){
  if(m<numBase)              return baseNames[m];
  if(m<RAINBOW_MODE)         return patternModes[m-numBase].name;
  if(m==RAINBOW_MODE)        return "Rainbow";
  if(m==AUTO_1S_MODE)        return "Auto-1 s";
  if(m==AUTO_1M_MODE)        return "Auto-1 m";
  if(m==AUTO_1H_MODE)        return "Auto-1 h";
  return "?";
}

// ───── setup ─────
void setup(){
  Serial.begin(115200);
  pinMode(BUTTON_PIN ,INPUT_PULLUP);
  pinMode(BRIGHT_PIN ,INPUT_PULLUP);
  strip.begin(); delay(10);

  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(MODE_ADDR   ,currentMode);
  EEPROM.get(BRIGHT_ADDR ,currentBrightnessIndex);
  if(currentMode<0||currentMode>=TOTAL_MODES){ currentMode=0; EEPROM.put(MODE_ADDR,currentMode); }
  if(currentBrightnessIndex<0||currentBrightnessIndex>=NUM_BRIGHTNESS){ currentBrightnessIndex=0; EEPROM.put(BRIGHT_ADDR,currentBrightnessIndex); }
  EEPROM.commit();
  strip.setBrightness(brightnessLevels[currentBrightnessIndex]);

  connectWiFi(); if(WiFi.status()==WL_CONNECTED) syncTime();

  time_t now=time(nullptr); struct tm t; localtime_r(&now,&t);
  Serial.printf("Start mode %d – %s\\n",currentMode,modeName(currentMode).c_str());
  showTime(t,currentMode);
}

// ───── loop ─────
void loop(){
  static bool wasModeBtn=false, wasBrightBtn=false;
  static int  lastSec=-1, lastMin=-1;
  static int  autoIdx=0;
  static time_t lastAutoTime=0;

  bool modeBtn = !digitalRead(BUTTON_PIN);
  if(modeBtn && !wasModeBtn){
    delay(50);
    if(!digitalRead(BUTTON_PIN)){
      while(!digitalRead(BUTTON_PIN)) delay(10);
      wasModeBtn=true;
      currentMode = (currentMode+1)%TOTAL_MODES;
      EEPROM.put(MODE_ADDR,currentMode); EEPROM.commit();
      Serial.printf("Mode → %d – %s\n",currentMode,modeName(currentMode).c_str());

      if(currentMode==AUTO_1S_MODE || currentMode==AUTO_1M_MODE || currentMode==AUTO_1H_MODE){
        for(int i=0;i<NUM_LEDS;++i) strip.setPixelColor(i,0xFFFFFF);
        strip.show(); delay(100); strip.clear(); strip.show();
        autoIdx=0; lastAutoTime=time(nullptr);
      }

      time_t n=time(nullptr); struct tm t; localtime_r(&n,&t);
      showTime(t,currentMode);
    }
  } else if(!modeBtn) wasModeBtn=false;

  bool brightBtn = !digitalRead(BRIGHT_PIN);
  if(brightBtn && !wasBrightBtn){
    delay(50);
    if(!digitalRead(BRIGHT_PIN)){
      while(!digitalRead(BRIGHT_PIN)) delay(10);
      wasBrightBtn=true;
      currentBrightnessIndex=(currentBrightnessIndex+1)%NUM_BRIGHTNESS;
      strip.setBrightness(brightnessLevels[currentBrightnessIndex]);
      EEPROM.put(BRIGHT_ADDR,currentBrightnessIndex); EEPROM.commit();
      Serial.printf("Brightness → %d\n",brightnessLevels[currentBrightnessIndex]);
      time_t n=time(nullptr); struct tm t; localtime_r(&n,&t); showTime(t,currentMode);
    }
  } else if(!brightBtn) wasBrightBtn=false;

  time_t now=time(nullptr); struct tm t; localtime_r(&now,&t);
  if(currentMode<=RAINBOW_MODE && t.tm_sec!=lastSec){
    lastSec=t.tm_sec; showTime(t,currentMode);
  }

  int interval = 0;
  if(currentMode==AUTO_1S_MODE) interval=1;
  else if(currentMode==AUTO_1M_MODE) interval=60;
  else if(currentMode==AUTO_1H_MODE) interval=3600;

  if(interval){
    if(now - lastAutoTime >= interval){
      lastAutoTime = now;
      autoIdx = (autoIdx+1)%AUTO_MODE_1;
      Serial.printf("Auto-cycle → %d – %s\n",autoIdx,modeName(autoIdx).c_str());
      showTime(t,autoIdx);
    }
  }

  if(WiFi.status()==WL_CONNECTED && now-lastSync>=3600) syncTime();
}

