// --------------------------------------------------------------------
// myFP2ESP32 FIRMWARE OFFICIAL RELEASE 306_07
// Copyright Robert Brown 2014-2023. All Rights Reserved.
// Copyright Holger M, 2019-2021. All Rights Reserved.
// Copyright Pieter P, SPIFFs examples found online
// Copyright Paul P, 2021-2022. All Rights Reserved. TMC22xx code
// Copyright Marco Gulino, 2022-2023. All Rights Reserved. MultiAP code.
//   myfp2esp32-firmware.ino
//   version: 306_07
//   date:    20-Aug-2023
// --------------------------------------------------------------------


// --------------------------------------------------------------------
// OFFICIAL FIRMWARE RELEASE 306_07
// Arduino IDE  1.8.19          https://downloads.arduino.cc/arduino-1.8.19-windows.zip
// Arduino ESP32 Core 2.0.11    https://github.com/espressif/arduino-esp32
// Arduino JSON 6.21.3          https://github.com/bblanchon/ArduinoJson
// esp8266-oled-ssd1306 4.4.0   https://github.com/ThingPulse/esp8266-oled-ssd1306
// ElegantOTA 2.2.7             Newer versions can have issues with upload
// TMCStepper Library           0.7.3 https://github.com/teemuatlut/TMCStepper
// Libraries are added using the Arduino IDE Library Manager
// ESP32 Core is added using the Arduino IDE Boards Manager
// --------------------------------------------------------------------


// --------------------------------------------------------------------
// PRINTED CIRCUIT BOARDS AND STRIPBOARDS
// --------------------------------------------------------------------
// https://sourceforge.net/projects/myfocuserpro2-esp32/files/PCB%20Gerber%20Files/
// https://sourceforge.net/projects/myfocuserpro2-esp32/files/Stripboard%20ESP32-DRV8825/


// --------------------------------------------------------------------
// CONFIG SECTION: YOU-TO-DO
// Edit controller_config.h and specify the controller options
// --------------------------------------------------------------------
// Specify BOARD AND TARGETCPU
// Specify DRVBRD
// Specify FIXEDSTEPMODE
// Specify STEPSPERREVOLUTION
// Specify CONTROLLER MODE, ACCESSPOINT or STATION
// Specify DISPLAY AND DRIVER TYPE
// Specify INFRAREDREMOTE
// Specify ELEGANTOTA
// Specify DUCKDNS


// --------------------------------------------------------------------
// INTERFACES
// --------------------------------------------------------------------
// ASCOM ALPACA SERVER port 4040
// ASCOM DISCOVERY PORT 32337
// Debug Server port 9090
// DUCKDNS
// Management Server port 6060 - Admin interface
// TCP/IP Server port 2020 - used by applications
// Web Server port 80


// ----------------------------------------------------------------------
// SPECIAL LICENSE
// ----------------------------------------------------------------------
// This code is released under license. If you copy or write new code based
// on the code in these files, you MUST include a link to these files AND
// you MUST include references to the author(s) of this code.


// ----------------------------------------------------------------------
// CONTRIBUTIONS
// ----------------------------------------------------------------------
// This project provides you with all the information you need to undertake
// the project. A high level of documentation and support is provided to
// assist your successful implementation of this project. I only ask that
// you consider a small donation in terms as a thank you, which can be done
// via PayPal and sent to user rbb1brown@gmail.com  (Robert Brown).


// ----------------------------------------------------------------------
// DO NOT EDIT BELOW THIS LINE
// ----------------------------------------------------------------------


// ----------------------------------------------------------------------
// CONTROLLER DEFINITIONS AND CONFIGURATION
// ----------------------------------------------------------------------
#include "driver_board.h"       // include driverboard class definitions
#include "controller_config.h"  // includes boarddefs.h and controller_defines.h


// ----------------------------------------------------------------------
// INCLUDES
// ----------------------------------------------------------------------
#include <Arduino.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include <ArduinoJson.h>
#include <SPIFFS.h>
#include <SPI.h>
#include <WiFi.h>
#include <Wire.h>
#include <esp_task_wdt.h>

#include <LOLIN_I2C_MOTOR.h>

//MN 08042024 enable builtin LED on ESP32-S3-Zero
#include <FastLED.h>+
#define NUM_LEDS 1
#define DATA_PIN 21
// Define the array of leds
CRGB leds[NUM_LEDS];

// --------------------------------------------------------------------
// CONTROLLERMODE
// --------------------------------------------------------------------
int myfp2esp32mode = CONTROLLERMODE;

// ACCESSPOINT /defines/accesspoint_defines.h
extern char myAPSSID[64];
extern char myAPPASSWORD[64];
extern IPAddress ap_ip;
extern IPAddress ap_dns;
extern IPAddress ap_gateway;
extern IPAddress ap_subnet;
#include "defines/accesspoint_defines.h"

// STATION /defines/station_defines.h
extern char mySSID[];
extern char myPASSWORD[];
extern char mySSID_1[];
extern char myPASSWORD_1[];
#include "defines/station_defines.h"

// MULTI-AP
#if defined(WIFICONFIGTYPE)
#if (WIFICONFIGTYPE == MULTIAP)




#include <WiFiMulti.h>
WiFiMulti wifiMulti;
#define MULTIAPDOCSIZE 1500  // 10 AP entries is 1280
#endif
#endif


// ----------------------------------------------------------------------
// Controller Data Class
// ----------------------------------------------------------------------
#include "controller_data.h"
CONTROLLER_DATA *ControllerData;


// --------------------------------------------------------------------
// DEBUG SERVER
// --------------------------------------------------------------------
#include "debug_server.h"
DEBUG_SERVER *debugsrvr;


// ----------------------------------------------------------------------
// DRIVER BOARD [uses Timer0]
// Default Configuration: Included
// ----------------------------------------------------------------------
#include "driver_board.h"
DRIVER_BOARD *driverboard;
// these capture compile time settings and are required to initialize a board correctly
int myboardnumber = DRVBRD;
int myfixedstepmode = FIXEDSTEPMODE;
int mystepsperrev = STEPSPERREVOLUTION;


// ----------------------------------------------------------------------
// ASCOM SERVER
// Default Configuration: Included
// ----------------------------------------------------------------------
#include "ascom_server.h"
ASCOM_SERVER *ascomsrvr;


// ----------------------------------------------------------------------
// TCP/IP SERVER
// Default Configuration: Included
// ----------------------------------------------------------------------
#include "tcpip_server.h"
TCPIP_SERVER *tcpipsrvr;


// ----------------------------------------------------------------------
// WEB SERVER
// Default Configuration: Included
// ----------------------------------------------------------------------
#include "web_server.h"
WEB_SERVER *websrvr;


// ----------------------------------------------------------------------
// MANAGEMENT SERVER
// Default Configuration: Included
// ----------------------------------------------------------------------
#include "management_server.h"
MANAGEMENT_SERVER *mngsrvr;


// ----------------------------------------------------------------------
// IN-OUT LEDS
// Default Configuration: Included
// ----------------------------------------------------------------------
// Loaded with DriverBoard


// ----------------------------------------------------------------------
// PUSHBUTTONS
// Default Configuration: Included
// ----------------------------------------------------------------------
// Loaded with DriverBoard


// ----------------------------------------------------------------------
// JOYSTICKS
// Default Configuration: Included
// ----------------------------------------------------------------------
// Loaded with DriverBoard


// ----------------------------------------------------------------------
// DISPLAY
// Library  Text      myOLED
// Library  Graphics  https://github.com/ThingPulse/esp8266-oled-ssd1306
// Optional
// ----------------------------------------------------------------------
#if defined(ENABLE_TEXTDISPLAY)
#include "display_text.h"
TEXT_DISPLAY *mydisplay;
#endif
#if defined(ENABLE_GRAPHICDISPLAY)
#include "display_graphics.h"
GRAPHIC_DISPLAY *mydisplay;
#endif


// ----------------------------------------------------------------------
// IR REMOTE
// Library  myfp2eIRremoteESP8266.zip
// Optional
// ----------------------------------------------------------------------
#if defined(ENABLE_INFRAREDREMOTE)
#include "ir_remote.h"
IR_REMOTE *irremote;
#endif


// ----------------------------------------------------------------------
// TEMPERATURE PROBE
// Library  myDallasTemperature
// Default Configuration: Included
// ----------------------------------------------------------------------
#include "temp_probe.h"
TEMP_PROBE *tempprobe;


// ----------------------------------------------------------------------
// OTAUPDATES (OVER THE AIR UPDATE) ELEGANTOTA
// Library https://github.com/ayushsharma82/ElegantOTA
// Optional
// ----------------------------------------------------------------------
// Set initial values for name and password in defines/otaupdates_defines.h
extern const char *OTAName;
extern const char *OTAPassword;


