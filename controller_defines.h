// ----------------------------------------------------------------------
// myFP2ESP32 GENERAL DEFINITIONS
// Copyright Robert Brown 2014-2023. All Rights Reserved.
// Copyright Holger M, 2019-2021. All Rights Reserved.
// controller_defines.h
// ----------------------------------------------------------------------

#ifndef _controller_defines_h
#define _controller_defines_h

#include <Arduino.h>


// ----------------------------------------------------------------------
// DO NOT CHANGE
// ----------------------------------------------------------------------

// -----------------------------------------------------------------------
// DEBUGGING
// CAUTION: DO NOT ENABLE DEBUGGING INFORMATION
// -----------------------------------------------------------------------
// Remove comment to enable boot messages to be written to Serial port
//#define BOOT_PRINT 1

#ifdef BOOT_PRINT
#define boot_msg_print(...) Serial.print(__VA_ARGS__)
#define boot_msg_println(...) Serial.println(__VA_ARGS__)
#else
#define boot_msg_print(...)
#define boot_msg_println(...)
#endif


// Debug Server externs
// These are helper functions in the .ino file
extern void debug_server_print(const char *);
extern void debug_server_print(char);
extern void debug_server_print(int);
extern void debug_server_print(unsigned int);
extern void debug_server_print(long);
extern void debug_server_print(unsigned long);
extern void debug_server_print(double);
extern void debug_server_print(String);
extern void debug_server_println(const char *);
extern void debug_server_println(char);
extern void debug_server_println(const int);
extern void debug_server_println(unsigned int);
extern void debug_server_println(long);
extern void debug_server_println(unsigned long);
extern void debug_server_println(double);
extern void debug_server_println(String);


// -----------------------------------------------------------------------
// DEFINITIONS: DO NOT CHANGE
// -----------------------------------------------------------------------
enum Oled_States { oled_off,
                   oled_on };

enum Focuser_States { State_Idle,
                      State_InitMove,
                      State_Backlash,
                      State_Moving,
                      State_FinishedMove,
                      State_SetHomePosition,
                      State_DelayAfterMove,
                      State_EndMove };

enum Option_States { Option_pushbtn_joystick,
                     Option_IRRemote,
                     Option_Display,
                     Option_Temperature,
                     Option_WiFi };

enum Display_Types { Type_None,
                     Type_Text,
                     Type_Graphic };

enum tmc2209stallguard { Use_Stallguard,
                         Use_Physical_Switch,
                         Use_None };

// controller modes
#define ACCESSPOINT 1
#define STATION 2

// STATIC AND DYNAMIC CONTROLLER TCP/IP ADDRESS - Only valid for STATIONMODE
#define DYNAMICIP 1
#define STATICIP 2

// WIFI CONFIG TYPES
#define READWIFICONFIG 1
#define MULTIAP 2

// Values
#define V_STOPPED 0
#define V_RUNNING 1
#define V_NOTENABLED 0
#define V_ENABLED 1
#define V_FAHRENHEIT 0
#define V_CELSIUS 1

// debug server
#define IPADDRESSLEN 16

// I2C
#define I2CDATAPIN 3  //MN ESP32-S3 Zero
#define I2CCLKPIN  4 //MN
#define PRO2ESP32LOLINS2MINI_I2CDATA 8
#define PRO2ESP32LOLINS2MINI_I2CLOCK 9
// MoveTimer (driver_board.cpp), default to timer1
// Task Timer, uses timer2
// IRRemote timer; in class, uses Timer 3

// undef

#undef DRVBRD
#undef FIXEDSTEPMODE
#undef STEPSPERREVOLUTION
#undef CONTROLLERMODE
#undef IPADDRESSMODE
#undef WIFICONFIGTYPE
#undef ENABLE_TEXTDISPLAY
#undef ENABLE_GRAPHICDISPLAY
#undef ENABLE_INFRAREDREMOTE
#undef ENABLE_ELEGANTOTA
#undef ENABLE_DUCKDNS

// Watch dog timer
#define WDT_TIMEOUT 30  // in seconds

// DO NOT CHANGE
#define REBOOTDELAY 2000  // wait (2s) before performing reboot
#define moving_in false
#define moving_out !moving_in
#define moving_main moving_in
// TEMPERATURE COMPENSATION
#define TC_DIRECTION_IN 0
#define TC_DIRECTION_OUT 1

// PORTS
#define ASCOMSERVERPORT 4040      // ASCOM Remote port
#define ASCOMDISCOVERYPORT 32227  // UDP
#define DEBUGSERVERPORT 9090
#define MNGSERVERPORT 6060    // Management interface - should not be changed
#define TCPIPSERVERPORT 2020  // TCPIP Server port for myFP2ESP32
#define WEBSERVERPORT 80      // Web server port

// Debug server output
#define DEBUGSERVEROUTPUTSERIAL 0
#define DEBUGSERVEROUTPUTPORT 1

// MOTOR SETTINGS
#define DEFAULTSTEPSIZE 50.0  // This is the default setting for the step size in microns
#define MINIMUMSTEPSIZE 0.0
#define MAXIMUMSTEPSIZE 100.0
#define DEFAULTMAXSTEPS 84000L     // Celestron 127WiFi 42Rev's = total Focus-Range        //MN
                                   // CYFANG 12GA-15BY Geerboxmotor -> 2000 steps per Rev. //MN
#define DEFAULTPOSITION 42000L

#define FOCUSERUPPERLIMIT 500000L  // arbitary focuser limit up to 500,000 steps
#define FOCUSERLOWERLIMIT 1024L    // lowest value that maxsteps can be
#define HOMESTEPS 200              // Prevent searching for home position switch never returning, 
                                   // this should be > than # of steps between closed and open