// ----------------------------------------------------------------------
// DUCKDNS [STATION ONLY]
// Library https://github.com/ayushsharma82/EasyDDNS
// Optional
// ----------------------------------------------------------------------
// Set initial values for duckdnsdomain and duckdnstoken in defines/duckdns_defines.h
extern char duckdnsdomain[];
extern char duckdnstoken[];
#include "defines/duckdns_defines.h"
#if defined(ENABLE_DUCKDNS)
#include "duckdns.h"
DUCK_DNS *cntlr_DuckDNS;
#endif


// ----------------------------------------------------------------------
// FOCUSER CONTROLLER DATA
// ----------------------------------------------------------------------
//extern const char *project_name;
//extern const char *program_version;
//extern const char *program_author;
#include "defines/app_defines.h"

// cached vars for management/web server pages
char devicename[32];
char titlecolor[8];
char subtitlecolor[8];
char headercolor[8];
char textcolor[8];
char backcolor[8];

// At 1st reboot after uploading data files the Management and TCPIP
// servers are enabled and started. For all other options, the options
// are not enabled and not started. If an option is then enabled via the
// Management server, the option state will be saved as enabled.

// On subsequent boots
// Any option that is found enabled will be initialised and started.
// If the start is successful then the status for that option will
// be set to V_RUNNING

// Runtime status flags for devices and servers
// state  = enabled (status can be stopped or running), notenabled (status will be stopped)
// status = v_stopped or v_running, on or off
byte ascomsrvr_status;  // V_STOPPED
byte display_status;    // V_STOPPED
byte duckdns_status;    // V_STOPPED
bool irremote_status;   // V_STOPPED
byte mngsrvr_status;    // V_RUNNING
byte ota_status;        // V_STOPPED
byte tcpipsrvr_status;  // V_RUNNING
byte websrvr_status;    // V_STOPPED
bool debugsrvr_status;

// There is no pushbutton state and inout-led state because they are in Driverboard


// ----------------------------------------------------------------------
// Task timer [timer1]
// handles state machine for options (display, temp probe, park, config saves)
// ----------------------------------------------------------------------
#include "tasktimer.h"
extern volatile int update_temp_flag;
extern volatile int update_display_flag;
extern volatile int update_park_flag;
extern volatile int save_board_flag;
extern volatile int save_cntlr_flag;
extern volatile int save_var_flag;
extern volatile int update_wifi_flag;  // check wifi connection - applicable to Station Mode only
extern volatile unsigned int park_maxcount;
extern volatile unsigned int display_maxcount;
extern portMUX_TYPE boardMux;
extern portMUX_TYPE cntlrMux;
extern portMUX_TYPE varMux;
extern portMUX_TYPE wifiMux;
extern portMUX_TYPE tempMux;
extern portMUX_TYPE displayMux;      // for the flag, now its time to update page on display
extern portMUX_TYPE displaytimeMux;  // for updating the actual time delay value display_maxcount
extern portMUX_TYPE parkMux;

// Mutex's required for focuser halt and move
volatile bool timerSemaphore = false;                           // move completed=true, still moving or not moving = false;
portMUX_TYPE timerSemaphoreMux = portMUX_INITIALIZER_UNLOCKED;  // protects timerSemaphore
volatile uint32_t stepcount;                                    // number of steps to go
portMUX_TYPE stepcountMux = portMUX_INITIALIZER_UNLOCKED;       // protects stepcount
volatile bool halt_alert;                                       // indicates if a halt occured when motor was moving
portMUX_TYPE halt_alertMux = portMUX_INITIALIZER_UNLOCKED;      // protects halt_alert


// FOCUSER
long ftargetPosition;              // target position
bool isMoving;                     // is the motor currently moving (true / false)
float temp;                        // the last temperature read
int update_delay_after_move_flag;  // when set to 1, indicates the flag has been set, default = 0, disabled = -1
enum Display_Types displaytype;    // None, text, graphics


// CONTROLLER
long rssi;                           // network signal strength in Stationmode
bool reboot_start;                   // flag used to indicate a reboot has occurred
bool filesystemloaded;               // flag indicator for webserver usage, rather than use SPIFFS.begin() test
char ipStr[16] = "000.000.000.000";  // shared between BT mode and other modes
char systemuptime[12];               // ddd:hh:mm
IPAddress ESP32IPAddress;
IPAddress myIP;

// For debugging
const char *T_FROM = " From ";
const char *T_TO = " to ";
const char *T_BLSTEPS = "BLsteps ";
const char *T_BLCOUNT = "BLcount ";
const char *T_STEPS = "steps ";
const char *T_STATEBL = "State_Backlash steps=";
const char *T_BLDONE = "BL done";
const char *T_MOVESTEPS = "Initiate motor move, steps ";
const char *T_MOVEDONE = "Move done";
const char *T_HALTALERT = "Halt_alert";
const char *T_HPSWALERT = "HPSW_alert";
const char *T_HPSWMSG1 = "HPSw=1, Pos=0";
const char *T_HPSWMSG2 = "StallGuard Pos = 0";
const char *T_HPSWMSG3 = "HPSw=0, Movout";
const char *T_HPSWMSG4 = "HPSw=0, Movout err";
const char *T_HPSWMSG5 = "HPEnd, Movout steps ";
const char *T_HPSWMSG6 = "HPSw=0, Movout ok";

const char *T_INITMOVE = "go State_InitMove";
const char *T_GOMOVING = "go State_Moving";
const char *T_GODELAYAFTERMOVE = "go State_DelayAfterMove";
const char *T_GOSETHOMEPOSITION = "go State_SetHomePosition";
const char *T_GOENDMOVE = "go State_EndMove";
const char *T_ENDMOVE = "park=enabled, update_park_flag=0, start count";
const char *T_RECONNECTWIFI = "err !connected: trying reconnect";
const char *T_CONFIGSAVED = "config saved";


// ----------------------------------------------------------------------
// PARK, DISPLAY, COILPOWER INTERACTION
// ----------------------------------------------------------------------
// for displays, oled_state controls if the display is on or off
// This is done via the use "move", "park" and "parktime".
// Park = false display is on
// Park = true  display is off
// When the motor moves, park = false and display turned on
// When the motor stops, parktime flag is set to 0, task timer starts counting to 30s
// When count is reached parktime flag is set to 1
// In loop(), parktime flag is then disabled, park set to to 1, display turns off
// "park" is also used to control the enable/disable of Coil Power
// The "park" feature is enabled/disabled in the Management Server
Oled_States oled_state = oled_off;


// --------------------------------------------------------------
// HELPER FUNCTIONS
// USED THROUGHOUT THIS CODE AND REFERENCED BY CLASSES
// required because the destination class might not exist depending on
// the controllerconfig.h settings.
// Using a help function, the class can be hidden from the calling
// request, and appropriate true/false returned according to if
// the class has been defined or not.
// --------------------------------------------------------------


// --------------------------------------------------------------
// support function : void fmemcpy( char *, char *, int );
// fast memcpy, null terminate destination
// --------------------------------------------------------------
void fmemcpy(char *target, char *source, int i) {
  char *dest = target;
  const char *org = source;
  while (i--) {
    *dest++ = *org++;
  }
  *dest = 0x00;
}


// --------------------------------------------------------------------
// print message char array
// --------------------------------------------------------------------
void debug_server_print(const char *s) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_message(s);
  }
#endif
}

// --------------------------------------------------------------------
// print char
// --------------------------------------------------------------------
void debug_server_print(char c) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_message(c);
  }
#endif
}

// --------------------------------------------------------------------
// print int
// --------------------------------------------------------------------
void debug_server_print(int d) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_message(d);
  }
#endif
}

// --------------------------------------------------------------------
// print unsigned int
// --------------------------------------------------------------------
void debug_server_print(unsigned int d) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_message(d);
  }
#endif
}

// --------------------------------------------------------------------
// print long
// --------------------------------------------------------------------
void debug_server_print(long d) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_message(d);
  }
#endif
}

// --------------------------------------------------------------------
// print unsigned long
// --------------------------------------------------------------------
void debug_server_print(long unsigned d) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_message(d);
  }
#endif
}

// --------------------------------------------------------------------
// print double
// --------------------------------------------------------------------
void debug_server_print(double d) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_message(d);
  }
#endif
}

// --------------------------------------------------------------------
// print String
// --------------------------------------------------------------------
void debug_server_print(String s) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_message(s);
  }
#endif
}

// --------------------------------------------------------------------
// print message followed by newline, char array
// --------------------------------------------------------------------
void debug_server_println(const char *s) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_messagenewline(s);
  }
#endif
}

// --------------------------------------------------------------------
// print char followed by newline
// --------------------------------------------------------------------
void debug_server_println(char c) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_messagenewline(c);
  }
#endif
}

// --------------------------------------------------------------------
// print int followed by newline
// --------------------------------------------------------------------
void debug_server_println(const int d) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_messagenewline(d);
  }
#endif
}

// --------------------------------------------------------------------
// print long followed by newline
// --------------------------------------------------------------------
void debug_server_println(long d) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_messagenewline(d);
  }
#endif
}

// --------------------------------------------------------------------
// print unsigned long followed by newline
// --------------------------------------------------------------------
void debug_server_println(long unsigned d) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_messagenewline(d);
  }
#endif
}

// --------------------------------------------------------------------
// print unsigned int followed by newline
// --------------------------------------------------------------------
void debug_server_println(const unsigned int d) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_messagenewline(d);
  }
#endif
}

// --------------------------------------------------------------------
// print double followed by newline
// --------------------------------------------------------------------
void debug_server_println(double d) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_messagenewline(d);
  }
#endif
}

// --------------------------------------------------------------------
// print String followed by newline
// --------------------------------------------------------------------
void debug_server_println(String s) {
#if defined(DEBUGSERVER)
  if (debugsrvr_status) {
    debugsrvr->send_messagenewline(s);
  }
#endif
}

// --------------------------------------------------------------------
// initialise DEBUG Server
// --------------------------------------------------------------------
bool init_debugserver() {
  debugsrvr = new DEBUG_SERVER();
  return true;
}

// --------------------------------------------------------------------
// start Debug Server
// --------------------------------------------------------------------
bool start_debugserver(void) {
  if (ControllerData->get_debugsrvr_enable())
    if (debugsrvr_status == V_STOPPED) {
      debugsrvr_status = debugsrvr->start_server();
      //Serial.print(T_START);
      //Serial.print(T_DEBUGSERVER);
      //Serial.println(debugsrvr_status);
      return debugsrvr_status;
    }
  debugsrvr_status = V_STOPPED;
  return false;
}

// --------------------------------------------------------------------
// stop Debug Server
// --------------------------------------------------------------------
void stop_debugserver(void) {
  debugsrvr->stop_server();
  debugsrvr_status = V_STOPPED;
  //Serial.print(T_DEBUGSERVER);
  //Serial.println(T_STOPPED);
}


// ----------------------------------------------------------------------
// bool init_display(void);
// initialise display vars and create pointer to the display class
// helper, optional
// ----------------------------------------------------------------------
bool display_init() {
  displaytype = Type_None;
#if defined(ENABLE_TEXTDISPLAY) || defined(ENABLE_GRAPHICDISPLAY)
#if defined(ENABLE_TEXTDISPLAY)
  displaytype = Type_Text;
  mydisplay = new TEXT_DISPLAY(OLED_ADDR);
  return true;
#endif
#if defined(ENABLE_GRAPHICDISPLAY)
  displaytype = Type_Graphic;
  mydisplay = new GRAPHIC_DISPLAY(OLED_ADDR);
  return true;
#endif
#endif  // #if defined(ENABLE_TEXTDISPLAY) || defined(ENABLE_GRAPHICDISPLAY)
  return false;
}

// ----------------------------------------------------------------------
// bool display_start(void);
// Display start
// helper, optional
// ----------------------------------------------------------------------
bool display_start(void) {
  if (display_status == V_RUNNING) {
    return true;
  }
  portENTER_CRITICAL(&displayMux);
  update_display_flag = -1;
  portEXIT_CRITICAL(&displayMux);

#if defined(ENABLE_TEXTDISPLAY) || defined(ENABLE_GRAPHICDISPLAY)
  // only start the display if is enabled in ControllerData
  if (ControllerData->get_display_enable() == V_ENABLED) {
    if (mydisplay->start() != true) {
      display_status = V_STOPPED;
      portENTER_CRITICAL(&displayMux);
      update_display_flag = -1;
      portEXIT_CRITICAL(&displayMux);
      return false;
    } else {
      display_status = V_RUNNING;
      portENTER_CRITICAL(&displayMux);
      update_display_flag = 0;
      portEXIT_CRITICAL(&displayMux);
      return true;
    }
  }
#endif
  return false;
}

// ----------------------------------------------------------------------
// void display_stop(void);
// Stop disables the display and release() display,
// sets ControllerData->display_enable state to V_NOTENABLED
// sets display_status to V_STOPPED
// helper, optional
// ----------------------------------------------------------------------
void display_stop(void) {
#if defined(ENABLE_TEXTDISPLAY) || defined(ENABLE_GRAPHICDISPLAY)
  portENTER_CRITICAL(&displayMux);
  update_display_flag = -1;
  portEXIT_CRITICAL(&displayMux);
  mydisplay->stop();
  display_status = V_STOPPED;
#endif
}

// ----------------------------------------------------------------------
// void display_update();
// Update display
// helper, optional
// ----------------------------------------------------------------------
void display_update() {
#if defined(ENABLE_TEXTDISPLAY) || defined(ENABLE_GRAPHICDISPLAY)
  if (oled_state == oled_off) {
    return;
  }
  mydisplay->update();
#endif
}

// ----------------------------------------------------------------------
// void display_update_position(long);
// Update display with focuser position
// helper, optional
// ----------------------------------------------------------------------
void display_update_position(long fposition) {
#if defined(ENABLE_TEXTDISPLAY) || defined(ENABLE_GRAPHICDISPLAY)
  if (oled_state == oled_off) {
    return;
  }

  // if update position when moving enabled, then update position
  if (ControllerData->get_display_updateonmove() == V_ENABLED) {
    mydisplay->update_position(fposition);
  }
#endif
}

// ----------------------------------------------------------------------
// void display_off(void);
// Turn display off (blank)
// helper, optional
// ----------------------------------------------------------------------
void display_off() {
#if defined(ENABLE_TEXTDISPLAY) || defined(ENABLE_GRAPHICDISPLAY)
  mydisplay->off();
#endif
}

// ----------------------------------------------------------------------
// void display_on(void);
// Turn display on
// helper, optional
// ----------------------------------------------------------------------
void display_on() {
#if defined(ENABLE_TEXTDISPLAY) || defined(ENABLE_GRAPHICDISPLAY)
  mydisplay->on();
#endif
}

// ----------------------------------------------------------------------
// void display_clear(void);
// Turn display clear
// helper, optional
// ----------------------------------------------------------------------
void display_clear() {
#if defined(ENABLE_TEXTDISPLAY) || defined(ENABLE_GRAPHICDISPLAY)
  mydisplay->clear();
#endif
}

// ----------------------------------------------------------------------
// bool irremote_init(void);
// Initialise infra-red remote
// helper, optional
// ----------------------------------------------------------------------
bool irremote_init() {
  irremote_status = V_STOPPED;
#if defined(ENABLE_INFRAREDREMOTE)
  if (ControllerData->get_brdirpin() != -1) {
    irremote = new IR_REMOTE();
    return true;
  }
#endif
  return false;
}

// ----------------------------------------------------------------------
// bool irremote_start(void);
// Initialise and start Infra red Remote
// helper, optional
// ----------------------------------------------------------------------
bool irremote_start(void) {
#if defined(ENABLE_INFRAREDREMOTE)
  if (irremote_status == V_RUNNING) {
    return true;
  }
  irremote->start();
  irremote_status = V_RUNNING;
  return true;
#endif
  irremote_status = V_STOPPED;
  return false;
}

// ----------------------------------------------------------------------
// bool irremote_update(void);
// Read Infra red Remote
// helper, optional
// ----------------------------------------------------------------------
void irremote_update(void) {
#if defined(ENABLE_INFRAREDREMOTE)
  if (irremote_status == V_STOPPED) {
    return;
  }
  irremote->update();
#endif
}


// ----------------------------------------------------------------------
// bool duckdns_start(void);
// Start Duck DNS service, only starts service if it has been enabled in the firmware
// There is no stop()
// helper, optional
// ----------------------------------------------------------------------
bool duckdns_start() {
#if defined(ENABLE_DUCKDNS)
  if (duckdns_status == V_RUNNING) {
    return true;
  }
  if (ControllerData->get_duckdns_enable() == V_ENABLED) {
    cntlr_DuckDNS = new DUCK_DNS();
    if (cntlr_DuckDNS->start() == true) {
      duckdns_status = V_RUNNING;
      return true;
    }
  }
#endif
  duckdns_status = V_STOPPED;
  return false;
}