#define HPSWOPEN 0                 // hpsw states refelect status of switch
#define HPSWCLOSED 1
#define LEDPULSE 0
#define LEDMOVE 1
#define PUSHBUTTON_STEPS 1

// Debug server output
#define DEBUGSERVEROUTPUTSERIAL 0
#define DEBUGSERVEROUTPUTPORT 1

// DISPLAY
#define OLED_ADDR 0x3C           // some displays maybe at 0x3D or 0x3F, use I2Cscanner to find the correct address
#define V_DISPLAYPAGETIMEMIN 2   // 2s minimum oled page display time
#define V_DISPLAYPAGETIMEMAX 10  // 10s maximum oled page display time

#define DISPLAYUPDATEONMOVE 15  // number of steps before refreshing position when moving if oledupdateonmove is 1
#define V_DISPLAYPAGETIMEDEFAULT 4

// DUCKDNS SERVICE
#define DUCKDNS_REFRESHRATE 120  // duck dns, check ip address every 2 minutes for an update

// SERIAL PORT
#define SERIALPORTSPEED 115200  // 9600, 14400, 19200, 28800, 38400, 57600, 115200

// TEMPERATURE PROBE
//#define DEFAULTTEMPREFRESHTIME 30  // refresh rate between temperature conversions - 30 timeslices = 3s
#define DEFAULTTEMPRESOLUTION 10  // Set the default DS18B20 resolution to 0.25 of a degree 9=0.5, 10=0.25, 11=0.125, 12=0.0625

// DELAY TIME BEFORE CHANGES ARE WRITTEN TO SPIFFS FILE
#define DEFAULTSAVETIME 600  // 600 timeslices, 10 timeslices per second = 600 / 10 = 60 seconds

// DEFAULT PARK TIME (Can be changed in Management Server)
#define DEFAULTPARKTIME 120  // 30-300s

// defines for ASCOMSERVER, WEBSERVER
#define NORMALWEBPAGE 200
//#define FILEUPLOADSUCCESS 300
#define BADREQUESTWEBPAGE 400
#define NOTFOUNDWEBPAGE 404
#define INTERNALSERVERERROR 500

#define LISTSHORT 1
#define LISTLONG 2


// ----------------------------------------------------------------------
// DO NOT CHANGE
// ----------------------------------------------------------------------
extern const char *DEFAULTTITLECOLOR;
extern const char *DEFAULTSUBTITLECOLOR;
extern const char *DEFAULTHEADERCOLOR;
extern const char *DEFAULTTEXTCOLLOR;
extern const char *DEFAULTBACKCOLOR;

extern const char *project_name;
extern const char *program_version;
extern const char *program_author;

extern const char *TEXTPAGETYPE;
extern const char *PLAINTEXTPAGETYPE;
extern const char *JSONTEXTPAGETYPE;
extern const char *JSONPAGETYPE;

extern const char *H_FILENOTFOUNDSTR;
extern const char *H_FSNOTLOADEDSTR;

extern const char *T_ACCESSPOINT;
extern const char *T_STATION;
extern const char *T_OFF;
extern const char *T_ON;
extern const char *T_DISABLED;
extern const char *T_ENABLED;
extern const char *T_CELSIUS;
extern const char *T_FAHRENHEIT;
extern const char *T_FOUND;
extern const char *T_NOTFOUND;
extern const char *T_NO;
extern const char *T_YES;
extern const char *T_START;
extern const char *T_STOP;
extern const char *T_STOPPED;
extern const char *T_RUNNING;
extern const char *T_IN;
extern const char *T_OUT;
extern const char *T_OK;
extern const char *T_NOTOK;
extern const char *T_ERROR;
extern const char *T_INIT;
extern const char *T_PARKED;
extern const char *T_NOTPARKED;
extern const char *T_DESERIALISEERROR;
extern const char *T_LOAD;
extern const char *T_LOADED;
extern const char *T_NOTLOADED;
extern const char *T_SAVED;
extern const char *T_OPENERROR;
extern const char *T_OPENFILE;
extern const char *T_ERRORFILESYSTEM;
extern const char *T_DELETE;
extern const char *T_NOTENABLED;
extern const char *T_NOTSUPPORTED;
extern const char *T_CONNECTED;
extern const char *T_ERRSTART;
extern const char *T_ERRNOTENABLED;

extern const char *T_SERIAL;
extern const char *T_I2C;
extern const char *T_HPSW;
extern const char *T_CONTROLLERDATA;
extern const char *T_DRIVERBOARD;
extern const char *T_ALPACA;
extern const char *T_DISCOVERY;
extern const char *T_DISPLAY;
extern const char *T_TCPIPSERVER;
extern const char *T_WEBSERVER;
extern const char *T_DEBUGSERVER;
extern const char *T_OTA;
extern const char *T_TEMPPROBE;
extern const char *T_DUCKDNS;
extern const char *T_TASKTIMER;
extern const char *T_MANAGEMENTSERVER;
extern const char *T_READWIFICONFIG;
extern const char *T_MULTIAP;
extern const char *T_STATICIP;
extern const char *T_DYNAMICIP;
extern const char *T_SSID;
extern const char *T_PWD;
extern const char *T_ALTCREDENTIALS;
extern const char *T_IPADDRESS;
extern const char *T_HOSTNAME;
extern const char *T_SYSTEMUPTIME;
extern const char *T_COILPOWER;
extern const char *T_IRREMOTE;
extern const char *T_WATCHDOGTIMER;
extern const char *T_READY;
extern const char *T_MOVING;
extern const char *T_MOVETO;
extern const char *T_TARGET;
extern const char *T_GOTO;
extern const char *T_TRUE;
extern const char *T_FALSE;

#endif  // _controller_defines_h