// ----------------------------------------------------------------------
// void duckdns_refresh_time(unsigned int);
// Set value of refresh time, which is the time bewteen sending updates to duckdns server
// helper, optional
// ----------------------------------------------------------------------
void duckdns_refreshtime(int rt) {
  // limit range from 60s to 3600 (1hr), default is 120s
  rt = (rt < 60) ? 60 : rt;
  rt = (rt > 3600) ? 3600 : rt;
  // if a different refresh time then save it
  if (ControllerData->get_duckdns_refreshtime() != rt) {
    ControllerData->set_duckdns_refreshtime(rt);
  }
#if defined(ENABLE_DUCKDNS)
  if (duckdns_status == V_RUNNING) {
    unsigned long dtime = ControllerData->get_duckdns_refreshtime() * 1000;
    cntlr_DuckDNS->set_refresh(dtime);
  }
#endif
}


// ----------------------------------------------------------------------
// get wifi signal strength (in stationmode)
// long getrssi(int);
// ----------------------------------------------------------------------
long getrssi() {
  long strength = WiFi.RSSI();
  return strength;
}


// ----------------------------------------------------------------------
// void get_systemuptime(void);
// CALCULATE SYSTEM UPTIME
// Outputs:  String systemuptime as dd:hh:mm   (days:hours:minutes)
// ----------------------------------------------------------------------
void get_systemuptime() {
  unsigned long elapsedtime = millis();
  int systemuptime_m = int((elapsedtime / (1000 * 60)) % 60);
  int systemuptime_h = int((elapsedtime / (1000 * 60 * 60)) % 24);
  int systemuptime_d = int((elapsedtime / (1000 * 60 * 60 * 24)) % 365);
  snprintf(systemuptime, 10, "%03d:%02d:%02d", systemuptime_d, systemuptime_h, systemuptime_m);
}


// ----------------------------------------------------------------------
// TimeCheck
// Delay after move only, All other states and times handled by task timer
// no longer used to implement delays
// ----------------------------------------------------------------------
byte TimeCheck(unsigned long x, unsigned long Delay) {
  unsigned long y = x + Delay;
  unsigned long z = millis();  // pick current time

  if ((x > y) && (x < z))
    return 0;  // overflow y
  if ((x < y) && (x > z))
    return 1;  // overflow z

  return (y < z);  // no or (z and y) overflow
}


// ----------------------------------------------------------------------
// void reboot_esp32(int);
// reboot controller
// ----------------------------------------------------------------------
void reboot_esp32(int reboot_delay) {
  if (isMoving == true) {
    driverboard->end_move();
  }
  // save the focuser settings immediately
  ControllerData->SaveNow(driverboard->getposition(), driverboard->getdirection());
  delay(reboot_delay);
  ESP.restart();
}


// ----------------------------------------------------------------------
// void steppermotormove(byte);
// move motor without updating position
// only used by sethomeposition and backlash
// direction moving_in, moving_out ^ reverse direction
// ignores ledmode
// ----------------------------------------------------------------------
void steppermotormove(byte ddir) {
  if (driverboard->get_leds_loaded() == true) {
    (ddir == moving_in) ? digitalWrite(ControllerData->get_brdinledpin(), 1) : digitalWrite(ControllerData->get_brdoutledpin(), 1);
  }
  driverboard->movemotor(ddir, false);
  if (driverboard->get_leds_loaded() == true) {
    (ddir == moving_in) ? digitalWrite(ControllerData->get_brdinledpin(), 0) : digitalWrite(ControllerData->get_brdoutledpin(), 0);
  }
}


// --------------------------------------------------------------------
// READ WIFICONFIG SSID/PASSWORD FROM FILE
// Inputs: wificonfig.jsn
// Outputs: mstatus
// --------------------------------------------------------------------
bool readwificonfig(char *xSSID, char *xPASSWORD, char *ySSID, char *yPASSWORD) {
#if defined(WIFICONFIGTYPE)
#if (WIFICONFIGTYPE == READWIFICONFIG)
  const String filename = "/wificonfig.jsn";
  String SSID_1, SSID_2;
  String PASSWORD_1, PASSWORD_2;

  // SPIFFS may have failed to start
  if (!filesystemloaded) {
    return false;
  }
  File f = SPIFFS.open(filename, "r");
  if (f) {
    String fdata = f.readString();
    f.close();

    // allocate json buffer, assistant = 192
    DynamicJsonDocument doc(250);
    DeserializationError jerror = deserializeJson(doc, fdata);
    if (!jerror) {
      // Decode JSON/Extract values
      SSID_1 = doc["mySSID"].as<const char *>();
      PASSWORD_1 = doc["myPASSWORD"].as<const char *>();
      SSID_2 = doc["mySSID_1"].as<const char *>();
      PASSWORD_2 = doc["myPASSWORD_1"].as<const char *>();

      // get first pair
      SSID_1.toCharArray(xSSID, SSID_1.length() + 1);
      PASSWORD_1.toCharArray(xPASSWORD, PASSWORD_1.length() + 1);

      // get second pair
      SSID_2.toCharArray(ySSID, SSID_2.length() + 1);
      PASSWORD_2.toCharArray(yPASSWORD, PASSWORD_2.length() + 1);
      return true;
    }
  }
#endif
#endif
  return false;
}


// --------------------------------------------------------------------
// READ MULTI-AP FROM FILE AND CONNECT
// Inputs: wifimulticonfig.jsn
// Outputs: connect status
// --------------------------------------------------------------------
bool wifiMultiSuccessful() {
#if defined(WIFICONFIGTYPE)
#if (WIFICONFIGTYPE == MULTIAP)
  const String filename = "/wifimulticonfig.jsn";
  if (filesystemloaded == false) {
    return false;
  }

  //Serial.print(T_START); Serial.println(T_MULTIAP);

  File f = SPIFFS.open(filename, "r");
  if (!f) {
    //Serial.println(T_NOTFOUND);
  } else {
    // JSON assistant - https://arduinojson.org/v5/assistant/
    // [256] for 2 AP's, 1280 = 10 AP's
    DynamicJsonDocument doc(MULTIAPDOCSIZE);
    DeserializationError jerror = deserializeJson(doc, f);
    f.close();
    if (jerror) {
      //Serial.(T_DESERIALISEERROR);
      //Serial.println(jerror.c_str());
      return false;
    }
    bool ap_sta = doc["ap_sta"] | true;
    if (ap_sta) {
      WiFi.mode(WIFI_MODE_APSTA);
      WiFi.softAP(ControllerData->get_devicename().c_str(), doc["ap_sta_psk"] | "");
    } else {
      WiFi.mode(WIFI_MODE_STA);
    }
    auto accessPoints = doc["APs"].as<JsonArray>();
    for (JsonVariant accessPoint : accessPoints) {
      //Serial.print(T_SSID);
      //Serial.print(accessPoint["ssid"].as<String>());
      //Serial.print(T_PWD);
      //Serial.println(accessPoint["psk"].as<String>());
      wifiMulti.addAP(accessPoint["ssid"], accessPoint["psk"]);
    }
    auto result = wifiMulti.run();
    //Serial.print(T_OK);
    //Serial.println(result == WL_CONNECTED);
    if (result == WL_CONNECTED) {
      //Serial.print(T_CONNECTED);
      //Serial.println(WiFi.SSID());
    }
    return result == WL_CONNECTED || ap_sta;
  }
#endif
#endif
  return false;
}


//-------------------------------------------------
// void load_vars(void);
// Load cached vars, gives quicker access for web pages etc
// when any cached variable changes, it is reloaded
//-------------------------------------------------
void load_vars() {
  reboot_start = true;
  halt_alert = false;
  isMoving = false;
  update_delay_after_move_flag = -1;
  update_wifi_flag = 0;

  // ascom server
  ascomsrvr_status = V_STOPPED;

  // Controller Data
  save_cntlr_flag = -1;
  save_var_flag = -1;
  save_board_flag = -1;

  // debug server
  debugsrvr_status = V_STOPPED;

  // display
  oled_state = oled_on;
  displaytype = Type_None;
  display_status = V_STOPPED;
  portENTER_CRITICAL(&displayMux);
  update_display_flag = -1;
  portEXIT_CRITICAL(&displayMux);

  // duckdns
  duckdns_status = V_STOPPED;

  // irremote
  irremote_status = V_STOPPED;

  // management server
  mngsrvr_status = V_STOPPED;

  // ota
  ota_status = V_STOPPED;

  // park
  update_park_flag = -1;

  // tcpip server
  tcpipsrvr_status = V_STOPPED;

  // temperature probe
  temp = 20.0;
  ControllerData->set_tcavailable(V_NOTENABLED);
  update_temp_flag = -1;

  // webserver
  websrvr_status = V_STOPPED;

  // cached vars
  String tmp;
  tmp = ControllerData->get_devicename();
  tmp.toCharArray(devicename, tmp.length() + 1);

  tmp = ControllerData->get_wp_titlecolor();
  tmp.toCharArray(titlecolor, tmp.length() + 1);

  tmp = ControllerData->get_wp_subtitlecolor();
  tmp.toCharArray(subtitlecolor, tmp.length() + 1);

  tmp = ControllerData->get_wp_headercolor();
  tmp.toCharArray(headercolor, tmp.length() + 1);

  tmp = ControllerData->get_wp_textcolor();
  tmp.toCharArray(textcolor, tmp.length() + 1);

  tmp = ControllerData->get_wp_backcolor();
  tmp.toCharArray(backcolor, tmp.length() + 1);
}


//-------------------------------------------------
// void setup(void)
//-------------------------------------------------
void setup() {
  // serial port is used for runtime messages and debugging support
  Serial.begin(SERIALPORTSPEED);

  //MN 08042024
  FastLED.addLeds<WS2812, DATA_PIN, RGB>(leds, NUM_LEDS);  // GRB ordering is typical
  //MN 08042024
  
  if (myboardnumber != PRO2ESP32LOLINS2MINI) {
    while (!Serial) {
      ;
    }
  }

  //Serial.print(T_START); Serial.println(T_SERIAL);
  filesystemloaded = false;


  //-------------------------------------------------
  // READ FOCUSER CONFIGURATION SETTINGS FROM CONFIG FILES
  //-------------------------------------------------
  // create pointer to the class and start
  boot_msg_print(T_START);
  boot_msg_println(T_CONTROLLERDATA);
  ControllerData = new CONTROLLER_DATA();


  //-------------------------------------------------
  // Initialise vars
  //-------------------------------------------------
  //Serial.println("Load vars");
  load_vars();


  //-------------------------------------------------
  // CONTROLLERMODE
  //-------------------------------------------------
  boot_msg_print("MODE ");
  boot_msg_println(myfp2esp32mode);


  //-------------------------------------------------
  // ACCESSPOINT START
  //-------------------------------------------------
  if (myfp2esp32mode == ACCESSPOINT) {
    boot_msg_print(T_START);
    boot_msg_println(T_ACCESSPOINT);
    WiFi.mode(WIFI_AP);
    delay(500);
    WiFi.config(ap_ip, ap_dns, ap_gateway, ap_subnet);
    delay(500);
    WiFi.softAP(myAPSSID, myAPPASSWORD);
  }


  //-------------------------------------------------
  // STATION START
  // Setup Station, as a station connecting to an existing wifi network
  //-------------------------------------------------
  if (myfp2esp32mode == STATION) {
    boot_msg_print(T_START);
    boot_msg_println(T_STATION);

#if defined(WIFICONFIGTYPE)
#if (WIFICONFIGTYPE == MULTIAP)
    boot_msg_print(T_START);
    boot_msg_println(T_MULTIAP);
    if (wifiMultiSuccessful() == false) {
      boot_msg_println(T_ERROR);
      reboot_esp32(2000);
    }
#elif (WIFICONFIGTYPE == READWIFICONFIG)
    // READWIFICONFIG, Station Mode only
    // read mySSID, myPASSWORD from file if file exists, otherwise use defaults
    boot_msg_print(T_START);
    boot_msg_println(T_READWIFICONFIG);
    readwificonfig(mySSID, myPASSWORD, mySSID_1, myPASSWORD_1);
#endif

    WiFi.mode(WIFI_STA);
    // if static ip then set this up before starting
    if (IPADDRESSMODE == STATICIP) {
      boot_msg_print(T_START);
      boot_msg_println(T_STATICIP);
      if (!WiFi.config(station_ip, station_gateway, station_subnet, station_dns1, station_dns2)) {
        boot_msg_println(T_ERROR);
      }
      delay(50);
    } else {
      boot_msg_print(T_START);
      boot_msg_println(T_DYNAMICIP);
    }
    // attempt to connect to user's wifi
    WiFi.begin(mySSID, myPASSWORD);
    WiFi.setSleep(false);
    boot_msg_print(T_SSID);
    boot_msg_println(mySSID);
    boot_msg_print(T_PWD);
    boot_msg_println(myPASSWORD);
    delay(1000);
    for (int attempts = 0; WiFi.status() != WL_CONNECTED; attempts++) {
      delay(500);
      if (attempts > 9) {
        delay(500);
        break;
      }
    }

    // check if connected after using first set of credentials
    // if not connected try second pair of credentials
    if (WiFi.status() != WL_CONNECTED) {
      // try again with 2nd set of credentials then reboot
      // after 10 failed attempts to log on
      // copy 2nd credentials into mySSID and myPASSWORD (these are referenced elsewhere)
      memset(mySSID, 0, 64);
      memset(myPASSWORD, 0, 64);
      memcpy(mySSID, mySSID_1, (sizeof(mySSID_1) / sizeof(mySSID_1[0])));
      memcpy(myPASSWORD, myPASSWORD_1, (sizeof(myPASSWORD_1) / sizeof(myPASSWORD_1[0])));
      boot_msg_println(T_ALTCREDENTIALS);
      boot_msg_print(T_SSID);
      boot_msg_println(mySSID);
      boot_msg_print(T_PWD);
      boot_msg_println(myPASSWORD);
      // attempt to start the WiFi with 2nd set alternative credentials
      WiFi.begin(mySSID, myPASSWORD);
      WiFi.setSleep(false);
      delay(1000);
      for (int attempts = 0; WiFi.status() != WL_CONNECTED; attempts++) {
        delay(500);
        if (attempts > 9) {
          boot_msg_print(T_ERROR);
          boot_msg_println(mySSID);
          reboot_esp32(2000);
        }
      }
    }
#endif
  }


  //-------------------------------------------------
  // START DEBUG SERVER
  //-------------------------------------------------
  boot_msg_print(T_START);
  boot_msg_println(T_DEBUGSERVER);
  if (init_debugserver() == false) {
    boot_msg_println(T_ERROR);
  } else if (start_debugserver() == false) {
    boot_msg_println(T_ERROR);
  }
  // It is now safe to use Debug Server


  //-------------------------------------------------
  // CONNECTION DETAILS
  // connection established
  //-------------------------------------------------
  ESP32IPAddress = WiFi.localIP();
  snprintf(ipStr, sizeof(ipStr), "%i.%i.%i.%i", ESP32IPAddress[0], ESP32IPAddress[1], ESP32IPAddress[2], ESP32IPAddress[3]);
  boot_msg_print(T_IPADDRESS);
  boot_msg_println(ipStr);

  boot_msg_print(T_HOSTNAME);
  boot_msg_println(WiFi.getHostname());


  //-------------------------------------------------
  // SYSTEM UP TIME START
  // days:hours:minutes
  //-------------------------------------------------
  boot_msg_print(T_START);
  boot_msg_println(T_SYSTEMUPTIME);
  get_systemuptime();


  //-------------------------------------------------
  // SETUP DRIVER BOARD
  //-------------------------------------------------
  boot_msg_print(T_START);
  boot_msg_println(T_DRIVERBOARD);

  // ensure targetposition will be same as focuser position
  // else after loading driverboard focuser will start moving immediately
  ftargetPosition = ControllerData->get_fposition();
  driverboard = new DRIVER_BOARD();
  driverboard->start(ControllerData->get_fposition());

  // Range checks for safety reasons
  ControllerData->set_brdstepmode((ControllerData->get_brdstepmode() < 1) ? 1 : ControllerData->get_brdstepmode());
  ControllerData->set_coilpower_enable((ControllerData->get_coilpower_enable() >= 1) ? 1 : 0);
  ControllerData->set_reverse_enable((ControllerData->get_reverse_enable() >= 1) ? 1 : 0);
  int pgtime = ControllerData->get_display_pagetime();
  pgtime = (pgtime < V_DISPLAYPAGETIMEMIN) ? V_DISPLAYPAGETIMEMIN : pgtime;
  pgtime = (pgtime > V_DISPLAYPAGETIMEMAX) ? V_DISPLAYPAGETIMEMAX : pgtime;
  ControllerData->set_display_pagetime(pgtime);
  ControllerData->set_maxstep((ControllerData->get_maxstep() < FOCUSERLOWERLIMIT) ? FOCUSERLOWERLIMIT : ControllerData->get_maxstep());
  ControllerData->set_stepsize((float)(ControllerData->get_stepsize() < 0.0) ? 0 : ControllerData->get_stepsize());
  ControllerData->set_stepsize((float)(ControllerData->get_stepsize() > MAXIMUMSTEPSIZE) ? MAXIMUMSTEPSIZE : ControllerData->get_stepsize());

  // Set coilpower
  boot_msg_print(T_COILPOWER);
  if (ControllerData->get_coilpower_enable() == V_NOTENABLED) {
    boot_msg_println(T_OFF);
    driverboard->releasemotor();
  } else {
    boot_msg_println(T_ON);
    driverboard->enablemotor();
  }

  // ensure driverboard position is same as setupData
  // set focuser position in DriverBoard
  driverboard->setposition(ControllerData->get_fposition());


  //-------------------------------------------------
  // SETUP IN-OUT LEDS
  // Included by default
  // Default state:  NotEnabled: Stopped
  //-------------------------------------------------
  // Now part of DriverBoard, and initialised/enabled/stopped there


  //-------------------------------------------------
  // SETUP PUSHBUTTONS
  // active high when pressed
  // Included by default
  // Default state:  NotEnabled: Stopped
  //-------------------------------------------------
  // Now part of DriverBoard, and initialised/enabled/stopped there


  //-------------------------------------------------
  // SETUP JOYSTICKS
  // Included by default
  // Default state:  NotEnabled: Stopped
  //-------------------------------------------------
  // Now part of DriverBoard, and initialised/enabled/stopped there


  //-------------------------------------------------
  // I2C INTERFACE START
  //-------------------------------------------------
  boot_msg_print(T_START);
  boot_msg_println(T_I2C);
  // pins defined in controller_defines.h
  if (myboardnumber == PRO2ESP32LOLINS2MINI) {
    Wire.begin(PRO2ESP32LOLINS2MINI_I2CDATA, PRO2ESP32LOLINS2MINI_I2CLOCK);
  } else {
    Wire.begin(I2CDATAPIN, I2CCLKPIN);
  }


  //-------------------------------------------------
  // DISPLAY
  // Manage via Management Server
  // Default state:  NotEnabled: Stopped
  //-------------------------------------------------
  if (display_init() == true) {
    boot_msg_print(T_START);
    boot_msg_println(T_DISPLAY);
    if (display_start() == false) {
      boot_msg_println(T_ERROR);
    }
  }


  //-------------------------------------------------
  // SETUP INFRA-RED REMOTE
  // Optional, use helper function
  // Default state:  NotEnabled: Stopped
  //-------------------------------------------------
  if (irremote_init() == true) {
    boot_msg_print(T_START);
    boot_msg_println(T_IRREMOTE);
    irremote_status = irremote_start();
    if (irremote_status == false) {
      boot_msg_println(T_ERROR);
    }
  }


  //-------------------------------------------------
  // SETUP DUCKDNS
  // Dependancy: WiFi has to be up and running before starting service
  // Optional, use helper function
  // Default state:  NotEnabled: Stopped
  //-------------------------------------------------
  // check if duckdns is loaded at boot time
  if (ControllerData->get_duckdns_enable() == V_ENABLED) {
    boot_msg_print(T_START);
    boot_msg_println(T_DUCKDNS);
    duckdns_status = duckdns_start();
    if (duckdns_status != V_RUNNING) {
      boot_msg_println(T_ERROR);
    }
  }


  //-------------------------------------------------
  // SETUP TEMPERATURE PROBE
  // Included by default
  // Manage via Management Server
  // Default state:  NotEnabled: Stopped
  //-------------------------------------------------
  // create the pointer to the class
  tempprobe = new TEMP_PROBE(ControllerData->get_brdtemppin());

  // initialise the probe
  if (tempprobe->init() == true) {
    if (ControllerData->get_tempprobe_enable() == V_ENABLED) {
      // probe is enabled, so start the probe
      boot_msg_print(T_START);
      boot_msg_println(T_TEMPPROBE);
      if (tempprobe->start() == true) {
        update_temp_flag = 0;
        temp = tempprobe->read();
      } else {
        boot_msg_println(T_ERROR);
      }
    }
  }


  //-------------------------------------------------
  // ASCOM ALPACA SERVER START
  // Dependancy: WiFi must be running before starting server
  // Manage via Management Server
  // Default state:  NotEnabled: Stopped
  //-------------------------------------------------
  ascomsrvr = new ASCOM_SERVER();
  // check if ascom server is to be started at boot time
  if (ControllerData->get_ascomsrvr_enable() == V_ENABLED) {
    boot_msg_print(T_START);
    boot_msg_println(T_ALPACA);
    ascomsrvr_status = ascomsrvr->start();
    if (ascomsrvr_status != V_RUNNING) {
      boot_msg_println(T_ERROR);
    }
  }


  //-------------------------------------------------
  // MANAGEMENT SERVER START
  // Dependancy: WiFi has to be up and running before starting
  // Default state:  Enabled: Started
  //-------------------------------------------------
  // create pointer to class
  mngsrvr = new MANAGEMENT_SERVER();
  // check if management server is to be started at boot time
  if (ControllerData->get_mngsrvr_enable() == V_ENABLED) {
    boot_msg_print(T_START);
    boot_msg_println(T_MANAGEMENTSERVER);
    mngsrvr_status = mngsrvr->start(ControllerData->get_mngsrvr_port());
    if (mngsrvr_status != V_RUNNING) {
      boot_msg_println(T_ERROR);
    }
  }


  //-------------------------------------------------
  // OTA UPDATES
  // Dependancy: WiFi and Management Server
  // Optional, managed by Management Server
  // Default state:  NotEnabled: Stopped
  //-------------------------------------------------
  // OTA if enabled, will be started by Management Server


  //-------------------------------------------------
  // TCP/IP SERVER START
  // Dependancy: WiFi has to be up and running before starting TCP/IP server
  // Default state:  Enabled: Started
  //-------------------------------------------------
  // create pointer to class
  tcpipsrvr = new TCPIP_SERVER();
  // check if tcpip server is to be started at boot time
  if (ControllerData->get_tcpipsrvr_enable() == V_ENABLED) {
    boot_msg_print(T_START);
    boot_msg_println(T_TCPIPSERVER);
    tcpipsrvr_status = tcpipsrvr->start(ControllerData->get_tcpipsrvr_port());
    if (tcpipsrvr_status != V_RUNNING) {
      boot_msg_println(T_ERROR);
    }
  }


  //-------------------------------------------------
  // WEBSERVER START
  // Dependancy: WiFi has to be up and running before starting Webserver service
  // Default state:  NotEnabled: Stopped
  //-------------------------------------------------
  // create pointer to class
  websrvr = new WEB_SERVER();
  // check if web server is to be started at boot time
  if (ControllerData->get_websrvr_enable() == V_ENABLED) {
    boot_msg_print(T_START);
    boot_msg_println(T_WEBSERVER);
    websrvr_status = websrvr->start(ControllerData->get_websrvr_port());
    if (websrvr_status != true) {
      boot_msg_println(T_ERROR);
    }
  }


  //-------------------------------------------------
  // TASK TIMER START
  // Should be the last to start
  //-------------------------------------------------
  boot_msg_print(T_START);
  boot_msg_println(T_TASKTIMER);
  init_task_timer();


  //-------------------------------------------------
  // WATCH DOG TIMER START
  // enable panic reboot, dump registers, serial output - exception decoder
  //-------------------------------------------------
  boot_msg_print(T_START);
  boot_msg_println(T_WATCHDOGTIMER);
  esp_task_wdt_init(WDT_TIMEOUT, true);
  esp_task_wdt_add(NULL);

  // Try to prevent WiFi power save mode
  WiFi.setSleep(false);

  // works with ESP32-C3-Super-Mini
  //digitalWrite(LED_BUILTIN, HIGH);  //MN 08042024

  //MN 08042024
  //MN this lights up the ESP32-S3-Zero builtin LED
  leds[0] = CRGB::Blue; 
  FastLED.setBrightness(16);
  FastLED.show();
  //MN 08042024


  reboot_start = false;
  boot_msg_println(T_READY);
}


// ----------------------------------------------------------------------
// void check_options();
// handle all device checks and updates
// ----------------------------------------------------------------------
void check_options() {
  static Option_States OptionState = Option_pushbtn_joystick;
  // mutex for option states
  static int o_mux;

  // Option state engine
  switch (OptionState) {
    case Option_pushbtn_joystick:
      // these are mutually exclusive, so use if else
      if (driverboard->get_pushbuttons_loaded() == true) {
        driverboard->update_pushbuttons();
      } else if (driverboard->get_joystick1_loaded() == true) {
        driverboard->update_joystick1();
      } else if (driverboard->get_joystick2_loaded() == true) {
        driverboard->update_joystick2();
      }
      OptionState = Option_IRRemote;
      break;

    case Option_IRRemote:
      // use helpers because optional
      irremote_update();
      OptionState = Option_Display;
      break;

    case Option_Display:
      if (display_status == V_RUNNING) {
        // display is optional so need to use helper
        portENTER_CRITICAL(&displayMux);
        o_mux = update_display_flag;
        portEXIT_CRITICAL(&displayMux);
        if (o_mux == 1) {
          portENTER_CRITICAL(&displayMux);
          update_display_flag = 0;
          portEXIT_CRITICAL(&displayMux);
          // use helper
          display_update();
        }
      }
      OptionState = Option_Temperature;
      break;

    case Option_Temperature:
      if (tempprobe->get_state() == V_RUNNING) {
        portENTER_CRITICAL(&tempMux);
        o_mux = update_temp_flag;
        portEXIT_CRITICAL(&tempMux);
        if (o_mux == 1) {
          portENTER_CRITICAL(&tempMux);
          update_temp_flag = 0;
          portEXIT_CRITICAL(&tempMux);
          // read temp AND check Temperature Compensation
          temp = tempprobe->update();
        }
      }
      OptionState = Option_WiFi;
      break;

  //MN 08042024
    case Option_WiFi:
      // wifi connect check
      portENTER_CRITICAL(&wifiMux);
      o_mux = update_wifi_flag;
      portEXIT_CRITICAL(&wifiMux);
      if (o_mux == 1) {
        if (myfp2esp32mode == STATION) {
          if (WiFi.status() != WL_CONNECTED) {
            debug_server_println(T_RECONNECTWIFI);
            WiFi.disconnect();
           //MN 08042024
           leds[0] = CRGB::Red; 
           FastLED.setBrightness(16);
           FastLED.show();
           //MN 08042024
            WiFi.reconnect();
          }
        }
        update_wifi_flag = 0;
      }
      OptionState = Option_pushbtn_joystick;
      break;

    default:
      OptionState = Option_pushbtn_joystick;
      break;
  }
}


void loop() {
  static Focuser_States FocuserState = State_Idle;
  static uint32_t backlash_count = 0;
  static bool DirOfTravel = (bool)ControllerData->get_focuserdirection();
  // focuser park status
  static bool Parked = true;
  static uint32_t TimeStampdelayaftermove = 0;
  // timersemaphore, used by movetimer
  static bool tms = false;
  static uint8_t updatecount = 0;
  static uint32_t steps = 0;
  static uint32_t damcounter = 0;
  // mutex for focuser states
  static int t_mux;
  // used in finding Home Position Switch
  static int stepstaken = 0;
  static bool hpswstate = false;

  esp_task_wdt_reset();

  // handle all the server loop checks, for new client or client requests

  // check ASCOM server for new clients
  ascomsrvr->loop();

  // check management server for new clients
  mngsrvr->loop(Parked);

  // check TCP/IP Server for new clients
  tcpipsrvr->loop(Parked);

  // check Web Server for new clients
  websrvr->loop(Parked);

  // Check Debug Server for client connections and requests
  if (debugsrvr_status == V_RUNNING) {
    debugsrvr->check_client();
  }

  check_options();

  // Focuser state engine
  switch (FocuserState) {
    case State_Idle:
      if (driverboard->getposition() != ftargetPosition) {
        // prepare to move focuser
        Parked = false;
        oled_state = oled_on;
        isMoving = true;
        driverboard->enablemotor();
        FocuserState = State_InitMove;
        debug_server_println(T_INITMOVE);
        debug_server_print(T_FROM);
        debug_server_println(driverboard->getposition());
        debug_server_print(T_TO);
        debug_server_println(ftargetPosition);
      } else {
        // focuser stationary, isMoving is false
        isMoving = false;

        // focuser stationary. isMoving is 0
        if (ControllerData->SaveConfiguration(driverboard->getposition(), DirOfTravel)) {
          debug_server_println(T_CONFIGSAVED);
        }

        // park can be enabled or disabled (management server)
        // park controls coil power off and display off after elapsed 30s following a move
        // if park is enabled, 30s after a move ends, coilpower(if enabled) and display get turned off
        // if park is not enabled, state of coilpower and display are not altered

        // check if parking is enabled
        if (ControllerData->get_park_enable() == true) {
          // parking is enabled in ControllerData
          // state_delayaftermove sets Parked false and sets park flag to 0
          if (Parked == false) {
            // check parked flag state for 1 (means park time delay is expired)
            portENTER_CRITICAL(&parkMux);
            t_mux = update_park_flag;
            portEXIT_CRITICAL(&parkMux);
            if (t_mux == 1) {
              // 30s wait is over, disable park flag
              // set flag to -1 so task timer no longer counts this flag
              portENTER_CRITICAL(&parkMux);
              update_park_flag = -1;
              portEXIT_CRITICAL(&parkMux);
              // park focuser if parking is enabled
              Parked = true;
              debug_server_println(T_PARKED);

              // handle coil power
              // Coil Power Status ON  - Controller does move, coil power remains on
              // Coil Power Status OFF - Controller enables coil power, moves motor, after 30s elapsed releases power to motor
              if (ControllerData->get_coilpower_enable() == V_NOTENABLED) {
                driverboard->releasemotor();
              }
              // turn off display
              debug_server_print(T_DISPLAY);
              debug_server_println(T_OFF);
              oled_state = oled_off;
              // focuser is parked, coil power off, display off
            }
          }
        }
      }
      break;

    case State_InitMove:
      isMoving = true;
      backlash_count = 0;
      DirOfTravel = (ftargetPosition > driverboard->getposition()) ? moving_out : moving_in;
      driverboard->enablemotor();
      if (ControllerData->get_focuserdirection() != DirOfTravel) {
        ControllerData->set_focuserdirection(DirOfTravel);
        // move is in opposite direction
        if (DirOfTravel == moving_in) {
          // check for backlash-in enabled
          if (ControllerData->get_backlash_in_enable()) {
            // get backlash in steps
            backlash_count = ControllerData->get_backlashsteps_in();
          }
        } else {
          // check for backlash-out enabled
          if (ControllerData->get_backlash_out_enable()) {
            // get backlash out steps
            backlash_count = ControllerData->get_backlashsteps_out();
          }
        }  // if ( DirOfTravel == moving_in)

        // check for graphics display, screen output is different
        if (displaytype == Type_Graphic) {
          // Holgers code: This is for a graphics display
          if (DirOfTravel != moving_main && backlash_count) {
            uint32_t sm = ControllerData->get_brdstepmode();
            uint32_t bl = backlash_count * sm;
            //debug_server_print(T_BLSTEPS);
            //debug_server_println(bl);

            if (DirOfTravel == moving_out) {
              // Trip to tuning point should be a fullstep position
              backlash_count = bl + sm - ((ftargetPosition + bl) % sm);
            } else {
              // Trip to tuning point should be a fullstep position
              backlash_count = bl + sm + ((ftargetPosition - bl) % sm);
            }
            debug_server_print(T_BLCOUNT);
            debug_server_println(backlash_count);
          }  // if (DirOfTravel != moving_main && backlash_count)
        }    // if ( displaytype == Type_Graphic )
      }      // if (ControllerData->get_focuserdirection() != DirOfTravel)

      // calculate number of steps to move
      // if target pos > current pos then steps = target pos - current pos
      // if target pos < current pos then steps = current pos - target pos
      steps = (ftargetPosition > driverboard->getposition()) ? ftargetPosition - driverboard->getposition() : driverboard->getposition() - ftargetPosition;

      // Error - cannot combine backlash steps to steps because that alters position
      // Backlash move SHOULD NOT alter focuser position as focuser is not actually moving
      // backlash is taking up the slack in the stepper motor/focuser mechanism, so position is not actually changing
      if (backlash_count != 0) {
        FocuserState = State_Backlash;
      } else {
        // if target pos > current pos then steps = target pos - current pos
        // if target pos < current pos then steps = current pos - target pos
        driverboard->initmove(DirOfTravel, steps);
        boot_msg_print(T_STEPS);
        boot_msg_println(steps);
        boot_msg_println(T_GOMOVING);
        FocuserState = State_Moving;
      }
      break;

    case State_Backlash:
      boot_msg_print(T_STATEBL);
      boot_msg_println(backlash_count);
      while (backlash_count != 0) {
        // take 1 step and do not adjust position
        steppermotormove(DirOfTravel);
        // ensure delay between steps
        delayMicroseconds(ControllerData->get_brdmsdelay());
        backlash_count--;
        // check if home position sensor activated?
        if (driverboard->hpsw_alert()) {
          boot_msg_println("HPS_alert() in BL move");
          // backlash move interrupted by HPSW closure
          portENTER_CRITICAL(&timerSemaphoreMux);
          timerSemaphore = false;
          portEXIT_CRITICAL(&timerSemaphoreMux);
          // drop out of while loop
          backlash_count = 0;
          // change state to State_Moving and handle HPSW
          boot_msg_println(T_GOMOVING);
          FocuserState = State_Moving;
        }
      }
      if (FocuserState == State_Backlash) {
        boot_msg_println(T_BLDONE);
        boot_msg_print(T_MOVESTEPS);
        boot_msg_println(steps);
        driverboard->initmove(DirOfTravel, steps);
        boot_msg_println(T_GOMOVING);
        FocuserState = State_Moving;
      } else {
        // FocuserState is State_Moving - timerSemaphore is false. is then caught by if(driverboard->hpsw_alert() ) and HPSW is processed
      }
      break;

    case State_Moving:
      portENTER_CRITICAL(&timerSemaphoreMux);
      tms = timerSemaphore;
      portEXIT_CRITICAL(&timerSemaphoreMux);
      if (tms == true) {
        // move has completed, the driverboard keeps track of focuser position
        boot_msg_println(T_MOVEDONE);
        // disable interrupt timer that moves motor
        driverboard->end_move();
        boot_msg_println(T_GODELAYAFTERMOVE);
        // cannot use task timer for delayaftermove, as delayaftermove can be less than 100ms
        // task timer minimum time slice is 100ms, so use timestamp instead
        TimeStampdelayaftermove = millis();
        FocuserState = State_DelayAfterMove;
      } else {
        // still moving - timer semaphore is false
        // check for halt_alert which is set by tcpip_server or web_server
        if (halt_alert) {
          boot_msg_println(T_HALTALERT);
          // reset halt_alert flag
          portENTER_CRITICAL(&halt_alertMux);
          halt_alert = false;
          portEXIT_CRITICAL(&halt_alertMux);
          // disable interrupt timer that moves motor
          driverboard->end_move();
          // check for < 0
          if (driverboard->getposition() < 0) {
            driverboard->setposition(0);
          }
          ftargetPosition = driverboard->getposition();
          ControllerData->set_fposition(driverboard->getposition());

          // we no longer need to keep track of steps here or halt because driverboard updates position on every move
          // handle delayaftermove using TimeCheck
          TimeStampdelayaftermove = millis();
          FocuserState = State_DelayAfterMove;
        }  // if ( halt_alert )

        // check for home postion switch
        if (driverboard->hpsw_alert()) {
          // hpsw is activated
          // disable interrupt timer that moves motor
          driverboard->end_move();
          if (ControllerData->get_hpswmsg_enable() == V_ENABLED) {
            boot_msg_print(T_HPSWALERT);
          }
          ftargetPosition = 0;
          driverboard->setposition(0);
          ControllerData->set_fposition(0);
          // check if display home position messages is enabled
          if (ControllerData->get_hpswmsg_enable() == V_ENABLED) {
            boot_msg_println(T_HPSWMSG1);
          }
          if (ControllerData->get_brdnumber() == PRO2ESP32TMC2209 || ControllerData->get_brdnumber() == PRO2ESP32TMC2209P) {
#if defined(USE_STALL_GUARD)
            // focuser is at home position, no need to handle set position, simple
            if (ControllerData->get_hpswmsg_enable() == V_ENABLED) {
              boot_msg_println(T_HPSWMSG2);
            }
            TimeStampdelayaftermove = millis();
            boot_msg_println(T_GODELAYAFTERMOVE);
            FocuserState = State_DelayAfterMove;
#else
            // not stall guard, must be a physical switch then we should jump to set home position
            if (ControllerData->get_hpswmsg_enable() == V_ENABLED) {
              boot_msg_println(T_GOSETHOMEPOSITION);
            }
            FocuserState = State_SetHomePosition;
#endif  // #if defined(USE_STALL_GUARD)
          } else {
            // not a tmc2209 board
            boot_msg_println(T_GOSETHOMEPOSITION);
            FocuserState = State_SetHomePosition;
          }  // if ( ControllerData->get_brdnumber() == PRO2ESP32TMC2209 || ControllerData->get_brdnumber() == PRO2ESP32TMC2209P )
        }    // if (driverboard->hpsw_alert() )

        // check for < 0
        if (driverboard->getposition() < 0) {
          portENTER_CRITICAL(&halt_alertMux);
          halt_alert = true;
          portEXIT_CRITICAL(&halt_alertMux);
        }

        // if the update position on display when moving is enabled, then update the display
        updatecount++;
        // update every 15th move to avoid overhead
        if (updatecount > DISPLAYUPDATEONMOVE) {
          updatecount = 0;
          // use helper
          display_update_position(driverboard->getposition());
        }
      }
      break;

    case State_SetHomePosition:
      // move out till home position switch opens
      if (ControllerData->get_hpswitch_enable() == V_ENABLED) {
        // check if display home position switch messages is enabled
        if (ControllerData->get_hpswmsg_enable() == V_ENABLED) {
          boot_msg_println(T_HPSWMSG3);
        }
        // HOME POSITION SWITCH IS CLOSED - Step out till switch opens then set position = 0
        stepstaken = 0;                  // Count number of steps to prevent going too far
        DirOfTravel = !DirOfTravel;      // We were going in, now we need to reverse and go out
        hpswstate = HPSWCLOSED;          // We know we got here because switch was closed
        while (hpswstate == HPSWCLOSED)  // while hpsw = closed = true = 1
        {
          // take 1 step
          if (ControllerData->get_reverse_enable() == V_NOTENABLED) {
            steppermotormove(DirOfTravel);
          } else {
            steppermotormove(!DirOfTravel);
          }

          delayMicroseconds(ControllerData->get_brdmsdelay());

          stepstaken++;
          // prevent an endless loop if the hpsw is not connected or is faulty
          if (stepstaken > HOMESTEPS) {
            if (ControllerData->get_hpswmsg_enable() == V_ENABLED) {
              boot_msg_println(T_HPSWMSG4);
            }
            hpswstate = HPSWOPEN;
          } else {
            // hpsw_alert returns true if closed, false = open
            hpswstate = driverboard->hpsw_alert();
          }
        }
        if (ControllerData->get_hpswmsg_enable() == V_ENABLED) {
          boot_msg_print(T_HPSWMSG5);
          boot_msg_println(stepstaken);
          boot_msg_println(T_HPSWMSG6);
        }
        ftargetPosition = 0;
        driverboard->setposition(0);
        ControllerData->set_fposition(0);
        // set direction of last move
        ControllerData->set_focuserdirection(DirOfTravel);
        if (ControllerData->get_hpswmsg_enable() == V_ENABLED) {
          boot_msg_println(T_HPSWMSG6);
        }
      }  //  if( ControllerData->get_homepositionswitch() == V_ENABLED)
      TimeStampdelayaftermove = millis();
      FocuserState = State_DelayAfterMove;
      boot_msg_println(T_GODELAYAFTERMOVE);
      break;

    case State_DelayAfterMove:
      // apply Delayaftermove, this MUST be done here in order to get accurate timing for delayaftermove
      // the task timer runs on 100ms slices, so cannot be used to control delayaftermove, this is why
      // a timecheck is used instead
      if (ControllerData->get_delayaftermove_enable() == V_ENABLED) {
        if (TimeCheck(TimeStampdelayaftermove, ControllerData->get_delayaftermove_time())) {
          damcounter = 0;
          boot_msg_println(T_GOENDMOVE);
          FocuserState = State_EndMove;
        }
        // keep looping around till timecheck for delayaftermove succeeds
        // BUT ensure there is a way to exit state if delayaftermove fails to timeout
        // a loop cycle is 1-4ms
        damcounter++;
        if (damcounter > 255) {
          damcounter = 0;
          boot_msg_println(T_GOENDMOVE);
          FocuserState = State_EndMove;
        }
      } else {
        // delay after move is disabled, so go to next focuser state
        boot_msg_println(T_GOENDMOVE);
        FocuserState = State_EndMove;
      }
      break;

    case State_EndMove:
      isMoving = false;
      // is parking enabled in controller?
      if (ControllerData->get_park_enable() == true) {
        boot_msg_println(T_ENDMOVE);
        portENTER_CRITICAL(&parkMux);
        update_park_flag = 0;
        portEXIT_CRITICAL(&parkMux);
      }
      FocuserState = State_Idle;
      break;

    default:
      FocuserState = State_Idle;
      break;
  }
}  // end Loop()
