// ----------------------------------------------------------------------
// myFP2ESP32 MANAGEMENT SERVER ROUTINES
// Copyright Robert Brown 2014-2023. All Rights Reserved.
// Copyright Holger M, 2019-2021. All Rights Reserved.
// management_server.cpp
// Default Configuration
// ----------------------------------------------------------------------


// ----------------------------------------------------------------------
// Includes
// ----------------------------------------------------------------------
#include <Arduino.h>
// includes boarddefs.h and controller_defines.h
#include "controller_config.h"
// Benoit Blanchon https://github.com/bblanchon/ArduinoJson
#include <ArduinoJson.h>
#include "SPIFFS.h"
#include <WebServer.h>


// ---------------------------------------------------------------------
// INCLUDES AND EXTERNS
// ---------------------------------------------------------------------

// Controller
#include "controller_data.h"
extern CONTROLLER_DATA *ControllerData;

// ASCOM ALPACA Server
#include "ascom_server.h"
extern ASCOM_SERVER *ascomsrvr;

// Debug Server
extern bool start_debugserver(void);
extern void stop_debugserver(void);

// Driver Board
#include "driver_board.h"
extern DRIVER_BOARD *driverboard;
extern int myboardnumber;
extern int myfixedstepmode;
extern int mystepsperrev;
#include "defines/tmcstepper_defines.h"

// Elegant OTA
extern const char *OTAName;
extern const char *OTAPassword;
#if defined(ENABLE_ELEGANTOTA)
#include <ElegantOTA.h>
#endif

// TCPIP Server
#include "tcpip_server.h"
extern TCPIP_SERVER *tcpipsrvr;

// Temperature probe
#include "temp_probe.h"
extern TEMP_PROBE *tempprobe;
extern float temp;

// Web Server
#include "web_server.h"
extern WEB_SERVER *websrvr;

// Management Server defines
extern const char *admin_username;
extern const char *admin_password;
#include "defines/management_defines.h"
#include "management_server.h"
extern MANAGEMENT_SERVER *mngsrvr;


// ----------------------------------------------------------------------
// DISPLAY IS OPTIONAL : USE HELPER FUNCTIONS
// ----------------------------------------------------------------------
extern bool display_start(void);
extern void display_stop();
extern void display_clear();
extern byte display_status;


// ----------------------------------------------------------------------
// DUCKDNS IS OPTIONAL : USE HELPER FUNCTIONS
// ----------------------------------------------------------------------
// duckdns cannot be stopped, there is only a start


// ----------------------------------------------------------------------
// EXTERNS
// ----------------------------------------------------------------------
extern void reboot_esp32(int);
extern long getrssi(void);
// system uptime days:hours:minutes
extern void get_systemuptime(void);

// cached vars
extern char devicename[32];
extern char titlecolor[8];
extern char subtitlecolor[8];
extern char headercolor[8];
extern char textcolor[8];
extern char backcolor[8];

//extern char ipStr[];
//extern char mySSID[];
extern char systemuptime[12];
extern int myfp2esp32mode;
extern long ftargetPosition;
extern bool isMoving;

//extern bool filesystemloaded;
// 120s subject to ControllerData->get_parktime()
extern volatile unsigned int park_maxcount;
// 4s   subject to ControllerData->get_display_pagetime(), default 4 seconds
extern volatile unsigned int display_maxcount;
extern portMUX_TYPE displaytimeMux;
extern portMUX_TYPE parkMux;

// Service states
extern byte duckdns_status;
extern byte irremote_status;
extern byte ota_status;
extern byte ascomsrvr_status;
extern byte tcpipsrvr_status;
extern byte websrvr_status;
extern bool debugsrvr_status;

extern volatile bool halt_alert;
extern portMUX_TYPE halt_alertMux;

// extern bool joystick_state;

// ----------------------------------------------------------------------
// MANAGEMENT SERVER DEFINITIONS
// ----------------------------------------------------------------------
// Page refresh time following a Management service reboot page time (s) between next page refresh
#define MAXSIZECUSTOMBRD 300
// temporary page when reboot occurs
#define RebootStr "<html><meta http-equiv=refresh content=15; url=/\"><head><title>Management Server></title></head><body><p>Please wait, controller rebooting.</p></body></html>"
// reboot button, each admin page
#define H_CREBOOT "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"cntlr_reboot\" value=\"true\"><input type=\"submit\" onclick=\"return confirm('Are you sure?')\" value=\"REBOOT\"></form>"

// ADMIN 1
// ASCOM Server
#define H_ENABLEASCOMSRVR "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"ascome\" value=\"enable\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"ENABLE\"></form>"
#define H_DISABLEASCOMSRVR "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"ascome\" value=\"disable\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"DISABLE\"></form>"
#define H_STARTASCOMSRVR "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"ascoms\" value=\"start\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"START\"></form>"
#define H_STOPASCOMSRVR "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"ascoms\" value=\"stop\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"STOP\"></form>"
#define H_ASCOMSRVRPORT "<form action=\"/admin1\" method =\"post\"><input type=\"text\" name=\"aport\" size=\"6\" value=\"%ASE%\">"
#define H_ASCOMSRVRPORTBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setasport\" value=\"Set\"></form>"
// Debug Server Start/Stop
#define DSSTOP "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"stopds\" value=\"true\"><input type=\"submit\" style=\"height:1.6em; width:5.5em\" value=\"STOP\"></form>"
#define DSSTART "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"startds\" value=\"true\"><input type=\"submit\" style=\"height:1.6em; width:5.5em\" value=\"START\"></form>"
// Port
#define DEBUGPORTFORM "<form action=\"/admin1\" method=\"post\"><input type=\"text\" name=\"dbp\" style=\"height:1.4em; width:5.5em\" value=\"%dbp%\">"
#define DEBUGPORTBTN "<input type=\"submit\" name=\"setdbport\" style=\"height:1.6em; width:5.5em\" value=\"SET\"></form>"
// Output to Serial | Port
#define DEBUGWRITETOPORT "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"setport\" value=\"true\"><input type=\"submit\" style=\"height:1.6em; width:5.5em\" value=\"PORT\"></form>"
#define DEBUGWRITETOSERIAL "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"setserial\" value=\"true\"><input type=\"submit\" style=\"height:1.6em; width:5.5em\" value=\"SERIAL\"></form>"
// TCPIP Server
#define H_ENABLETCPIPSRVR "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"tcpe\" value=\"enable\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"ENABLE\"></form>"
#define H_DISABLETCPIPSRVR "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"tcpe\" value=\"disable\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"DISABLE\"></form>"
#define H_STARTTCPIPSRVR "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"tcps\" value=\"start\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"START\"></form>"
#define H_STOPTCPIPSRVR "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"tcps\" value=\"stop\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"STOP\"></form>"
#define H_TCPIPSRVRPORT "<form action=\"/admin1\" method =\"post\"><input type=\"text\" name=\"tport\" size=\"6\" value=\"%TCE%\">"
#define H_TCPIPSRVRPORTBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"settcport\" value=\"Set\"></form>"
// WEB Server
#define H_ENABLEWEBSRVR "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"webe\" value=\"enable\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"ENABLE\"></form>"
#define H_DISABLEWEBSRVR "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"webe\" value=\"disable\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"DISABLE\"></form>"
#define H_STARTWEBSRVR "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"webs\" value=\"start\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"START\"></form>"
#define H_STOPWEBSRVR "<form action=\"/admin1\" method=\"post\"><input type=\"hidden\" name=\"webs\" value=\"stop\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"STOP\"></form>"
#define H_WEBSRVRPORT "<form action=\"/admin1\" method =\"post\"><input type=\"text\" name=\"wport\" size=\"6\" value=\"%wspnum%\">"
#define H_WEBSRVRPORTBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setwsport\" value=\"Set\"></form>"

// ADMIN 2
// DUCKDNS
// domain
#define H_DUCKDNS_DOMAIN "<form action=\"/admin2\" method=\"post\"><input type=\"text\" name=\"ddomain\" size=\"30\" value=\"%ddom%\">"
#define H_DUCKDNS_DOMBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setddnsdom\" value=\"Set\"></form>"
// token
#define H_DUCKDNS_TOKEN "<form action=\"/admin2\" method=\"post\"><input type=\"text\" name=\"dtoken\" size=\"30\" value=\"%ddtok%\">"
#define H_DUCKDNS_TOKENBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setddnstok\" value=\"Set\"></form>"
// refresh time
#define H_DUCKDNSRTIME "<form action=\"/admin2\" method =\"post\"><input type=\"text\" name=\"ddnsr\" size=\"6\" value=\"%ddnst%\">"
#define H_DUCKDNSRTBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setduckt\" value=\"Set\"></form>"
// OTA Name
#define H_OTA_NAME "<form action=\"/admin2\" method=\"post\"><input type=\"text\" name=\"otaname\" size=\"15\" value=\"%otan%\">"
#define H_OTA_NAMEBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setotaname\" value=\"Set\"></form>"
// OTA Password
#define H_OTA_PASSWORD "<form action=\"/admin2\" method=\"post\"><input type=\"text\" name=\"otapwd\" size=\"15\" value=\"%otap%\">"
#define H_OTA_PASSWORDBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setotapwd\" value=\"Set\"></form>"
// OTA ID
#define H_OTAID "<form action=\"/admin2\" method=\"post\"><input type=\"text\" name=\"otaida\" size=\"15\" value=\"%otaid%\">"
#define H_OTA_IDBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setotaid\" value=\"Set\"></form>"

// ADMIN3
// POSITION
#define H_POS_VALUE "<form action=\"/admin3\" method=\"post\"><input type=\"text\" name=\"pos\" size=\"10\" value=\"%posval%\">"
#define H_POS_SETBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setpos\" value=\"Set\">"
// GOTO
#define H_POS_GOTOBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"gopos\" value=\"Goto\"></form>"
// MAXSTEPS
#define H_MAX_VALUE "<form action=\"/admin3\" method=\"post\"><input type=\"text\" name=\"max\" size=\"10\" value=\"%maxval%\">"
#define H_MAX_VALUEBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setmax\" value=\"Set\"></form>"
// Coil Power State %CPS% Enable/Disable %BCPS%
#define H_ENABLECP "<form action=\"/admin3\" method=\"post\"><input type=\"hidden\" name=\"cpst\" value=\"on\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DISABLECP "<form action=\"/admin3\" method=\"post\"><input type=\"hidden\" name=\"cpst\" value=\"off\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
// Delay After Move Enable/Disable
#define H_DAMENABLE "<form action=\"/admin3\" method=\"post\"><input type=\"hidden\" name=\"damen\" value=\"on\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DAMDISABLE "<form action=\"/admin3\" method=\"post\"><input type=\"hidden\" name=\"damen\" value=\"off\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
// Delay After Move Time Interval
#define H_DELAYAFTERMOVEVAL "<form action=\"/admin3\" method =\"post\"><input type=\"text\" name=\"damt\" size=\"6\" value=\"%damnum%\">"
#define H_DELAYAFTERMOVEBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setdamt\" value=\"Set\"></form>"
// Motor speed html to create radio buttons on web page
#define H_MS_FORM "<form action=\"/admin3\" method=\"post\"><input type=\"hidden\" name=\"msd\" value=\"true\">"
#define H_MS_FORM_BUTTON "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Set\"> </form>"
#define H_MSSLOWCHECKED "<input type=\"radio\" name=\"ms\" value=\"0\" Checked> S"
#define H_MSSLOWUNCHECKED "<input type=\"radio\" name=\"ms\" value=\"0\"> S"
#define H_MSMEDCHECKED "<input type=\"radio\" name=\"ms\" value=\"1\" Checked> M"
#define H_MSMEDUNCHECKED "<input type=\"radio\" name=\"ms\" value=\"1\"> M"
#define H_MSFASTCHECKED "<input type=\"radio\" name=\"ms\" value=\"2\" Checked> F"
#define H_MSFASTUNCHECKED "<input type=\"radio\" name=\"ms\" value=\"2\"> F"
// Motor Speed Delay
#define H_MOTORSPEEDDELAYVAL "<form action=\"/admin3\" method =\"post\"><input type=\"text\" name=\"msd\" size=\"6\" value=\"%msdnum%\">"
#define H_MOTORSPEEDDELAYBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setmsd\" value=\"Set\"></form>"
// Park State %PTS% Enable/Disable %BPTS%
#define H_ENABLEPARK "<form action=\"/admin3\" method=\"post\"><input type=\"hidden\" name=\"park\" value=\"on\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DISABLEPARK "<form action=\"/admin3\" method=\"post\"><input type=\"hidden\" name=\"park\" value=\"off\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
// Park Timeout State %PTT% Enable/Disable %BPTT%
#define H_TIMEOUTPARKVAL "<form action=\"/admin3\" method =\"post\"><input type=\"text\" name=\"pat\" size=\"6\" value=\"%panum%\">"
#define H_TIMEOUTPARKBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setpat\" value=\"Set\"></form>"
// REVERSE DIRECTION %RDS% Enable/Disable %BRDS%
#define H_ENABLERD "<form action=\"/admin3\" method=\"post\"><input type=\"hidden\" name=\"rdst\" value=\"on\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DISABLERD "<form action=\"/admin3\" method=\"post\"><input type=\"hidden\" name=\"rdst\" value=\"off\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
// STEPMODE
#define STEPMODEFRM "<form action=\"/admin3\" method=\"post\"><select name=\"stepm\">"
#define STEPMODE1Y "<option value=\"1\" selected>1</option>"
#define STEPMODE1N "<option value=\"1\">1</option>"
#define STEPMODE2Y "<option value=\"2\" selected>2</option>"
#define STEPMODE2N "<option value=\"2\">2</option>"
#define STEPMODE4Y "<option value=\"4\" selected>4</option>"
#define STEPMODE4N "<option value=\"4\">4</option>"
#define STEPMODE8Y "<option value=\"8\" selected>8</option>"
#define STEPMODE8N "<option value=\"8\">8</option>"
#define STEPMODE16Y "<option value=\"16\" selected>16</option>"
#define STEPMODE16N "<option value=\"16\">16</option>"
#define STEPMODE32Y "<option value=\"32\" selected>32</option>"
#define STEPMODE32N "<option value=\"32\">32</option>"
#define STEPMODE64Y "<option value=\"64\" selected>64</option>"
#define STEPMODE64N "<option value=\"64\">64</option>"
#define STEPMODE128Y "<option value=\"128\" selected>128</option>"
#define STEPMODE128N "<option value=\"128\">128</option>"
#define STEPMODE256Y "<option value=\"256\" selected>256</option>"
#define STEPMODE256N "<option value=\"256\">256</option>"
#define STEPMODEBTN "</select><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Set\"></form>"
// Step Size
// Enable
#define H_ENABLESTEPSZSTATE "<form action=\"/admin3\" method=\"post\"><input type=\"hidden\" name=\"sss\" value=\"on\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DISABLESTEPSZSTATE "<form action=\"/admin3\" method=\"post\"><input type=\"hidden\" name=\"sss\" value=\"off\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
// Stepsize
#define H_STEPSIZEVAL "<form action=\"/admin3\" method =\"post\"><input type=\"text\" name=\"ssv\" size=\"6\" value=\"%ssnum%\">"
#define H_STEPSIZEBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setss\" value=\"Set\"></form>"

// ADMIN 4
// Backlash In
#define H_ENABLEBLINSTATE "<form action=\"/admin4\" method=\"post\"><input type=\"hidden\" name=\"blinst\" value=\"on\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DISABLEBLINSTATE "<form action=\"/admin4\" method=\"post\"><input type=\"hidden\" name=\"blinst\" value=\"off\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
#define H_BLINSTEPS "<form action=\"/admin4\" method =\"post\"><input type=\"text\" name=\"blis\" size=\"6\" value=\"%blinum%\"> <input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setblis\" value=\"Set\"></form>"
// Backlash Out
#define H_ENABLEBLOUTSTATE "<form action=\"/admin4\" method=\"post\"><input type=\"hidden\" name=\"bloutst\" value=\"on\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DISABLEBLOUTSTATE "<form action=\"/admin4\" method=\"post\"><input type=\"hidden\" name=\"bloutst\" value=\"off\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
#define H_BLOUTSTEPS "<form action=\"/admin4\" method =\"post\"><input type=\"text\" name=\"blos\" size=\"6\" value=\"%blonum%\"> <input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setblos\" value=\"Set\"></form>"

// ADMIN5 HPSW
// Home Position State %HPS% Enable/Disable %BHPS%
#define H_ENABLEHPSW "<form action=\"/admin5\" method=\"post\"><input type=\"hidden\" name=\"hpswst\" value=\"on\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DISABLEHPSW "<form action=\"/admin5\" method=\"post\"><input type=\"hidden\" name=\"hpswst\" value=\"off\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
// Home Position Messages State %HPS% Enable/Disable %BHPS%
#define H_ENABLEHPSWMSGS "<form action=\"/admin5\" method=\"post\"><input type=\"hidden\" name=\"hpmsgs\" value=\"on\" Checked><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DISABLEHPSWMSGS "<form action=\"/admin5\" method=\"post\"><input type=\"hidden\" name=\"hpmsgs\" value=\"off\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
// TMC2209 Stall Guard Options
// TMC2209 html to create radio buttons for stall guard
#define H_SG_FORM "<form action=\"/admin5\" method=\"post\"><input type=\"hidden\" name=\"stg\" value=\"true\">"
#define H_SG_FORM_BUTTON "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Set\"> </form>"
#define H_SG_Use_Stallguard_CHECKED "<input type=\"radio\" name=\"stgs\" value=\"0\" Checked> Use_Stallguard"
#define H_SG_Use_Stallguard_UNCHECKED "<input type=\"radio\" name=\"stgs\" value=\"0\"> Use_Stallguard"
#define H_SG_Use_Physical_Switch_CHECKED "<input type=\"radio\" name=\"stgs\" value=\"1\" Checked> Use_Physical_Switch"
#define H_SG_Use_Physical_Switch_UNCHECKED "<input type=\"radio\" name=\"stgs\" value=\"1\"> Use_Physical_Switch"
#define H_SG_Use_None_CHECKED "<input type=\"radio\" name=\"stgs\" value=\"2\" Checked> Use_None"
#define H_SG_Use_None_UNCHECKED "<input type=\"radio\" name=\"stgs\" value=\"2\"> Use_None"
// Stall Guard Value
#define H_SG_VALUE "<form action=\"/admin5\" method=\"post\"><input type=\"text\" name=\"sgv\" size=\"10\" value=\"%sgval%\">"
#define H_SG_VALUEBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setsgv\" value=\"Set\"></form>"
// TMC2209 Current setting mA
#define H_TMC2209CURRENT "<form action=\"/admin5\" method=\"post\"><input type=\"text\" name=\"2209c\" size=\"10\" value=\"%c09%\">"
#define H_TMC2209CURRENTBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"set09c\" value=\"Set\"></form>"
// TMC2225 Current setting mA
#define H_TMC2225CURRENT "<form action=\"/admin5\" method=\"post\"><input type=\"text\" name=\"2225c\" size=\"10\" value=\"%c25%\">"
#define H_TMC2225CURRENTBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"set25c\" value=\"Set\"></form>"

// ADMIN6 INPUTS
// LED Enable/Disable State %IOS% Enable/Disable %BIOS%
#define H_ENABLELED "<form action=\"/admin6\" method=\"post\"><input type=\"hidden\" name=\"ledst\" value=\"on\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DISABLELED "<form action=\"/admin6\" method=\"post\"><input type=\"hidden\" name=\"ledst\" value=\"off\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
// LED Mode: %IOM% Pulse/Move %BIOM%
#define H_LEDMODEMOVE "<form action=\"/admin6\" method=\"post\"><input type=\"hidden\" name=\"lmode\" value=\"mov\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"MOVE\"></form>"
#define H_LEDMODEPULSE "<form action=\"/admin6\" method=\"post\"><input type=\"hidden\" name=\"lmode\" value=\"pul\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"PULSE\"></form>"
// Push Buttons State %PBS% Enable/Disable %BPBS%
#define H_ENABLEPB "<form action=\"/admin6\" method=\"post\"><input type=\"hidden\" name=\"pbst\" value=\"on\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DISABLEPB "<form action=\"/admin6\" method=\"post\"><input type=\"hidden\" name=\"pbst\" value=\"off\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
// Push Buttons Steps value %PBN%  form field %PBNI% button %BPBN%
#define H_PBSTEPS "<form action=\"/admin6\" method =\"post\"><input type=\"text\" name=\"pbn\" size=\"6\" value=\"%pbnum%\">"
#define H_PBSTEPSFORMED "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setpbn\" value=\"Set\"></form>"
// Joystick1
#define H_ENABLEJ1 "<form action=\"/admin6\" method=\"post\"><input type=\"hidden\" name=\"j1st\" value=\"on\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DISABLEJ1 "<form action=\"/admin6\" method=\"post\"><input type=\"hidden\" name=\"j1st\" value=\"off\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
// Joystick2
#define H_ENABLEJ2 "<form action=\"/admin6\" method=\"post\"><input type=\"hidden\" name=\"j2st\" value=\"on\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DISABLEJ2 "<form action=\"/admin6\" method=\"post\"><input type=\"hidden\" name=\"j2st\" value=\"off\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"

// ADMIN7 DISPLAY
// DisplayEnable/Disable
#define H_ENABLEDISPLAYFORM "<form action=\"/admin7\" method=\"post\">"
#define H_ENABLEDISPLAYBUTTON "<input type=\"hidden\" name=\"en\" value=\"enable\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DISABLEDISPLAYFORM "<input type=\"hidden\" name=\"en\" value=\"disable\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
// Display Start/Stop
#define H_STARTDISPLAYFORM "<form action=\"/admin7\" method=\"post\">"
#define H_STARTDISPLAYBUTTON "<input type=\"hidden\" name=\"st\" value=\"start\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Start\"></form>"
#define H_STOPDISPLAYBUTTON "<input type=\"hidden\" name=\"st\" value=\"stop\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Stop\"></form>"
// Display Update Position When Moving
#define H_UPDATEPOSFORM "<form action=\"/admin7\" method=\"post\">"
#define H_UPDATEPOSENABLE "<input type=\"hidden\" name=\"up\" value=\"enable\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_UPDATEPOSDISABLE "<input type=\"hidden\" name=\"up\" value=\"disable\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
// Page Display Time
#define H_PGTIMEFORM "<form action=\"/admin7\" method=\"post\">"
#define H_PGTIMEINPUT "<input type=\"text\" name=\"pt\" style=\"height: 1.6em; width: 4.0em\"value=\"%ptnum%\">"
#define H_PGTIMEBUTTON "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setpt\" value=\"Set\"></form>"
// Page Options, checkboxes, form info is inline
#define H_PAGE1_CHECKED "<input type=\"checkbox\" name=\"pg1\" Checked> Pos, Target, isMoving, Temp, Maxsteps, Speed, Stepmode"
#define H_PAGE1_UNCHECKED "<input type=\"checkbox\" name=\"pg1\" > Pos, Target, isMoving, Temp, Maxsteps, Speed, Stepmode"
#define H_PAGE2_CHECKED "<input type=\"checkbox\" name=\"pg2\" Checked> CP, RD, Backlash"
#define H_PAGE2_UNCHECKED "<input type=\"checkbox\" name=\"pg2\" > CP, RD, Backlash"
#define H_PAGE3_CHECKED "<input type=\"checkbox\" name=\"pg3\" Checked> Stepsize, HPsw, Leds, Buttons"
#define H_PAGE3_UNCHECKED "<input type=\"checkbox\" name=\"pg3\" > Stepsize, HPsw, Leds, Buttons"
#define H_PAGE4_CHECKED "<input type=\"checkbox\" name=\"pg4\" Checked> Temperature Settings"
#define H_PAGE4_UNCHECKED "<input type=\"checkbox\" name=\"pg4\" > Temperature Settings"
#define H_PAGE5_CHECKED "<input type=\"checkbox\" name=\"pg5\" Checked> Display Settings"
#define H_PAGE5_UNCHECKED "<input type=\"checkbox\" name=\"pg5\" > Display Settings"
#define H_PAGE6_CHECKED "<input type=\"checkbox\" name=\"pg6\" Checked> Presets"
#define H_PAGE6_UNCHECKED "<input type=\"checkbox\" name=\"pg6\" > Presets"
#define H_PAGE7_CHECKED "<input type=\"checkbox\" name=\"pg7\" Checked> AP-SM, SSID, IP, ASCOM and MANAGEMENT Servers"
#define H_PAGE7_UNCHECKED "<input type=\"checkbox\" name=\"pg7\" > AP-SM, SSID, IP, ASCOM and MANAGEMENT Servers"
#define H_PAGE8_CHECKED "<input type=\"checkbox\" name=\"pg8\" Checked> TCPIP and Web Servers"
#define H_PAGE8_UNCHECKED "<input type=\"checkbox\" name=\"pg8\" > TCPIP and Web Servers"
#define H_DISPLAYPOFORM "<form action=\"/admin7\" method=\"post\">"
#define H_DISPLAYPOBUTTON "<input type=\"submit\" name=\"spo\" style=\"height: 1.6em; width: 5.5em\" value=\"Set\"></form>"

// ADMIN8 TEMPERATURE PROBE
// Temp probe Enabled State %TPP% Enable/Disable %BTP%
#define H_ENABLETEMPPROBE "<form action=\"/admin8\" method=\"post\"><input type=\"hidden\" name=\"tps\" value=\"enable\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Enable\"></form>"
#define H_DISABLETEMPPROBE "<form action=\"/admin8\" method=\"post\"><input type=\"hidden\" name=\"tps\" value=\"disable\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Disable\"></form>"
// Temp probe Run State %TPR% Start/Stop %BTPR%
#define H_STARTTEMPPROBE "<form action=\"/admin8\" method=\"post\"><input type=\"hidden\" name=\"tpru\" value=\"start\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Start\"></form>"
#define H_STOPTEMPPROBE "<form action=\"/admin8\" method=\"post\"><input type=\"hidden\" name=\"tpru\" value=\"stop\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Stop\"></form>"
// Temp Mode %TPM% State %BTM%
#define H_TEMPMODECELSIUS "<form action=\"/admin8\" method=\"post\"><input type=\"hidden\" name=\"tm\" value=\"cel\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"C\"></form>"
#define H_TEMPMODEFAHRENHEIT "<form action=\"/admin8\" method=\"post\"><input type=\"hidden\" name=\"tm\" value=\"fah\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"F\"></form>"
// Temp probe resolution
#define H_TEMPPROBERESOLUTION "<form action=\"/admin8\" method =\"post\"><input type=\"text\" name=\"tpr\" size=\"6\" value=\"%tprn%\"> <input type=\"submit\"style=\"height: 1.6em; width: 5.5em\" name=\"settpr\" value=\"Set\"></form>"
// Temp Comp Direction %TCD% %BTCD%
#define H_TEMPCOMPDIRECTIONIN "<form action=\"/admin8\" method=\"post\"><input type=\"hidden\" name=\"tcd\" value=\"in\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"In\"></form>"
#define H_TEMPCOMPDIRECTIONOUT "<form action=\"/admin8\" method=\"post\"><input type=\"hidden\" name=\"tcd\" value=\"out\"><input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" value=\"Out\"></form>"
// Temp Coefficent
#define H_TEMPCOEFFICENT "<form action=\"/admin8\" method =\"post\"><input type=\"text\" name=\"tce\" size=\"6\" value=\"%tcnum%\"> <input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"settc\" value=\"Set\"></form>"

// ADMIN9 MISC
// DEVICE NAME
#define H_DEVICE_NAME "<form action=\"/admin9\" method=\"post\"><input type=\"text\" name=\"dname\" size=\"10\" value=\"%dnam%\">"
#define H_DEVICE_NAMEBTN "<input type=\"submit\" style=\"height: 1.6em; width: 5.5em\" name=\"setdname\" value=\"Set\"></form>"
// File list format
#define H_FILELISTFORM "<form action=\"/admin9\" method=\"post\">"
#define H_FILELISTSHORT "<input type=\"hidden\" name=\"flf\" value=\"short\"><input type=\"submit\" name=\"listt\" style=\"height: 1.6em; width: 5.5em\" value=\"SHORT\">"
#define H_FILELISTLONG "<input type=\"hidden\" name=\"flf\" value=\"long\"><input type=\"submit\" name=\"listt\" style=\"height: 1.6em; width: 5.5em\" value=\"LONG\">"
#define H_FILELISTFORMEND "</form>"
// Web page colors
#define H_COLORFORM "<form action=\"/admin9\" method=\"post\">"
#define H_TITLECOLORFIELD "<input type=\"text\" name=\"tic\" size=\"6\" value=\"%tinum%\">"
#define H_TITLECOLORBTN "<input type=\"submit\" name=\"tiset\" style=\"height: 1.6em; width: 5.5em\" value=\"SET\">"
#define H_SUBTITLECOLORFIELD "<input type=\"text\" name=\"stc\" size=\"6\" value=\"%stnum%\">"
#define H_SUBTITLECOLORBTN "<input type=\"submit\" name=\"stset\" style=\"height: 1.6em; width: 5.5em\" value=\"SET\">"
#define H_HEADERCOLORFIELD "<input type=\"text\" name=\"hci\" size=\"6\" value=\"%hcnum%\">"
#define H_HEADERCOLORBTN "<input type=\"submit\" name=\"hcset\" style=\"height: 1.6em; width: 5.5em\" value=\"SET\">"
#define H_TEXTCOLORFIELD "<input type=\"text\" name=\"tci\" size=\"6\" value=\"%tcnum%\">"
#define H_TEXTCOLORBTN "<input type=\"submit\" name=\"tecset\" style=\"height: 1.6em; width: 5.5em\" value=\"SET\">"
#define H_BACKCOLORFIELD "<input type=\"text\" name=\"bci\" size=\"6\" value=\"%bcnum%\">"
#define H_BACKCOLORBTN "<input type=\"submit\" name=\"bcset\" style=\"height: 1.6em; width: 5.5em\" value=\"SET\">"
#define H_COLORFORMEND "</form>"

// adminmove
// Move HTML page
#define H_MOVL500 "<form action=\"/move\" method=\"post\"><input type=\"hidden\" name=\"mvl500\" value=\"true\"><input type=\"submit\" style=\"height: 1.6em; width: 3.5em\" value=\"-500\"></form>"
#define H_MOVL100 "<form action=\"/move\" method=\"post\"><input type=\"hidden\" name=\"mvl100\" value=\"true\"><input type=\"submit\" style=\"height: 1.6em; width: 3.5em\" value=\"-100\"></form>"
#define H_MOVL10 "<form action=\"/move\" method=\"post\"><input type=\"hidden\" name=\"mvl10\" value=\"true\"><input type=\"submit\" style=\"height: 1.6em; width: 3.5em\" value=\"-10\"></form>"
#define H_MOVL1 "<form action=\"/move\" method=\"post\"><input type=\"hidden\" name=\"mvl1\" value=\"true\"><input type=\"submit\" style=\"height: 1.6em; width: 3.5em\" value=\"-1\"></form>"
#define H_MOVP1 "<form action=\"/move\" method=\"post\"><input type=\"hidden\" name=\"mvp1\" value=\"true\"><input type=\"submit\" style=\"height: 1.6em; width: 3.5em\" value=\"1\"></form>"
#define H_MOVP10 "<form action=\"/move\" method=\"post\"><input type=\"hidden\" name=\"mvp10\" value=\"true\"><input type=\"submit\" style=\"height: 1.6em; width: 3.5em\" value=\"10\"></form>"
#define H_MOVP100 "<form action=\"/move\" method=\"post\"><input type=\"hidden\" name=\"mvp100\" value=\"true\"><input type=\"submit\" style=\"height: 1.6em; width: 3.5em\" value=\"100\"></form>"
#define H_MOVP500 "<form action=\"/move\" method=\"post\"><input type=\"hidden\" name=\"mvp500\" value=\"true\"><input type=\"submit\" style=\"height: 1.6em; width: 3.5em\" value=\"500\"></form>"
#define H_HALTBTNMOVE "<form action=\"/move\" method=\"post\"><input type=\"hidden\" name=\"ha\" value=\"true\"><input type=\"submit\" style=\"height: 1.6em; width: 4.5em\" value=\"Halt\"></form>"
#define H_MPI "<form action=\"/move\" method=\"post\"><input type=\"text\" name=\"pos\" size=\"10\" value=\"\">"
#define H_MBP "<input type=\"submit\" style=\"height: 1.6em; width: 4.5em\" value=\"GOTO\"></form>"

// pages
#define T_ADMIN1 "/admin1 "
#define T_ADMIN2 "/admin2 "
#define T_ADMIN3 "/admin3 "
#define T_ADMIN4 "/admin4 "
#define T_ADMIN5 "/admin5 "
#define T_ADMIN6 "/admin6 "
#define T_ADMIN7 "/admin7 "
#define T_ADMIN8 "/admin8 "
#define T_ADMIN9 "/admin9 "
#define T_MOVE "/move "
#define T_LINKS "/links "
#define T_DELETE "/delete "
#define T_DELETEOK "/deleteok "
#define T_SAVE "/save "
#define T_CONFIGSAVED "/configsaved "
#define T_CONFIGNOTSAVED "/confignotsaved "
#define T_ADMINNOTFOUND "/adminnotfound "
#define T_UPLOAD "/upload "
#define T_SUCCESS "/success "
#define T_FAIL "/fail "
#define T_CMDS "/cmds "
#define T_BOARDEDIT "/brdedit "
#define GeT true
#define PosT false
bool admin1_type;
bool admin2_type;
bool admin3_type;
bool admin4_type;
bool admin5_type;
bool admin6_type;
bool admin7_type;
bool admin8_type;
bool admin9_type;
bool adminmove_type;
bool adminlinks_type;
bool brdedit_type;
//bool brdsave_type;

// ----------------------------------------------------------------------
// helper handlers .on for mserver
// ----------------------------------------------------------------------
void ms_getadmin1() {
  admin1_type = GeT;
  mngsrvr->getadmin1();
}

void ms_postadmin1() {
  admin1_type = PosT;
  mngsrvr->getadmin1();
}

void ms_getadmin2() {
  admin2_type = GeT;
  mngsrvr->getadmin2();
}

void ms_postadmin2() {
  admin2_type = PosT;
  mngsrvr->getadmin2();
}

void ms_getadmin3() {
  admin3_type = GeT;
  mngsrvr->getadmin3();
}

void ms_postadmin3() {
  admin3_type = PosT;
  mngsrvr->getadmin3();
}

void ms_getadmin4() {
  admin4_type = GeT;
  mngsrvr->getadmin4();
}

void ms_postadmin4() {
  admin4_type = PosT;
  mngsrvr->getadmin4();
}

void ms_getadmin5() {
  admin5_type = GeT;
  mngsrvr->getadmin5();
}

void ms_postadmin5() {
  admin5_type = PosT;
  mngsrvr->getadmin5();
}

void ms_getadmin6() {
  admin6_type = GeT;
  mngsrvr->getadmin6();
}

void ms_postadmin6() {
  admin6_type = PosT;
  mngsrvr->getadmin6();
}

void ms_getadmin7() {
  admin7_type = GeT;
  mngsrvr->getadmin7();
}

void ms_postadmin7() {
  admin7_type = PosT;
  mngsrvr->getadmin7();
}

void ms_getadmin8() {
  admin8_type = GeT;
  mngsrvr->getadmin8();
}

void ms_postadmin8() {
  admin8_type = PosT;
  mngsrvr->getadmin8();
}

void ms_getadmin9() {
  admin9_type = GeT;
  mngsrvr->getadmin9();
}

void ms_postadmin9() {
  admin9_type = PosT;
  mngsrvr->getadmin9();
}

void ms_getmove(void) {
  adminmove_type = GeT;
  mngsrvr->getmove();
}

void ms_postmove(void) {
  adminmove_type = PosT;
  mngsrvr->getmove();
}

void ms_getlinks(void) {
  mngsrvr->getlinks();
}

void ms_getnotfound() {
  mngsrvr->get_notfound();
}

String ms_geturi(void) {
  return mngsrvr->get_uri();
}

void ms_deletefile() {
  mngsrvr->get_deletefile();
}

void ms_post_deletefile() {
  mngsrvr->post_deletefile();
}

void ms_get_filelist() {
  if (ControllerData->get_filelistformat() == LISTSHORT) {
    mngsrvr->get_filelist_short();
  } else {
    mngsrvr->get_filelist_long();
  }
}

void ms_cmds() {
  mngsrvr->handlecmds();
}

void ms_uploadfile() {
  mngsrvr->upload_file();
}

void ms_post_uploadstart(void) {
  mngsrvr->post_uploadstart();
}

void ms_post_uploadfile(void) {
  mngsrvr->post_uploadfile();
}

void ms_success(void) {
  mngsrvr->success();
}

void ms_fail(void) {
  mngsrvr->fail();
}

// get interface: JSON
void ms_handleget() {
  mngsrvr->handleget();
}

// set interface: JSON
void ms_handleset() {
  mngsrvr->handleset();
}

// Driver Board management
void msget_brdedit() {
  brdedit_type = GeT;
  mngsrvr->brdedit();
}

void mspost_brdedit() {
  brdedit_type = PosT;
  mngsrvr->brdedit();
}

void ms_rssi() {
  mngsrvr->rssi();
}

void ms_saveconfig(void) {
  mngsrvr->saveconfig();
}

// XHTML
void ms_getposition() {
  mngsrvr->getposition();
}

void ms_getismoving() {
  mngsrvr->getismoving();
}

void ms_gettargetposition() {
  mngsrvr->gettargetposition();
}

//void ms_reboot()
//{
//  mngsrvr->reboot();
//}

// ----------------------------------------------------------------------
// MANAGEMENT SERVER CLASS
// ----------------------------------------------------------------------
MANAGEMENT_SERVER::MANAGEMENT_SERVER() {
}

// ----------------------------------------------------------------------
// start management server
// ----------------------------------------------------------------------
bool MANAGEMENT_SERVER::start(unsigned long port) {
  debug_server_println(T_MANAGEMENTSERVER);
  debug_server_print(T_START);

  // prevent any attempt to start if already started
  if (this->_loaded == true) {
    return true;
  }

  this->_port = port;

  // update port if not the same as in controllerdata
  if (this->_port != ControllerData->get_mngsrvr_port()) {
    ControllerData->set_mngsrvr_port(this->_port);
  }

  // check if server already created, if not, create one
  if (this->_loaded == false) {
    mserver = new WebServer(this->_port);
  }

  // admin pages
  mserver->on("/", HTTP_GET, ms_getadmin1);
  mserver->on("/", HTTP_POST, ms_postadmin1);
  mserver->on("/admin1", HTTP_GET, ms_getadmin1);
  mserver->on("/admin1", HTTP_POST, ms_postadmin1);
  mserver->on("/admin2", HTTP_GET, ms_getadmin2);
  mserver->on("/admin2", HTTP_POST, ms_postadmin2);
  mserver->on("/admin3", HTTP_GET, ms_getadmin3);
  mserver->on("/admin3", HTTP_POST, ms_postadmin3);
  mserver->on("/admin4", HTTP_GET, ms_getadmin4);
  mserver->on("/admin4", HTTP_POST, ms_postadmin4);
  mserver->on("/admin5", HTTP_GET, ms_getadmin5);
  mserver->on("/admin5", HTTP_POST, ms_postadmin5);
  mserver->on("/admin6", HTTP_GET, ms_getadmin6);
  mserver->on("/admin6", HTTP_POST, ms_postadmin6);
  mserver->on("/admin7", HTTP_GET, ms_getadmin7);
  mserver->on("/admin7", HTTP_POST, ms_postadmin7);
  mserver->on("/admin8", HTTP_GET, ms_getadmin8);
  mserver->on("/admin8", HTTP_POST, ms_postadmin8);
  mserver->on("/admin9", HTTP_GET, ms_getadmin9);
  mserver->on("/admin9", HTTP_POST, ms_postadmin9);

  // XHTML
  mserver->on("/po", ms_getposition);
  mserver->on("/im", ms_getismoving);
  mserver->on("/ta", ms_gettargetposition);

  // file handling pages
  mserver->on("/delete", HTTP_GET, ms_deletefile);
  mserver->on("/delete", HTTP_POST, ms_post_deletefile);
  mserver->on("/list", HTTP_GET, ms_get_filelist);
  mserver->on("/upload", HTTP_GET, ms_uploadfile);
  mserver->on(
    "/upload", HTTP_POST, []() {
      ms_post_uploadstart();
    },
    ms_post_uploadfile);
  mserver->on("/success", ms_success);
  mserver->on("/fail", ms_fail);
  // get set JSON interface
  mserver->on("/cmds", ms_cmds);
  // generic get function
  mserver->on("/get", ms_handleget);
  // generic set function
  mserver->on("/set", ms_handleset);
  // driver board management
  mserver->on("/brdedit", HTTP_GET, msget_brdedit);
  mserver->on("/brdedit", HTTP_POST, mspost_brdedit);
  // misc
  mserver->on("/rssi", HTTP_GET, ms_rssi);
  mserver->on("/uri", ms_geturi);
  mserver->on("/save", ms_saveconfig);
  mserver->on("/move", HTTP_GET, ms_getmove);
  mserver->on("/move", HTTP_POST, ms_postmove);
  mserver->on("/links", ms_getlinks);

  // not found
  mserver->onNotFound([]() {
    ms_getnotfound();
  });

  // start elegant ota
#if defined(ENABLE_ELEGANTOTA)
  char buff[32];
  debug_server_print(T_OTA);
  debug_server_println(T_START);
  ControllerData->get_ota_id().toCharArray(buff, ControllerData->get_ota_id().length() + 1);
  ElegantOTA.setID(buff); // removed since no ".setID(buf)" defined -> MN
  ElegantOTA.begin(mserver, OTAName, OTAPassword);
  ota_status = V_RUNNING;
  // Elegant OTA cannot be stopped
#endif
  mserver->begin();
  this->_loaded = true;
  this->_state = V_RUNNING;
  debug_server_println(T_RUNNING);
  return this->_loaded;
}

// ----------------------------------------------------------------------
// stop management server
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::stop(void) {
  debug_server_print(T_MANAGEMENTSERVER);
  debug_server_println(T_STOP);
  if (this->_loaded == true) {
    mserver->stop();
    delete mserver;
    this->_loaded = false;
    this->_state = V_STOPPED;
#if defined(ENABLE_ELEGANTOTA)
    ota_status = V_STOPPED;
#endif
  }
}

// ----------------------------------------------------------------------
// void loop(bool);
// check for clients
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::loop(bool parked) {
  // avoid a crash
  if (this->_loaded == false) {
    return;
  }
  _parkstate = parked;
  mserver->handleClient();
}

// ----------------------------------------------------------------------
// bool handlefileread(String);
// send the right file to the client (if it exists)
// ----------------------------------------------------------------------
bool MANAGEMENT_SERVER::handlefileread(String path) {
  debug_server_print(T_MS);
  debug_server_print(T_READFILE);
  debug_server_println(path);
  if (path.endsWith("/")) {
    // if a folder is requested, send the index file
    path += "index.html";
  }
  // get the MIME type
  String contentType = get_contenttype(path);
  // if the file exists
  if (SPIFFS.exists(path)) {
    File file = SPIFFS.open(path, "r");
    // send it to the client
    mserver->streamFile(file, contentType);
    file.close();
    return true;
  } else {
    // resource does not exist
    return false;
  }
}

// ----------------------------------------------------------------------
// File System Not Loaded
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::file_sys_error(void) {
  // spiffs not started
  debug_server_print(T_MS);
  debug_server_println(T_ERRORFILESYSTEM);
  send_myheader();
  send_mycontent(H_FSNOTLOADEDSTR);
}

// ----------------------------------------------------------------------
// Check if everything is ok to load a page
// ----------------------------------------------------------------------
bool MANAGEMENT_SERVER::check_access(void) {
  if (!mserver->authenticate(admin_username, admin_password)) {
    debug_server_print(T_MS);
    debug_server_println(T_ERRAUTH);
    mserver->requestAuthentication();
    return false;
  }

  if (this->_loaded == false) {
    debug_server_print(T_MS);
    debug_server_println(T_NOTLOADED);
    return false;
  } else {
    return true;
  }
}

// ----------------------------------------------------------------------
// Send a redirect pg to client
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::send_redirect(String pg) {
  mserver->sendHeader("Location", pg);
  mserver->send(301);
}

// ----------------------------------------------------------------------
// send json string to client
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::send_json(String str) {
  mserver->sendHeader("Access-Control-Allow-Origin", "*");
  mserver->send(NORMALWEBPAGE, JSONPAGETYPE, str);
}

// ----------------------------------------------------------------------
// convert the file extension to the MIME type
// ----------------------------------------------------------------------
String MANAGEMENT_SERVER::get_contenttype(String filename) {
  String retval = "text/plain";
  if (filename.endsWith(".html")) {
    retval = "text/html";
  } else if (filename.endsWith(".css")) {
    retval = "text/css";
  } else if (filename.endsWith(".js")) {
    retval = "application/javascript";
  } else if (filename.endsWith(".json")) {
    retval = "text/json";
  } else if (filename.endsWith(".ico")) {
    retval = "image/x-icon";
  }
  //retval = "application/octet-stream";
  return retval;
}

// ----------------------------------------------------------------------
// check if digit is hex
// ----------------------------------------------------------------------
bool MANAGEMENT_SERVER::is_hexdigit(char c) {
  if ((c >= '0') && (c <= '9')) {
    return true;
  }
  if ((c >= 'a') && (c <= 'f')) {
    return true;
  }
  if ((c >= 'A') && (c <= 'F')) {
    return true;
  }
  return false;
}

// ----------------------------------------------------------------------
// Handler for /admin1
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::getadmin1(void) {
  String AdminPg;
  String msg;

  AdminPg.reserve(6100);  // 306-05   5840

  debug_server_println(T_ADMIN1);

  if (!check_access()) {
    return;
  }

  // post handler
  if (admin1_type == PosT) {
    debug_server_println(T_POST);
    checkreboot();

    // ASCOM ALPACA Server name=ascome, value=enable or value=disable
    msg = mserver->arg("ascome");
    if (msg != "") {
      if (msg == "enable") {
        ControllerData->set_ascomsrvr_enable(V_ENABLED);
      } else if (msg == "disable") {
        ascomsrvr->stop();
        ascomsrvr_status = V_STOPPED;
        ControllerData->set_ascomsrvr_enable(V_NOTENABLED);
      }
      goto Get_Handler;
    }

    // ASCOM ALPACA Server start/stop service, name=ascoms, value=start or value=stop
    msg = mserver->arg("ascoms");
    if (msg != "") {
      if (msg == "start") {
        if (ControllerData->get_ascomsrvr_enable() == V_ENABLED) {
          ascomsrvr_status = ascomsrvr->start();
          if (ascomsrvr_status != V_RUNNING) {
            debug_server_print(T_ERRSTART);
            debug_server_println(T_ALPACA);
          }
        }
      } else if (msg == "stop") {
        ascomsrvr->stop();
        ascomsrvr_status = V_STOPPED;
      }
      goto Get_Handler;
    }

    // ASCOM ALPACA Server port, name=ap, setasport
    msg = mserver->arg("setasport");
    if (msg != "") {
      String ap = mserver->arg("aport");
      if (ap != "") {
        unsigned long newport = 0;
        newport = ap.toInt();
        if (ascomsrvr_status == V_STOPPED) {
          unsigned long currentport = ControllerData->get_ascomsrvr_port();
          if (newport == currentport) {
            // port is the same so do not bother to change it
          } else {
            if (newport == ControllerData->get_debugsrvr_port()) {
              // do not bother to change it
            }                   
            else if (newport == ControllerData->get_mngsrvr_port()) {
              // do not bother to change it
            } else if (newport == ControllerData->get_websrvr_port()) {
              // do not bother to change it
            } else if (newport == ControllerData->get_tcpipsrvr_port()) {
              // do not bother to change it
            } else {
              ControllerData->set_ascomsrvr_port(newport);
            }
          }
        }
      }
      goto Get_Handler;
    }

    // debug server start-stop
    msg = mserver->arg("startds");
    if (msg != "") {
      debugsrvr_status = start_debugserver();
      goto Get_Handler;
    }
    // stop
    msg = mserver->arg("stopds");
    if (msg != "") {
      stop_debugserver();
      goto Get_Handler;
    }

    // if update debug server port
    msg = mserver->arg("setdbport");
    if (msg != "") {
      if (debugsrvr_status == V_RUNNING) {
        goto Get_Handler;
      }
      // get new server port number
      String tp = mserver->arg("dbp");
      if (tp != "") {
        unsigned long newport = (unsigned long)tp.toInt();
        unsigned long currentport = ControllerData->get_debugsrvr_port();
        if (newport == currentport) {
          // do nothing
        } else if (newport < 1000) {
          newport = DEBUGSERVERPORT;
        } else if (newport == ControllerData->get_ascomsrvr_port()) {
          // port is the same so do not bother to change it
        } else if (newport == ControllerData->get_websrvr_port()) {
          // do not bother to change it
        } else if (newport == ControllerData->get_mngsrvr_port()) {
          // do not bother to change it
        } else if (newport == ControllerData->get_tcpipsrvr_port()) {
          // do not bother to change it
        } else {
          // assign new port and save it
          ControllerData->set_debugsrvr_port(newport);
        }
      }
      goto Get_Handler;
    }

    // debug server output, setdbp (port), setdbs (serial)
    msg = mserver->arg("setport");
    if (msg != "") {
      ControllerData->set_debugsrvr_out(DEBUGSERVEROUTPUTPORT);
      goto Get_Handler;
    }
    msg = mserver->arg("setserial");
    if (msg != "") {
      ControllerData->set_debugsrvr_out(DEBUGSERVEROUTPUTSERIAL);
      goto Get_Handler;
    }

    // TCP/IP Server name=tcpe, value=enable or value=disable
    msg = mserver->arg("tcpe");
    if (msg != "") {
      if (msg == "enable") {
        ControllerData->set_tcpipsrvr_enable(V_ENABLED);
      } else if (msg == "disable") {
        tcpipsrvr->stop();
        tcpipsrvr_status = V_STOPPED;
        ControllerData->set_tcpipsrvr_enable(V_NOTENABLED);
      }
      goto Get_Handler;
    }

    // TCP/IP Server name=tcps, value=start or value=stop
    msg = mserver->arg("tcps");
    if (msg != "") {
      if (msg == "start") {
        if (ControllerData->get_tcpipsrvr_enable() == V_ENABLED) {
          tcpipsrvr_status = tcpipsrvr->start(ControllerData->get_tcpipsrvr_port());
          if (tcpipsrvr_status != V_RUNNING) {
            debug_server_print(T_ERRSTART);
            debug_server_println(T_TCPIPSERVER);
          }
        }
      } else if (msg == "stop") {
        tcpipsrvr->stop();
        tcpipsrvr_status = V_STOPPED;
      }
      goto Get_Handler;
    }

    // TCP/IP Server change port
    msg = mserver->arg("settcport");
    if (msg != "") {
      String tp = mserver->arg("tport");
      if (tp != "") {
        unsigned long newport = 0;
        newport = tp.toInt();
        if (tcpipsrvr_status == V_STOPPED) {
          unsigned long currentport = ControllerData->get_tcpipsrvr_port();
          if (newport == currentport) {
            // ignore
          } else if (newport == ControllerData->get_mngsrvr_port()) {
            // do not bother to change it
          } else if (newport == ControllerData->get_ascomsrvr_port()) {
            // do not bother to change it
          } else if (newport == ControllerData->get_websrvr_port()) {
            // do not bother to change it
          } else {
            ControllerData->set_tcpipsrvr_port(newport);
          }
        }
      }
      goto Get_Handler;
    }

    // WEB Server name=webe, value=enable or value=disable
    msg = mserver->arg("webe");
    if (msg != "") {
      if (msg == "enable") {
        ControllerData->set_websrvr_enable(V_ENABLED);
      } else if (msg == "disable") {
        websrvr->stop();
        websrvr_status = V_STOPPED;
        ControllerData->set_websrvr_enable(V_NOTENABLED);
      }
      goto Get_Handler;
    }

    // Web Server name=webs, value=start or value=stop
    msg = mserver->arg("webs");
    if (msg != "") {
      if (msg == "start") {
        if (ControllerData->get_websrvr_enable() == V_ENABLED) {
          // enabled
          websrvr_status = websrvr->start(ControllerData->get_websrvr_port());
          if (websrvr_status != V_RUNNING) {
            debug_server_print(T_ERRSTART);
            debug_server_println(T_WEBSERVER);
          }
        }
      } else if (msg == "stop") {
        websrvr->stop();
        websrvr_status = V_STOPPED;
      }
      goto Get_Handler;
    }

    // webserver change port - we should be able to change port if not running or enabled or not enabled
    msg = mserver->arg("setwsport");
    if (msg != "") {
      String wp = mserver->arg("wport");
      if (wp != "") {
        unsigned long newport = 0;
        newport = wp.toInt();
        if (websrvr_status == V_STOPPED) {
          unsigned long currentport = ControllerData->get_websrvr_port();
          if (newport == currentport) {
            // port is the same so do not bother to change it
          } else if (newport == ControllerData->get_mngsrvr_port()) {
            // do not bother to change it
          } else if (newport == ControllerData->get_ascomsrvr_port()) {
            // do not bother to change it
          } else if (newport == ControllerData->get_tcpipsrvr_port()) {
            // do not bother to change it
          } else {
            ControllerData->set_websrvr_port(newport);
          }
        }
      }
    }
  }  // end of post handler

Get_Handler:

  if (SPIFFS.exists("/admin1.html")) {
    File file = SPIFFS.open("/admin1.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    // ASCOM Server enable/disable %ASS %BAS%
    if (ControllerData->get_ascomsrvr_enable() == V_ENABLED) {
      AdminPg.replace("%BAS%", H_DISABLEASCOMSRVR);
      AdminPg.replace("%ASS%", T_ENABLED);
    } else {
      AdminPg.replace("%BAS%", H_ENABLEASCOMSRVR);
      AdminPg.replace("%ASS%", T_DISABLED);
    }

    // ASCOM server [start | stop] service Button %BASST%, Status %ASST%, name=ascom
    if (ascomsrvr_status == V_RUNNING) {
      AdminPg.replace("%BASST%", H_STOPASCOMSRVR);
      AdminPg.replace("%ASST%", T_RUNNING);
    } else {
      AdminPg.replace("%BASST%", H_STARTASCOMSRVR);
      AdminPg.replace("%ASST%", T_STOPPED);
    }

    // ASCOM change port, %ASP%, %ASPI% %BASP%
    String argval = String(ControllerData->get_ascomsrvr_port());
    // Input text field for port number
    msg = H_ASCOMSRVRPORT;
    msg.replace("%ASE%", argval);
    AdminPg.replace("%ASPI%", msg);
    AdminPg.replace("%BASP%", H_ASCOMSRVRPORTBTN);

    // Debug Server State and Start/Stop
    if (debugsrvr_status == V_RUNNING) {
      AdminPg.replace("%DBGS%", T_RUNNING);
      AdminPg.replace("%BDBGS%", DSSTOP);
    } else {
      AdminPg.replace("%DBGS%", T_STOPPED);
      AdminPg.replace("%BDBGS%", DSSTART);
    }

    // Debug Server Port %DBGP% value, %dbp% text field, %BDBGP% button
    msg = DEBUGPORTFORM;
    msg.replace("%dbp%", String(ControllerData->get_debugsrvr_port()));
    AdminPg.replace("%DBGP%", msg);
    AdminPg.replace("%BDBGP%", DEBUGPORTBTN);

    // Debug Server Send Output to
    // 0=DEBUGSERVEROUTPUTSERIAL 1=DEBUGSERVEROUTPUTPORT
    if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
      AdminPg.replace("%DST%", "Serial");
      AdminPg.replace("%BDST%", DEBUGWRITETOPORT);
    } else {
      AdminPg.replace("%DST%", "Port");
      AdminPg.replace("%BDST%", DEBUGWRITETOSERIAL);
    }

    // TCP/IP Server [enable | disable] %TCE% %BTCE%
    if (ControllerData->get_tcpipsrvr_enable() == V_ENABLED) {
      AdminPg.replace("%BTCE%", H_DISABLETCPIPSRVR);
      AdminPg.replace("%TCE%", T_ENABLED);
    } else {
      AdminPg.replace("%BTCE%", H_ENABLETCPIPSRVR);
      AdminPg.replace("%TCE%", T_DISABLED);
    }

    // TCP/IP server [start | stop] Button %BTCPS%, Status %TCPS%, name=tcps
    if (tcpipsrvr_status == V_RUNNING) {
      AdminPg.replace("%BTCPS%", H_STOPTCPIPSRVR);
      AdminPg.replace("%TCPS%", T_RUNNING);
    } else {
      AdminPg.replace("%BTCPS%", H_STARTTCPIPSRVR);
      AdminPg.replace("%TCPS%", T_STOPPED);
    }

    // TCP/IP change port, val %TCE%, form+input %TCI%, button %BTCP%
    // Input text field for port number
    msg = H_TCPIPSRVRPORT;
    msg.replace("%TCE%", String(ControllerData->get_tcpipsrvr_port()));
    AdminPg.replace("%TCI%", msg);
    AdminPg.replace("%BTCP%", H_TCPIPSRVRPORTBTN);

    // WEB Server [enable | disable] %WSS% %BWSS%
    if (ControllerData->get_websrvr_enable() == V_ENABLED) {
      AdminPg.replace("%BWSS%", H_DISABLEWEBSRVR);
      AdminPg.replace("%WSS%", T_ENABLED);
    } else {
      AdminPg.replace("%BWSS%", H_ENABLEWEBSRVR);
      AdminPg.replace("%WSS%", T_DISABLED);
    }

    // WEB server [start | stop] Button %BWBS%, Status %WBS%, name=webs
    if (websrvr_status == V_RUNNING) {
      AdminPg.replace("%BWBS%", H_STOPWEBSRVR);
      AdminPg.replace("%WBS%", T_RUNNING);
    } else {
      AdminPg.replace("%BWBS%", H_STARTWEBSRVR);
      AdminPg.replace("%WBS%", T_STOPPED);
    }

    // Webserver Port number val %WSP% form+input %WSI% button %BWSP%
    // Input text field
    msg = H_WEBSRVRPORT;
    msg.replace("%wspnum%", String(ControllerData->get_websrvr_port()));
    AdminPg.replace("%WSI%", msg);
    AdminPg.replace("%BWSP%", H_WEBSRVRPORTBTN);

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }

  debug_server_print(T_ADMIN1);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}

// ----------------------------------------------------------------------
// Handler for /admin2
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::getadmin2(void) {
  String AdminPg;
  String msg;

  AdminPg.reserve(4300);  // 306-03    4121

  debug_server_println(T_ADMIN2);

  if (!check_access()) {
    return;
  }

  // post handler
  if (admin2_type == PosT) {
    debug_server_println(T_POST);
    checkreboot();

    // duckdns domain name
    msg = mserver->arg("setddnsdom");
    if (msg != "") {
      String dom = mserver->arg("ddomain");
      if (dom != "") {
        ControllerData->set_duckdns_domain(dom);
      }
      goto Get_Handler;
    }

    // duckdns token
    msg = mserver->arg("setddnstok");
    if (msg != "") {
      String dtok = mserver->arg("dtoken");
      if (dtok != "") {
        ControllerData->set_duckdns_token(dtok);
      }
      goto Get_Handler;
    }

    // duckdns refresh time
    msg = mserver->arg("setduckt");
    if (msg != "") {
      String st = mserver->arg("ddnsr");
      int rt = st.toInt();
      // limit range from 60s to 3600 (1hr), default is 120s
      rt = (rt < 60) ? 60 : rt;
      rt = (rt > 3600) ? 3600 : rt;
      ControllerData->set_duckdns_refreshtime(rt);
      goto Get_Handler;
    }

    // ota name
    msg = mserver->arg("setotaname");
    if (msg != "") {
      String dom = mserver->arg("otaname");
      if (dom != "") {
        ControllerData->set_ota_name(dom);
      }
      goto Get_Handler;
    }

    // ota password
    msg = mserver->arg("setotapwd");
    if (msg != "") {
      String dom = mserver->arg("otapwd");
      if (dom != "") {
        ControllerData->set_ota_password(dom);
      }
      goto Get_Handler;
    }

    // ota id
    msg = mserver->arg("setotaid");
    if (msg != "") {
      String dom = mserver->arg("otaida");
      if (dom != "") {
        ControllerData->set_ota_id(dom);
      }
      goto Get_Handler;
    }
  }  // end of post handler

Get_Handler:

  if (SPIFFS.exists("/admin2.html")) {
    File file = SPIFFS.open("/admin2.html", "r");
    AdminPg = file.readString();
    file.close();
    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    // duckdns, state %DDS%
    if (duckdns_status == V_RUNNING) {
      AdminPg.replace("%DUCS%", T_RUNNING);
    } else {
      AdminPg.replace("%DUCS%", T_STOPPED);
    }

    // duckdns domain
    msg = H_DUCKDNS_DOMAIN;
    msg.replace("%ddom%", ControllerData->get_duckdns_domain());
    AdminPg.replace("%DDNI%", msg);
    AdminPg.replace("%BDDNI%", H_DUCKDNS_DOMBTN);

    // duckdns token
    msg = H_DUCKDNS_TOKEN;
    msg.replace("%ddtok%", ControllerData->get_duckdns_token());
    AdminPg.replace("%DDNT%", msg);
    AdminPg.replace("%BDDNT%", H_DUCKDNS_TOKENBTN);

    // duckdns refresh time value [%DDT%] Field DDTI Button %BDDT%
    String rtnum = String(ControllerData->get_duckdns_refreshtime());
    msg = H_DUCKDNSRTIME;
    msg.replace("%ddnst%", rtnum);
    AdminPg.replace("%DDTI%", msg);
    AdminPg.replace("%BDDT%", H_DUCKDNSRTBTN);

    // ota state : cannot stop ota
    if (ota_status == V_RUNNING) {
      AdminPg.replace("%OTAS%", T_RUNNING);
    } else {
      AdminPg.replace("%OTAS%", T_STOPPED);
    }

    // ota name
    msg = H_OTA_NAME;
    msg.replace("%otan%", ControllerData->get_ota_name());
    AdminPg.replace("%OTAN%", msg);
    AdminPg.replace("%BOTAN%", H_OTA_NAMEBTN);

    // ota password
    msg = H_OTA_PASSWORD;
    msg.replace("%otap%", ControllerData->get_ota_password());
    AdminPg.replace("%OTAP%", msg);
    AdminPg.replace("%BOTAP%", H_OTA_PASSWORDBTN);

    // ota sid
    msg = H_OTAID;
    msg.replace("%otaid%", ControllerData->get_ota_id());
    AdminPg.replace("%OTAID%", msg);
    AdminPg.replace("%BOTAID%", H_OTA_IDBTN);

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }
  debug_server_print(T_ADMIN2);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}


// ----------------------------------------------------------------------
// Handler for get /admin3
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::getadmin3(void) {
  // get admin pg 3 and send to client
  String AdminPg;

  AdminPg.reserve(6900);  // 306-02 6672

  debug_server_println(T_ADMIN3);

  if (!check_access()) {
    return;
  }

  // post handler
  if (admin3_type == PosT) {
    String msg;

    debug_server_println(T_POST);
    checkreboot();

    // position set
    msg = mserver->arg("setpos");
    if (msg != "") {
      // get new position value from text field pos
      // set new position only: this is NOT a move
      String fp = mserver->arg("pos");
      if (fp != "") {
        long tp = 0;
        long maxp = ControllerData->get_maxstep();
        tp = fp.toInt();
        // range check the new position
        tp = (tp < 0) ? 0 : tp;
        ftargetPosition = (tp > maxp) ? maxp : tp;
        driverboard->setposition(ftargetPosition);
        ControllerData->set_fposition(ftargetPosition);
        goto Get_Handler;
      }
    }

    // position goto
    msg = mserver->arg("gopos");
    if (msg != "") {
      // get new position value from text field pos
      // set target position: this is a move
      String fp = mserver->arg("pos");
      if (fp != "") {
        long tp = 0;
        long maxp = ControllerData->get_maxstep();
        tp = fp.toInt();
        // range check the new position
        tp = (tp < 0) ? 0 : tp;
        ftargetPosition = (tp > maxp) ? maxp : tp;
        isMoving = 1;
        goto Get_Handler;
      }
    }

    // maxsteps
    msg = mserver->arg("setmax");
    if (msg != "") {
      String newmaxpos = mserver->arg("max");
      if (newmaxpos != "") {
        long tp = 0;
        long cpos = driverboard->getposition();
        long maxp = ControllerData->get_maxstep();
        tp = newmaxpos.toInt();
        tp = (tp < cpos) ? cpos + 1 : tp;
        tp = (tp < FOCUSERLOWERLIMIT) ? FOCUSERLOWERLIMIT : tp;
        tp = (tp > maxp) ? maxp : tp;
        ControllerData->set_maxstep(tp);
      }
      goto Get_Handler;
    }

    // coil power enable cpst on off
    msg = mserver->arg("cpst");
    if (msg != "") {
      if (msg == "on") {
        ControllerData->set_coilpower_enable(V_ENABLED);
        driverboard->enablemotor();
      } else if (msg = "off") {
        ControllerData->set_coilpower_enable(V_NOTENABLED);
        driverboard->releasemotor();
      }
      goto Get_Handler;
    }

    // DelayAfterMove on, off
    msg = mserver->arg("damen");
    if (msg != "") {
      if (msg == "off") {
        ControllerData->set_delayaftermove_enable(V_NOTENABLED);
      } else if (msg == "on") {
        ControllerData->set_delayaftermove_enable(V_ENABLED);
      }
      goto Get_Handler;
    }

    // DelayAfterMove Time Interval 0-250ms
    msg = mserver->arg("setdamt");
    if (msg != "") {
      String tdam = mserver->arg("damt");
      if (tdam != "") {
        int damtime = tdam.toInt();
        damtime = (damtime < 0) ? 0 : damtime;
        damtime = (damtime > 250) ? 250 : damtime;
        ControllerData->set_delayaftermove_time(damtime);
      }
      goto Get_Handler;
    }

    // update motorspeed
    msg = mserver->arg("ms");
    if (msg != "") {
      int mspd = 0;
      mspd = msg.toInt();
      mspd = (mspd < SLOW) ? SLOW : mspd;
      mspd = (mspd > FAST) ? FAST : mspd;
      ControllerData->set_motorspeed(mspd);
      goto Get_Handler;
    }

    // motor speed delay 1000-14000
    msg = mserver->arg("setmsd");
    if (msg != "") {
      String msd = mserver->arg("msd");
      if (msd != "") {
        unsigned long newdelay = 4000;
        newdelay = msd.toInt();
        newdelay = (newdelay < 1000) ? 1000 : newdelay;
        newdelay = (newdelay > 14000) ? 14000 : newdelay;
        ControllerData->set_brdmsdelay(newdelay);
      }
      goto Get_Handler;
    }

    // park enable on, off
    msg = mserver->arg("park");
    if (msg != "") {
      if (msg == "on") {
        ControllerData->set_park_enable(V_ENABLED);
      } else if (msg = "off") {
        ControllerData->set_park_enable(V_NOTENABLED);
      }
      goto Get_Handler;
    }

    // park time in seconds
    msg = mserver->arg("setpat");
    if (msg != "") {
      String ti = mserver->arg("pat");
      int pt = ti.toInt();
      // range check 30s to 300s (5m)
      pt = (pt < 30) ? 30 : pt;
      pt = (pt > 300) ? 300 : pt;
      ControllerData->set_parktime(pt);
      // update park_maxcount
      portENTER_CRITICAL(&parkMux);
      park_maxcount = pt * 10;
      portEXIT_CRITICAL(&parkMux);
      goto Get_Handler;
    }

    // reverse direction rdst on off
    msg = mserver->arg("rdst");
    if (msg != "") {
      if (msg == "on") {
        ControllerData->set_reverse_enable(V_ENABLED);
      } else if (msg = "off") {
        ControllerData->set_reverse_enable(V_NOTENABLED);
      }
      goto Get_Handler;
    }

    // stepmode
    msg = mserver->arg("stepm");
    if (msg != "") {
      int stepm = msg.toInt();
      int smtmp = ControllerData->get_brdmaxstepmode();
      stepm = (stepm < STEP1) ? STEP1 : stepm;
      stepm = (stepm > smtmp) ? smtmp : stepm;
      driverboard->setstepmode(stepm);
      goto Get_Handler;
    }

    // stepsize enable/disable
    msg = mserver->arg("sss");
    if (msg != "") {
      if (msg == "on") {
        ControllerData->set_stepsize_enable(V_RUNNING);
      } else if (msg == "off") {
        ControllerData->set_stepsize_enable(V_STOPPED);
      }
      goto Get_Handler;
    }

    // stepsize value
    msg = mserver->arg("setss");
    if (msg != "") {
      String st = mserver->arg("ssv");
      float steps = st.toFloat();
      ControllerData->set_stepsize(steps);
      goto Get_Handler;
    }
  }  // end of post handler

Get_Handler:

  if (SPIFFS.exists("/admin3.html")) {
    String msg;
    String tmp;

    File file = SPIFFS.open("/admin3.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    // position
    msg = H_POS_VALUE;
    msg.replace("%posval%", String(driverboard->getposition()));
    AdminPg.replace("%POS%", msg);
    AdminPg.replace("%BPOS%", H_POS_SETBTN);
    AdminPg.replace("%MOV%", H_POS_GOTOBTN);

    // maxsteps
    msg = H_MAX_VALUE;
    msg.replace("%maxval%", String(ControllerData->get_maxstep()));
    AdminPg.replace("%MAX%", msg);
    AdminPg.replace("%BMAX%", H_MAX_VALUEBTN);

    // Coil Power, State %CPS%, Enable-Disable %BCPS%
    if (ControllerData->get_coilpower_enable() == V_ENABLED) {
      AdminPg.replace("%BCPS%", H_DISABLECP);
      AdminPg.replace("%CPS%", T_ENABLED);
    } else {
      AdminPg.replace("%BCPS%", H_ENABLECP);
      AdminPg.replace("%CPS%", T_DISABLED);
    }

    // delay after move enable/disable, use button
    if (ControllerData->get_delayaftermove_enable() != V_ENABLED) {
      AdminPg.replace("%BDAMS%", H_DAMENABLE);
      AdminPg.replace("%DAMS%", T_DISABLED);
    } else {
      AdminPg.replace("%BDAMS%", H_DAMDISABLE);
      AdminPg.replace("%DAMS%", T_ENABLED);
    }

    // delay after move interval time %DAMV% (10-250ms), button %BDAMV%
    tmp = String(ControllerData->get_delayaftermove_time());
    msg = H_DELAYAFTERMOVEVAL;
    msg.replace("%damnum%", tmp);
    AdminPg.replace("%DAMV%", msg);
    AdminPg.replace("%BDAMV%", H_DELAYAFTERMOVEBTN);

    // motorspeed
    String msbuffer = String(ControllerData->get_motorspeed());
    switch (ControllerData->get_motorspeed()) {
      case 0:
        msbuffer = H_MSSLOWCHECKED;
        msbuffer = msbuffer + H_MSMEDUNCHECKED;
        msbuffer = msbuffer + H_MSFASTUNCHECKED;
        break;
      case 1:
        msbuffer = H_MSSLOWUNCHECKED;
        msbuffer = msbuffer + H_MSMEDCHECKED;
        msbuffer = msbuffer + H_MSFASTUNCHECKED;
        break;
      case 2:
        msbuffer = H_MSSLOWUNCHECKED;
        msbuffer = msbuffer + H_MSMEDUNCHECKED;
        msbuffer = msbuffer + H_MSFASTCHECKED;
        break;
      default:
        msbuffer = H_MSSLOWUNCHECKED;
        msbuffer = msbuffer + H_MSMEDUNCHECKED;
        msbuffer = msbuffer + H_MSFASTCHECKED;
        break;
    }
    AdminPg.replace("%MSF%", H_MS_FORM);
    AdminPg.replace("%MS%", msbuffer);
    AdminPg.replace("%BMSF%", H_MS_FORM_BUTTON);

    // motor speed delay value %MSD%  button %BMSD%
    tmp = String(ControllerData->get_brdmsdelay());
    msg = H_MOTORSPEEDDELAYVAL;
    msg.replace("%msdnum%", tmp);
    AdminPg.replace("%MSD%", msg);
    AdminPg.replace("%BMSD%", H_MOTORSPEEDDELAYBTN);

    // Park, State %PTS%, button %BPTS%
    if (ControllerData->get_park_enable() == V_ENABLED) {
      AdminPg.replace("%BPTS%", H_DISABLEPARK);
      AdminPg.replace("%PTS%", T_ENABLED);
    } else {
      AdminPg.replace("%BPTS%", H_ENABLEPARK);
      AdminPg.replace("%PTS%", T_DISABLED);
    }

    // Park status %PAS%
    if (_parkstate == false) {
      AdminPg.replace("%PAS%", T_NOTPARKED);
    } else {
      AdminPg.replace("%PAS%", T_PARKED);
    }

    // Park, Timeout in seconds (0-600) input %PTT% button %BPTT%
    msg = H_TIMEOUTPARKVAL;
    msg.replace("%panum%", String(ControllerData->get_parktime()));
    AdminPg.replace("%PTT%", msg);
    AdminPg.replace("%BPTT%", H_TIMEOUTPARKBTN);

    // Reverse Direction State %RDS%, Enable-Disable %BRDS%
    if (ControllerData->get_reverse_enable() == V_ENABLED) {
      AdminPg.replace("%BRDS%", H_DISABLERD);
      AdminPg.replace("%RDS%", T_ENABLED);
    } else {
      AdminPg.replace("%BRDS%", H_ENABLERD);
      AdminPg.replace("%RDS%", T_DISABLED);
    }

    // step mode
    String smbuffer;
    smbuffer.reserve(600);  // 306-02
    smbuffer = STEPMODEFRM;
    int sm = ControllerData->get_brdstepmode();
    switch (sm) {
      case 1:
        smbuffer = smbuffer + STEPMODE1Y;
        smbuffer = smbuffer + STEPMODE2N;
        smbuffer = smbuffer + STEPMODE4N;
        smbuffer = smbuffer + STEPMODE8N;
        smbuffer = smbuffer + STEPMODE16N;
        smbuffer = smbuffer + STEPMODE32N;
        smbuffer = smbuffer + STEPMODE64N;
        smbuffer = smbuffer + STEPMODE128N;
        smbuffer = smbuffer + STEPMODE256N;
        break;
      case 2:
        smbuffer += STEPMODE1N;
        smbuffer += STEPMODE2Y;
        smbuffer = smbuffer + STEPMODE4N;
        smbuffer = smbuffer + STEPMODE8N;
        smbuffer = smbuffer + STEPMODE16N;
        smbuffer = smbuffer + STEPMODE32N;
        smbuffer = smbuffer + STEPMODE64N;
        smbuffer = smbuffer + STEPMODE128N;
        smbuffer = smbuffer + STEPMODE256N;
        break;
      case 4:
        smbuffer += STEPMODE1N;
        smbuffer += STEPMODE2N;
        smbuffer = smbuffer + STEPMODE4Y;
        smbuffer = smbuffer + STEPMODE8N;
        smbuffer = smbuffer + STEPMODE16N;
        smbuffer = smbuffer + STEPMODE32N;
        smbuffer = smbuffer + STEPMODE64N;
        smbuffer = smbuffer + STEPMODE128N;
        smbuffer = smbuffer + STEPMODE256N;
        break;
      case 8:
        smbuffer += STEPMODE1N;
        smbuffer += STEPMODE2N;
        smbuffer = smbuffer + STEPMODE4N;
        smbuffer = smbuffer + STEPMODE8Y;
        smbuffer = smbuffer + STEPMODE16N;
        smbuffer = smbuffer + STEPMODE32N;
        smbuffer = smbuffer + STEPMODE64N;
        smbuffer = smbuffer + STEPMODE128N;
        smbuffer = smbuffer + STEPMODE256N;
        break;
      case 16:
        smbuffer += STEPMODE1N;
        smbuffer += STEPMODE2N;
        smbuffer = smbuffer + STEPMODE4N;
        smbuffer = smbuffer + STEPMODE8N;
        smbuffer = smbuffer + STEPMODE16Y;
        smbuffer = smbuffer + STEPMODE32N;
        smbuffer = smbuffer + STEPMODE64N;
        smbuffer = smbuffer + STEPMODE128N;
        smbuffer = smbuffer + STEPMODE256N;
        break;
      case 32:
        smbuffer += STEPMODE1N;
        smbuffer += STEPMODE2N;
        smbuffer = smbuffer + STEPMODE4N;
        smbuffer = smbuffer + STEPMODE8N;
        smbuffer = smbuffer + STEPMODE16N;
        smbuffer = smbuffer + STEPMODE32Y;
        smbuffer = smbuffer + STEPMODE64N;
        smbuffer = smbuffer + STEPMODE128N;
        smbuffer = smbuffer + STEPMODE256N;
        break;
      case 64:
        smbuffer += STEPMODE1N;
        smbuffer += STEPMODE2N;
        smbuffer = smbuffer + STEPMODE4N;
        smbuffer = smbuffer + STEPMODE8N;
        smbuffer = smbuffer + STEPMODE16N;
        smbuffer = smbuffer + STEPMODE32N;
        smbuffer = smbuffer + STEPMODE64Y;
        smbuffer = smbuffer + STEPMODE128N;
        smbuffer = smbuffer + STEPMODE256N;
        break;
      case 128:
        smbuffer += STEPMODE1N;
        smbuffer += STEPMODE2N;
        smbuffer = smbuffer + STEPMODE4N;
        smbuffer = smbuffer + STEPMODE8N;
        smbuffer = smbuffer + STEPMODE16N;
        smbuffer = smbuffer + STEPMODE32N;
        smbuffer = smbuffer + STEPMODE64N;
        smbuffer = smbuffer + STEPMODE128Y;
        smbuffer = smbuffer + STEPMODE256N;
        break;
      case 256:
        smbuffer += STEPMODE1N;
        smbuffer += STEPMODE2N;
        smbuffer = smbuffer + STEPMODE4N;
        smbuffer = smbuffer + STEPMODE8N;
        smbuffer = smbuffer + STEPMODE16N;
        smbuffer = smbuffer + STEPMODE32N;
        smbuffer = smbuffer + STEPMODE64N;
        smbuffer = smbuffer + STEPMODE128N;
        smbuffer = smbuffer + STEPMODE256Y;
        break;
    }
    // <tr><td style="color: #%STC%"><b>STEP MODE</b> [%SMV%]</td><td> %SM% </td><td> %BSM% </td></tr>
    AdminPg.replace("%SMV%", String(sm));
    AdminPg.replace("%SM%", smbuffer);
    AdminPg.replace("%BSM%", STEPMODEBTN);

    // Step size enable state
    if (ControllerData->get_stepsize_enable()) {
      AdminPg.replace("%BSSE%", String(H_DISABLESTEPSZSTATE));
      AdminPg.replace("%SSE%", T_ENABLED);
    } else {
      AdminPg.replace("%BSSE%", String(H_ENABLESTEPSZSTATE));
      AdminPg.replace("%SSE%", T_DISABLED);
    }

    // step size value %SS% button %BSS%
    msg = H_STEPSIZEVAL;
    msg.replace("%ssnum%", String(ControllerData->get_stepsize()));
    AdminPg.replace("%SS%", msg);
    AdminPg.replace("%BSS%", H_STEPSIZEBTN);

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }

  debug_server_print(T_ADMIN3);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}


// ----------------------------------------------------------------------
// Handler for /admin4
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::getadmin4(void) {
  String AdminPg;
  String msg;

  AdminPg.reserve(3600);  // 306-01    3334

  debug_server_println(T_ADMIN4);

  if (!check_access()) {
    return;
  }

  // post handler
  if (admin4_type == PosT) {
    debug_server_println(T_POST);
    checkreboot();

    // backlash in enable/disable
    msg = mserver->arg("blinst");
    if (msg != "") {
      if (msg == "on") {
        ControllerData->set_backlash_in_enable(V_ENABLED);
      } else if (msg == "off") {
        ControllerData->set_backlash_in_enable(V_NOTENABLED);
      }
      goto Get_Handler;
    }

    // backlash out enable/disable
    msg = mserver->arg("bloutst");
    if (msg != "") {
      if (msg == "on") {
        ControllerData->set_backlash_out_enable(V_ENABLED);
      } else if (msg == "off") {
        ControllerData->set_backlash_out_enable(V_NOTENABLED);
      }
      goto Get_Handler;
    }

    // backlash in steps, setblis, blis,
    msg = mserver->arg("setblis");
    if (msg != "") {
      String st = mserver->arg("blis");
      byte steps = st.toInt();  // no need to test for < 0 and > 255 as it is a byte value
      ControllerData->set_backlashsteps_in(steps);
      goto Get_Handler;
    }

    // backlash out steps, setbos, bos
    msg = mserver->arg("setblos");
    if (msg != "") {
      String st = mserver->arg("blos");
      byte steps = st.toInt();
      ControllerData->set_backlashsteps_out(steps);
      goto Get_Handler;
    }

  }  // end of post handler

Get_Handler:

  if (SPIFFS.exists("/admin4.html")) {
    File file = SPIFFS.open("/admin4.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    if (ControllerData->get_backlash_in_enable()) {
      AdminPg.replace("%BBLIS%", String(H_DISABLEBLINSTATE));
      AdminPg.replace("%BLIS%", T_ENABLED);
    } else {
      AdminPg.replace("%BBLIS%", String(H_ENABLEBLINSTATE));
      AdminPg.replace("%BLIS%", T_DISABLED);
    }

    msg = H_BLINSTEPS;
    msg.replace("%blinum%", String(ControllerData->get_backlashsteps_in()));
    AdminPg.replace("%BBLIN%", msg);

    if (ControllerData->get_backlash_out_enable()) {
      AdminPg.replace("%BBLOS%", String(H_DISABLEBLOUTSTATE));
      AdminPg.replace("%BLOS%", T_ENABLED);
    } else {
      AdminPg.replace("%BBLOS%", String(H_ENABLEBLOUTSTATE));
      AdminPg.replace("%BLOS%", T_DISABLED);
    }

    msg = H_BLOUTSTEPS;
    msg.replace("%blonum%", String(ControllerData->get_backlashsteps_out()));
    AdminPg.replace("%BBLON%", msg);

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }

  debug_server_print(T_ADMIN4);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}


// ----------------------------------------------------------------------
// Handler for /admin5
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::getadmin5(void) {
  String AdminPg;
  String msg;

  AdminPg.reserve(4600);  // 306-02    4402

  debug_server_println(T_ADMIN5);

  if (!check_access()) {
    return;
  }

  // post handler
  if (admin5_type == PosT) {
    debug_server_println(T_POST);
    checkreboot();

    // HOME POSITION SWITCH hpsw enable state on off
    msg = mserver->arg("hpswst");
    if (msg != "") {
      if ((msg == "on") || (msg == "off")) {
        // is hpsw pin available for this board
        if (ControllerData->get_brdhpswpin() == -1) {
          debug_server_print(T_HPSW);
          debug_server_println(T_NOTSUPPORTED);
        } else {
          // hpsw pin is available
          if (msg == "on") {
            // enable hpsw
            ControllerData->set_hpswitch_enable(V_ENABLED);
          } else if (msg == "off") {
            // disable hpsw
            ControllerData->set_hpswitch_enable(V_NOTENABLED);
          }
          // now call driverboard to init the hpsw
          if (driverboard->init_hpsw() == false) {
            debug_server_print(T_HPSW);
            debug_server_print(T_ERROR);
            debug_server_println(T_INIT);
          }
        }
      }  // if( msg == "on" || msg == "off")
      goto Get_Handler;
    }

    // home position switch messages, on, off
    msg = mserver->arg("hpmsgs");
    if (msg != "") {
      if (msg == "on") {
        ControllerData->set_hpswmsg_enable(V_ENABLED);
      } else if (msg == "off") {
        ControllerData->set_hpswmsg_enable(V_NOTENABLED);
      }
      goto Get_Handler;
    }

    // tmc2209 stall guard
    // void set_stallguard_state(tmc2209stallguard);
    msg = mserver->arg("stg");
    if (msg != "") {
      String st = mserver->arg("stgs");
      int sg = 0;
      sg = st.toInt();
      if (sg == 0) {
        ControllerData->set_stallguard_state(Use_Stallguard);
        debug_server_println(T_STALLGUARD);
      } else if (sg == 1) {
        ControllerData->set_stallguard_state(Use_Physical_Switch);
        debug_server_println(T_PHYSICALSWITCH);
      } else if (sg == 2) {
        ControllerData->set_stallguard_state(Use_None);
        debug_server_println(T_NONE);
      } else {
        // error
        ControllerData->set_stallguard_state(Use_None);
      }
      // now call driverboard to init the hpsw after changing tmc2209 mode
      if (driverboard->init_hpsw() == false) {
        debug_server_print(T_HPSW);
        debug_server_print(T_ERROR);
        debug_server_println(T_INIT);
      }
      goto Get_Handler;
    }

    // stall guard value
    msg = mserver->arg("setsgv");
    if (msg != "") {
      String dom = mserver->arg("sgv");
      if (dom != "") {
        int sgval = dom.toInt();
        sgval = (sgval < 0) ? 0 : sgval;
        sgval = (sgval > 255) ? 255 : sgval;
        sgval = sgval & 0xff;
        driverboard->setstallguardvalue((byte)sgval);
      }
      goto Get_Handler;
    }

    // tmc2209 current
    msg = mserver->arg("set09c");
    if (msg != "") {
      String dom = mserver->arg("2209c");
      if (dom != "") {
        int val = dom.toInt();
        driverboard->settmc2209current(val);
      }
      goto Get_Handler;
    }

    // tmc2225 current
    msg = mserver->arg("set25c");
    if (msg != "") {
      String dom = mserver->arg("2225c");
      if (dom != "") {
        int val = dom.toInt();
        driverboard->settmc2225current(val);
      }
      goto Get_Handler;
    }
  }  // end of post handler

Get_Handler:

  if (SPIFFS.exists("/admin5.html")) {
    File file = SPIFFS.open("/admin5.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    // HPSW and TMC Currents

    // Home Position Switch, State %HPS%, Enable/Disable %BHPS%
    if (ControllerData->get_hpswitch_enable() == V_ENABLED) {
      AdminPg.replace("%BHPS%", H_DISABLEHPSW);
      AdminPg.replace("%HPS%", T_ENABLED);
    } else {
      AdminPg.replace("%BHPS%", H_ENABLEHPSW);
      AdminPg.replace("%HPS%", T_DISABLED);
    }

    // Home Position Switch, Status [Open | Closed] %HPSS%
    bool hpswstatus = !((bool)digitalRead(ControllerData->get_brdhpswpin()));
    if (hpswstatus) {
      AdminPg.replace("%HPSS%", "Closed");
    } else {
      AdminPg.replace("%HPSS%", "Open");
    }

    // Home Position Switch Show Messages, state %HPM%, button %BHPM%
    if (ControllerData->get_hpswmsg_enable() == V_ENABLED) {
      AdminPg.replace("%BHPM%", H_DISABLEHPSWMSGS);
      AdminPg.replace("%HPM%", T_ENABLED);
    } else {
      AdminPg.replace("%BHPM%", H_ENABLEHPSWMSGS);
      AdminPg.replace("%HPM%", T_DISABLED);
    }

    // TMC2209 Stall guard
    String sgbuffer0;  // stall guard
    String sgbuffer1;  // physical switch
    String sgbuffer2;  // none
    tmc2209stallguard stallguard_state = ControllerData->get_stallguard_state();
    switch (stallguard_state) {
      case Use_Stallguard:
        sgbuffer0 = H_SG_Use_Stallguard_CHECKED;
        sgbuffer1 += H_SG_Use_Physical_Switch_UNCHECKED;
        sgbuffer2 += H_SG_Use_None_UNCHECKED;
        break;
      case Use_Physical_Switch:
        sgbuffer0 = H_SG_Use_Stallguard_UNCHECKED;
        sgbuffer1 += H_SG_Use_Physical_Switch_CHECKED;
        sgbuffer2 += H_SG_Use_None_UNCHECKED;
        break;
      case Use_None:
        sgbuffer0 = H_SG_Use_Stallguard_UNCHECKED;
        sgbuffer1 += H_SG_Use_Physical_Switch_UNCHECKED;
        sgbuffer2 += H_SG_Use_None_CHECKED;
        break;
      default:
        sgbuffer0 = H_SG_Use_Stallguard_UNCHECKED;
        sgbuffer1 += H_SG_Use_Physical_Switch_UNCHECKED;
        sgbuffer2 += H_SG_Use_None_CHECKED;
        break;
    }
    AdminPg.replace("%SGF%", H_SG_FORM);
    AdminPg.replace("%SG0%", sgbuffer0);
    AdminPg.replace("%SG1%", sgbuffer1);
    AdminPg.replace("%SG2%", sgbuffer2);
    AdminPg.replace("%BSGF%", H_SG_FORM_BUTTON);

    // stall guard value
    msg = H_SG_VALUE;
    msg.replace("%sgval%", String(ControllerData->get_stallguard_value()));
    AdminPg.replace("%SGV%", msg);
    AdminPg.replace("%BSGV%", H_SG_VALUEBTN);

    // tmc2209 current
    msg = H_TMC2209CURRENT;
    msg.replace("%c09%", String(ControllerData->get_tmc2209current()));
    AdminPg.replace("%09I%", msg);
    AdminPg.replace("%B09C%", H_TMC2209CURRENTBTN);

    // tmc2225 current
    msg = H_TMC2225CURRENT;
    msg.replace("%c25%", String(ControllerData->get_tmc2225current()));
    AdminPg.replace("%25I%", msg);
    AdminPg.replace("%B25C%", H_TMC2225CURRENTBTN);

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }

  debug_server_print(T_ADMIN5);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}


// ----------------------------------------------------------------------
// Handler for /admin6
// Leds,PB,Joysticks
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::getadmin6(void) {
  String AdminPg;
  String msg;

  AdminPg.reserve(4500);  // 306-01    4253

  debug_server_println(T_ADMIN6);

  if (!check_access()) {
    return;
  }

  // post handler
  if (admin6_type == PosT) {
    debug_server_println(T_POST);
    checkreboot();

    // LEDS ENABLE/DISABLE leds, on, off
    msg = mserver->arg("ledst");
    if (msg != "") {
      if (msg == "on") {
        driverboard->set_leds(true);
      } else if (msg == "off") {
        driverboard->set_leds(false);
      }
      goto Get_Handler;
    }

    // LED MODE, move or pulse
    msg = mserver->arg("lmode");
    if (msg != "") {
      if (msg == "mov") {
        ControllerData->set_inoutled_mode(LEDMOVE);
      }
      if (msg == "pul") {
        ControllerData->set_inoutled_mode(LEDPULSE);
      }
      goto Get_Handler;
    }

    // push buttons enable/disable
    msg = mserver->arg("pbst");
    if (msg != "") {
      if (msg == "on") {
        // if current state is disabled then enable
        if (driverboard->set_pushbuttons(true) != true) {
          debug_server_print(T_PB);
          debug_server_print(T_ERROR);
          debug_server_println(T_SET);
        }
      } else if (msg == "off") {
        if (driverboard->set_pushbuttons(false) != true) {
          debug_server_print(T_PB);
          debug_server_print(T_ERROR);
          debug_server_println(T_SET);
        }
      }
      goto Get_Handler;
    }

    // push button steps
    msg = mserver->arg("setpbn");
    if (msg != "") {
      String st = mserver->arg("pbn");
      int pt = st.toInt();
      ControllerData->set_pushbutton_steps(pt);
      goto Get_Handler;
    }

    // joystick1 enable/disable
    msg = mserver->arg("j1st");
    if (msg != "") {
      if (msg == "on") {
        if (driverboard->set_joystick1(true) != true) {
          debug_server_print(T_J1);
          debug_server_print(T_ERROR);
          debug_server_println(T_SET);
        }
      } else if (msg == "off") {
        if (driverboard->set_joystick1(false) != true) {
          debug_server_print(T_J1);
          debug_server_print(T_ERROR);
          debug_server_println(T_SET);
        }
      }
      goto Get_Handler;
    }

    // joystick2 enable/disable
    msg = mserver->arg("j2st");
    if (msg != "") {
      if (msg == "on") {
        if (driverboard->set_joystick2(true) != true) {
          debug_server_print(T_J2);
          debug_server_print(T_ERROR);
          debug_server_println(T_SET);
        }
      } else if (msg == "off") {
        if (driverboard->set_joystick2(false) != true) {
          debug_server_print(T_J2);
          debug_server_print(T_ERROR);
          debug_server_println(T_SET);
        }
      }
      goto Get_Handler;
    }
  }  // end of post handler

Get_Handler:

  if (SPIFFS.exists("/admin6.html")) {
    File file = SPIFFS.open("/admin6.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    // In-out LEDs Enable/Disable, State %IOS%, Button %BIOS%
    if (ControllerData->get_inoutled_enable() == V_ENABLED) {
      AdminPg.replace("%BIOS%", H_DISABLELED);
      AdminPg.replace("%IOS%", T_ENABLED);
    } else {
      AdminPg.replace("%BIOS%", H_ENABLELED);
      AdminPg.replace("%IOS%", T_DISABLED);
    }

    // In-out LEDs Mode, State %IOM%, Button %BIOM%
    if (ControllerData->get_inoutled_mode() == LEDPULSE) {
      AdminPg.replace("%BIOM%", H_LEDMODEMOVE);
      AdminPg.replace("%IOM%", "Pulse");
    } else {
      AdminPg.replace("%BIOM%", H_LEDMODEPULSE);
      AdminPg.replace("%IOM%", "Move");
    }

    // IR remote state
    if (irremote_status) {
      AdminPg.replace("%IRS%", T_RUNNING);
    } else {
      AdminPg.replace("%IRS%", T_STOPPED);
    }

    // Joystick1 state %J1S% button %BJ1S%
    if (ControllerData->get_joystick1_enable() == V_ENABLED) {
      AdminPg.replace("%BJ1S%", H_DISABLEJ1);
      AdminPg.replace("%J1S%", T_ENABLED);
    } else {
      AdminPg.replace("%BJ1S%", H_ENABLEJ1);
      AdminPg.replace("%J1S%", T_DISABLED);
    }

    // Joystick2 state %J2S% button %BJ2S%
    if (ControllerData->get_joystick2_enable() == V_ENABLED) {
      AdminPg.replace("%BJ2S%", H_DISABLEJ2);
      AdminPg.replace("%J2S%", T_ENABLED);
    } else {
      AdminPg.replace("%BJ2S%", H_ENABLEJ2);
      AdminPg.replace("%J2S%", T_DISABLED);
    }

    //  Push Buttons state, State %PBS%, Enable-disable %BPBS%
    if (ControllerData->get_pushbutton_enable() == V_ENABLED) {
      AdminPg.replace("%BPBS%", H_DISABLEPB);
      AdminPg.replace("%PBS%", T_ENABLED);
    } else {
      AdminPg.replace("%BPBS%", H_ENABLEPB);
      AdminPg.replace("%PBS%", T_DISABLED);
    }

    // Push Buttons value Input field Steps %PBI% button %BPBI%
    msg = H_PBSTEPS;
    msg.replace("%pbnum%", String(ControllerData->get_pushbutton_steps()));
    AdminPg.replace("%PBI%", msg);
    AdminPg.replace("%BPBI%", H_PBSTEPSFORMED);

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }

  debug_server_print(T_ADMIN6);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}


// ----------------------------------------------------------------------
// Handler for /admin7
// DISPLAY
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::getadmin7(void) {
  String AdminPg;
  String msg;

  AdminPg.reserve(4400);  // 306-01  4180

  debug_server_println(T_ADMIN7);

  if (!check_access()) {
    return;
  }

  // post handler
  if (admin7_type == PosT) {
    debug_server_println(T_POST);
    checkreboot();

    // display state enable disable
    msg = mserver->arg("en");
    if (msg != "") {
      if (msg == "enable") {
        ControllerData->set_display_enable(V_ENABLED);
      } else if (msg == "disable") {
        ControllerData->set_display_enable(V_NOTENABLED);
        // check if display is running, if so stop the display
        if (display_status == V_RUNNING) {
          // clear display
          display_clear();
          display_stop();
        }
      }
      goto Get_Handler;
    }

    // display state start
    msg = mserver->arg("st");
    if (msg != "") {
      if (msg == "start") {
        if (ControllerData->get_display_enable() == V_ENABLED) {
          if (display_start() != true) {
            debug_server_print(T_DISPLAY);
            debug_server_println(T_ERRSTART);
          }
        }
      } else if (msg == "stop") {
        display_stop();
      }
      goto Get_Handler;
    }

    // display update position when moving state enable disable
    msg = mserver->arg("up");
    if (msg != "") {
      if (msg == "enable") {
        ControllerData->set_display_updateonmove(V_ENABLED);
      } else if (msg == "disable") {
        ControllerData->set_display_updateonmove(V_NOTENABLED);
      }
      goto Get_Handler;
    }

    // display page time update
    msg = mserver->arg("setpt");
    if (msg != "") {
      String tp = mserver->arg("pt");
      if (tp != "") {
        int pgtime = tp.toInt();
        pgtime = (pgtime < V_DISPLAYPAGETIMEMIN) ? V_DISPLAYPAGETIMEMIN : pgtime;
        pgtime = (pgtime > V_DISPLAYPAGETIMEMAX) ? V_DISPLAYPAGETIMEMAX : pgtime;
        ControllerData->set_display_pagetime(pgtime);
        // update display_maxcount
        portENTER_CRITICAL(&displaytimeMux);
        display_maxcount = pgtime * 10;
        portEXIT_CRITICAL(&displaytimeMux);
      }
      goto Get_Handler;
    }

    // display page option, pg1 is at [7], 11111111b
    String pageoption = ControllerData->get_display_pageoption();
    if (pageoption == "") {
      pageoption = "00000001";
      ControllerData->set_display_pageoption(pageoption);
    }
    // make sure there are 8 digits, pad leading 0's if necessary
    while (pageoption.length() < 8) {
      pageoption = '0' + pageoption;
    }

    // when a form is submitted, only checkboxes which are currently checked are submitted to the server,
    // with the value on by default
    msg = mserver->arg("spo");
    if (msg != "") {
      msg = mserver->arg("pg1");
      if (msg == "on") {
        pageoption[7] = '1';
      } else {
        pageoption[7] = '0';
      }

      msg = mserver->arg("pg2");
      if (msg == "on") {
        pageoption[6] = '1';
      } else {
        pageoption[6] = '0';
      }

      msg = mserver->arg("pg3");
      if (msg == "on") {
        pageoption[5] = '1';
      } else {
        pageoption[5] = '0';
      }

      msg = mserver->arg("pg4");
      if (msg == "on") {
        pageoption[4] = '1';
      } else {
        pageoption[4] = '0';
      }

      msg = mserver->arg("pg5");
      if (msg == "on") {
        pageoption[3] = '1';
      } else {
        pageoption[3] = '0';
      }

      msg = mserver->arg("pg6");
      if (msg == "on") {
        pageoption[2] = '1';
      } else {
        pageoption[2] = '0';
      }

      msg = mserver->arg("pg7");
      if (msg == "on") {
        pageoption[1] = '1';
      } else {
        pageoption[1] = '0';
      }

      msg = mserver->arg("pg8");
      if (msg == "on") {
        pageoption[0] = '1';
      } else {
        pageoption[0] = '0';
      }
      ControllerData->set_display_pageoption(pageoption);
      goto Get_Handler;
    }
  }  // end of post handler

Get_Handler:

  if (SPIFFS.exists("/admin7.html")) {
    File file = SPIFFS.open("/admin7.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    // DISPLAY State enable %DST% Button %BDST%  Enabled NotEnabled
    AdminPg.replace("%DSTF%", H_ENABLEDISPLAYFORM);
    if (ControllerData->get_display_enable() != V_ENABLED) {
      AdminPg.replace("%BDSTF%", H_ENABLEDISPLAYBUTTON);
      AdminPg.replace("%DST%", T_DISABLED);
    } else {
      AdminPg.replace("%BDSTF%", H_DISABLEDISPLAYFORM);
      AdminPg.replace("%DST%", T_ENABLED);
    }

    // DISPLAY Start/Stop Form %DSSF% Value %DSS%  Button %BDSSF%
    AdminPg.replace("%DSSF%", H_STARTDISPLAYFORM);
    if (display_status != V_RUNNING) {
      AdminPg.replace("%BDSSF%", H_STARTDISPLAYBUTTON);
      AdminPg.replace("%DSS%", T_STOPPED);
    } else {
      AdminPg.replace("%BDSSF%", H_STOPDISPLAYBUTTON);
      AdminPg.replace("%DSS%", T_RUNNING);
    }

    // DISPLAY Update Position when moving
    // Enable/Disable Form %DSPF% Value %DSPM%  Button %BDSPM%
    AdminPg.replace("%DSPF%", H_UPDATEPOSFORM);
    if (ControllerData->get_display_updateonmove() != V_ENABLED) {
      AdminPg.replace("%BDSPM%", H_UPDATEPOSENABLE);
      AdminPg.replace("%DSPM%", T_DISABLED);
    } else {
      AdminPg.replace("%BDSPM%", H_UPDATEPOSDISABLE);
      AdminPg.replace("%DSPM%", T_ENABLED);
    }

    // form %PTF% page display time value %PTS% input text field %PTI% button %BPTF%
    AdminPg.replace("%PTF%", H_PGTIMEFORM);
    String dpt = String(ControllerData->get_display_pagetime());
    msg = H_PGTIMEINPUT;
    msg.replace("%ptnum%", dpt);
    AdminPg.replace("%PTI%", msg);
    AdminPg.replace("%BPTF%", H_PGTIMEBUTTON);

    // page display options %PDO% %P1% to %P8% %BPDO%
    AdminPg.replace("%PDO%", H_DISPLAYPOFORM);

    // need to get display options and then set each checkbox
    String pageoption = ControllerData->get_display_pageoption();
    // now build the page option html code
    // start with page1, which is right most bit
    // if 0, then unchecked, else if 1 then checked
    // read it in reverse order
    if (pageoption[7] == '0') {
      AdminPg.replace("%P1%", H_PAGE1_UNCHECKED);
    } else {
      AdminPg.replace("%P1%", H_PAGE1_CHECKED);
    }
    if (pageoption[6] == '0') {
      AdminPg.replace("%P2%", H_PAGE2_UNCHECKED);
    } else {
      AdminPg.replace("%P2%", H_PAGE2_CHECKED);
    }
    if (pageoption[5] == '0') {
      AdminPg.replace("%P3%", H_PAGE3_UNCHECKED);
    } else {
      AdminPg.replace("%P3%", H_PAGE3_CHECKED);
    }
    if (pageoption[4] == '0') {
      AdminPg.replace("%P4%", H_PAGE4_UNCHECKED);
    } else {
      AdminPg.replace("%P4%", H_PAGE4_CHECKED);
    }
    if (pageoption[3] == '0') {
      AdminPg.replace("%P5%", H_PAGE5_UNCHECKED);
    } else {
      AdminPg.replace("%P5%", H_PAGE5_CHECKED);
    }
    if (pageoption[2] == '0') {
      AdminPg.replace("%P6%", H_PAGE6_UNCHECKED);
    } else {
      AdminPg.replace("%P6%", H_PAGE6_CHECKED);
    }
    if (pageoption[1] == '0') {
      AdminPg.replace("%P7%", H_PAGE7_UNCHECKED);
    } else {
      AdminPg.replace("%P7%", H_PAGE7_CHECKED);
    }
    if (pageoption[0] == '0') {
      AdminPg.replace("%P8%", H_PAGE8_UNCHECKED);
    } else {
      AdminPg.replace("%P8%", H_PAGE8_CHECKED);
    }
    // insert form submit button for page options
    AdminPg.replace("%BPDO%", H_DISPLAYPOBUTTON);

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }

  debug_server_print(T_ADMIN7);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}


// ----------------------------------------------------------------------
// Handler for /admin8
// TEMP
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::getadmin8(void) {
  // get admin pg 8 and send to client
  String AdminPg;
  String msg;

  AdminPg.reserve(4000);  // 306-01    3705

  debug_server_println(T_ADMIN8);

  if (!check_access()) {
    return;
  }

  // post handler
  if (admin8_type == PosT) {
    debug_server_println(T_POST);
    checkreboot();

    // Temperature Probe ENABLE/DISABLE, tps, enabled. not enabled
    msg = mserver->arg("tps");
    if (msg != "") {
      if (msg == "enable") {
        ControllerData->set_tempprobe_enable(V_ENABLED);
      } else if (msg == "disable") {
        // if running then stop it first
        tempprobe->stop();
        ControllerData->set_tempprobe_enable(V_NOTENABLED);
      }
      goto Get_Handler;
    }

    // PROBE START-STOP
    msg = mserver->arg("tpru");
    if (msg != "") {
      if (msg == "start") {
        if (ControllerData->get_tempprobe_enable() == 1) {
          if (tempprobe->start() != true) {
            debug_server_print(T_TEMPPROBE);
            debug_server_println(T_ERRSTART);
          }
        }
      } else if (msg == "stop") {
        if (tempprobe->get_state()) {
          tempprobe->stop();
        }
      }
      goto Get_Handler;
    }

    // Temperature probe celsius/fahrenheit
    msg = mserver->arg("tm");
    if (msg != "") {
      if (msg == "cel") {
        ControllerData->set_tempmode(V_CELSIUS);
      } else if (msg == "fah") {
        ControllerData->set_tempmode(V_FAHRENHEIT);
      }
      goto Get_Handler;
    }

    // Temperature probe resolution
    msg = mserver->arg("settpr");
    if (msg != "") {
      String st = mserver->arg("tpr");
      int tr = st.toInt();
      tr = (tr < 9) ? 9 : tr;
      tr = (tr > 12) ? 12 : tr;
      ControllerData->set_tempresolution((byte)tr);
      goto Get_Handler;
    }

    // Temperature Compensation Direction
    msg = mserver->arg("tcd");
    if (msg != "") {
      if (msg == "in") {
        ControllerData->set_tcdirection(TC_DIRECTION_IN);
      } else if (msg == "out") {
        ControllerData->set_tcdirection(TC_DIRECTION_OUT);
      }
      goto Get_Handler;
    }

    // Temperature Coefficient
    msg = mserver->arg("settc");
    if (msg != "") {
      String st = mserver->arg("tce");
      int tc = st.toInt();
      ControllerData->set_tempcoefficient(tc);
      goto Get_Handler;
    }
  }  // end of post handler

Get_Handler:

  if (SPIFFS.exists("/admin8.html")) {
    File file = SPIFFS.open("/admin8.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    // Temperature Probe State %TPS% Enable/Disable %BTPS%
    if (ControllerData->get_tempprobe_enable() == V_NOTENABLED) {
      AdminPg.replace("%TPS%", T_DISABLED);
      AdminPg.replace("%BTPS%", H_ENABLETEMPPROBE);
    } else {
      AdminPg.replace("%TPS%", T_ENABLED);
      AdminPg.replace("%BTPS%", H_DISABLETEMPPROBE);
    }

    // Temperature Probe Run State %TPR% Start/Stop %BTPR%
    if (tempprobe->get_state() == V_STOPPED) {
      AdminPg.replace("%TPR%", T_STOPPED);
      AdminPg.replace("%BTPR%", H_STARTTEMPPROBE);
    } else {
      AdminPg.replace("%TPR%", T_RUNNING);
      AdminPg.replace("%BTPR%", H_STOPTEMPPROBE);
    }

    // Temperature Mode %TPM%, %BTPM%
    // Celcius=1, Fahrenheit=0
    if (ControllerData->get_tempmode() == V_CELSIUS) {
      // celsius - Change to Fahrenheit
      AdminPg.replace("%BTPM%", H_TEMPMODEFAHRENHEIT);
      AdminPg.replace("%TPM%", T_CELSIUS);
    } else {
      // Fahrenheit - change to celsius
      AdminPg.replace("%BTPM%", H_TEMPMODECELSIUS);
      AdminPg.replace("%TPM%", T_FAHRENHEIT);
    }

    // Temperature Resolution  %BTR%
    msg = H_TEMPPROBERESOLUTION;
    msg.replace("%tprn%", String(ControllerData->get_tempresolution()));
    AdminPg.replace("%BTR%", msg);

    // Temp Comp Available %TCA%
    if (ControllerData->get_tcavailable() == V_ENABLED) {
      AdminPg.replace("%TCA%", T_ENABLED);
    } else {
      AdminPg.replace("%TCA%", T_DISABLED);
    }

    // Temperature Compensation Direction value %TCD% %BTCD%
    // TC_DIRECTION_IN or TC_DIRECTION_OUT
    if (ControllerData->get_tcdirection() == TC_DIRECTION_IN) {
      AdminPg.replace("%BTCD%", H_TEMPCOMPDIRECTIONOUT);
      AdminPg.replace("%TCD%", T_IN);
    } else {
      AdminPg.replace("%BTCD%", H_TEMPCOMPDIRECTIONIN);
      AdminPg.replace("%TCD%", T_OUT);
    }

    // Temp Comp State %TSC%
    if (ControllerData->get_tempcomp_enable() == V_ENABLED) {
      AdminPg.replace("%TCS%", T_ENABLED);
    } else {
      AdminPg.replace("%TCS%", T_DISABLED);
    }

    // Temp Comp Coefficient %TCC% input field + button
    msg = H_TEMPCOEFFICENT;
    int tc = ControllerData->get_tempcoefficient();
    msg.replace("%tcnum%", String(tc));
    AdminPg.replace("%TCC%", msg);

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }

  debug_server_print(T_ADMIN8);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}


// ----------------------------------------------------------------------
// Handler for get /admin9
// MISC
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::getadmin9(void) {
  // get admin pg 9 and send to client
  String AdminPg;
  String msg;

  AdminPg.reserve(4800);  // 306-05    TODO

  debug_server_println(T_ADMIN9);

  if (!check_access()) {
    return;
  }

  // post handler
  if (admin9_type == PosT) {
    debug_server_println(T_POST);
    checkreboot();

    // devicename
    msg = mserver->arg("setdname");
    if (msg != "") {
      String dom = mserver->arg("dname");
      if (dom != "") {
        ControllerData->set_devicename(dom);
        dom.toCharArray(devicename, dom.length() + 1);
      }
      goto Get_Handler;
    }

    // get file list format
    msg = mserver->arg("listt");
    if (msg != "") {
      String str = mserver->arg("flf");
      if (str != "") {
        if (str == "short") {
          ControllerData->set_filelistformat(LISTSHORT);
        } else if (str == "long") {
          ControllerData->set_filelistformat(LISTLONG);
        }
      }
      goto Get_Handler;
    }

    // get page title color
    msg = mserver->arg("tiset");
    if (msg != "") {
      String str = mserver->arg("tic");
      if (str != "") {
        boolean flag = true;
        // check that all are hex digits 0..9 and a..f
        int len = str.length();
        for (int i = 0; i < len; i++) {
          char ch = str[i];
          if (is_hexdigit(ch) == false) {
            flag = false;
          }
        }
        if (flag) {
          ControllerData->set_wp_titlecolor(str);
          str.toCharArray(titlecolor, str.length() + 1);
        }
      }
      goto Get_Handler;
    }

    // get page sub-title color
    msg = mserver->arg("stset");
    if (msg != "") {
      String str = mserver->arg("stc");
      if (str != "") {
        boolean flag = true;
        // check that all are hex digits 0..9 and a..f
        int len = str.length();
        for (int i = 0; i < len; i++) {
          char ch = str[i];
          if (is_hexdigit(ch) == false) {
            flag = false;
          }
        }
        if (flag) {
          ControllerData->set_wp_subtitlecolor(str);
          str.toCharArray(subtitlecolor, str.length() + 1);
        }
      }
      goto Get_Handler;
    }

    // get header color
    msg = mserver->arg("hcset");
    if (msg != "") {
      String str = mserver->arg("hci");
      if (str != "") {
        boolean flag = true;
        // check that all are hex digits 0..9 and a..f
        int len = str.length();
        for (int i = 0; i < len; i++) {
          char ch = str[i];
          if (is_hexdigit(ch) == false) {
            flag = false;
          }
        }
        if (flag) {
          ControllerData->set_wp_headercolor(str);
          str.toCharArray(headercolor, str.length() + 1);
        }
      }
      goto Get_Handler;
    }

    // get text color
    msg = mserver->arg("tecset");
    if (msg != "") {
      String str = mserver->arg("tci");
      if (str != "") {
        boolean flag = true;
        // check that all are hex digits 0..9 and a..f
        int len = str.length();
        for (int i = 0; i < len; i++) {
          char ch = str[i];
          if (is_hexdigit(ch) == false) {
            flag = false;
          }
        }
        if (flag) {
          ControllerData->set_wp_textcolor(str);
          str.toCharArray(textcolor, str.length() + 1);
        }
      }
      goto Get_Handler;
    }

    // get background color
    msg = mserver->arg("bcset");
    if (msg != "") {
      String str = mserver->arg("bci");
      if (str != "") {
        boolean flag = true;
        // check that all are hex digits 0..9 and a..f
        int len = str.length();
        for (int i = 0; i < len; i++) {
          char ch = str[i];
          if (is_hexdigit(ch) == false) {
            flag = false;
          }
        }
        if (flag) {
          ControllerData->set_wp_backcolor(str);
          str.toCharArray(backcolor, str.length() + 1);
        }
      }
      goto Get_Handler;
    }
  }  // end of post handler

Get_Handler:

  if (SPIFFS.exists("/admin9.html")) {
    File file = SPIFFS.open("/admin9.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    // Controller Mode
    if (myfp2esp32mode == ACCESSPOINT) {
      AdminPg.replace("%MOD%", T_ACCESSPOINT);
    } else {
      AdminPg.replace("%MOD%", T_STATION);
    }

    // Static IP
    if (IPADDRESSMODE == STATICIP) {
      AdminPg.replace("%IPS%", T_ON);
    } else {
      AdminPg.replace("%IPS%", T_OFF);
    }

    // device name
    msg = H_DEVICE_NAME;
    msg.replace("%dnam%", String(devicename));
    AdminPg.replace("%DNA%", msg);
    AdminPg.replace("%BDNA%", H_DEVICE_NAMEBTN);

    // List format form %FLF% List format state %FLS% List Input field %FLL% List Button %BFLF%</td></tr></table>
    AdminPg.replace("%FLF%", H_FILELISTFORM);
    if (ControllerData->get_filelistformat() == LISTSHORT) {
      AdminPg.replace("%FLL%", H_FILELISTLONG);
      AdminPg.replace("%FLS%", "Short");
    } else {
      AdminPg.replace("%FLL%", H_FILELISTSHORT);
      AdminPg.replace("%FLS%", "Long");
    }
    AdminPg.replace("%BFLF%", H_FILELISTFORMEND);

    // build pg form for web page colors
    AdminPg.replace("%PGF%", H_COLORFORM);

    // Title Color default
    // input text field %TCI% button %BTCI%
    msg = H_TITLECOLORFIELD;
    msg.replace("%tinum%", ControllerData->get_wp_titlecolor());
    AdminPg.replace("%TCI%", msg);
    AdminPg.replace("%BTCI%", H_TITLECOLORBTN);

    // Sub-title Color default
    // input text field %STC% button %BTCI%
    msg = H_SUBTITLECOLORFIELD;
    msg.replace("%stnum%", ControllerData->get_wp_subtitlecolor());
    AdminPg.replace("%SUTC%", msg);
    AdminPg.replace("%BSUTC%", H_SUBTITLECOLORBTN);

    // Header Group Color %HC% default
    msg = H_HEADERCOLORFIELD;
    msg.replace("%hcnum%", ControllerData->get_wp_headercolor());
    AdminPg.replace("%HCI%", msg);
    AdminPg.replace("%BHCI%", H_HEADERCOLORBTN);

    // Text Color %TC% default
    msg = H_TEXTCOLORFIELD;
    msg.replace("%tcnum%", ControllerData->get_wp_textcolor());
    AdminPg.replace("%TEI%", msg);
    AdminPg.replace("%BTEI%", H_TEXTCOLORBTN);

    // Background color input text field %BCI% button %BBC%
    msg = H_BACKCOLORFIELD;
    msg.replace("%bcnum%", ControllerData->get_wp_backcolor());
    AdminPg.replace("%BCI%", msg);
    AdminPg.replace("%BBCI%", H_BACKCOLORBTN);

    // end of form
    AdminPg.replace("%TCFE%", H_COLORFORMEND);

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }

  debug_server_print(T_ADMIN9);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}

// ----------------------------------------------------------------------
// handler for /move
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::getmove(void) {
  String AdminPg;
  String msg;

  AdminPg.reserve(5200);  // 306-05 4996

  debug_server_println(T_MOVE);

  if (!check_access()) {
    return;
  }

  if (adminmove_type == PosT) {

    debug_server_println(T_POST);

    // check if focuser is moving, cannot move if focuser is already moving
    if (isMoving == false) {
      long pos = 0;

      // Check the move buttons
      String mv = mserver->arg("mvl500");
      if (mserver->arg("mvl500") != "") {
        pos = -500;
      } else if (mserver->arg("mvl100") != "") {
        pos = -100;
      } else if (mserver->arg("mvl10") != "") {
        pos = -10;
      } else if (mserver->arg("mvl1") != "") {
        pos = -1;
      } else if (mserver->arg("mvp1") != "") {
        pos = 1;
      } else if (mserver->arg("mvp10") != "") {
        pos = 10;
      } else if (mserver->arg("mvp100") != "") {
        pos = 100;
      } else if (mserver->arg("mvp500") != "") {
        pos = 500;
      }
      if (pos != 0) {
        // a move button was pressed, so now process the move
        // get current focuser position
        long curpos = driverboard->getposition();
        // get maxsteps
        long maxpos = ControllerData->get_maxstep();
        // calculate target
        long target = curpos + pos;
        // range check target position
        target = (target < 0) ? 0 : target;
        target = (target > maxpos) ? maxpos : target;
        // apply the move
        ftargetPosition = target;
        goto Get_Handler;
      }

      // so it was not a move button. check for the goto button
      // handle goto
      String fp = mserver->arg("pos");
      if (fp != "") {
        long tp = 0;
        long max = (long)ControllerData->get_maxstep();
        tp = fp.toInt();
        tp = (tp < 0) ? 0 : tp;
        tp = (tp > max) ? max : tp;
        // apply the move
        ftargetPosition = tp;
        goto Get_Handler;
      }
    }
  }
  // end of move_post

Get_Handler:
  if (SPIFFS.exists("/adminmove.html")) {
    File file = SPIFFS.open("/adminmove.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    String pos = String(driverboard->getposition());

    //AdminPg.replace("%CPO%", String(driverboard->getposition()));
    AdminPg.replace("%CPO%", pos);
    pos = String(ControllerData->get_maxstep());
    AdminPg.replace("%MAX%", pos);
    AdminPg.replace("%TPO%", String(ftargetPosition));
    if (isMoving == true) {
      AdminPg.replace("%MOV%", "True");
    } else {
      AdminPg.replace("%MOV%", "False");
    }
    // Move form
    // now the buttons -500 to +500, each button is its own form
    AdminPg.replace("%MOVL500%", H_MOVL500);
    AdminPg.replace("%MOVL100%", H_MOVL100);
    AdminPg.replace("%MOVL10%", H_MOVL10);
    AdminPg.replace("%MOVL1%", H_MOVL1);
    AdminPg.replace("%MOVP1%", H_MOVP1);
    AdminPg.replace("%MOVP10%", H_MOVP10);
    AdminPg.replace("%MOVP100%", H_MOVP100);
    AdminPg.replace("%MOVP500%", H_MOVP500);

    // position and goto position button
    // Position [value span id POS2 %CP%] Input Field %PI% button %BP%
    AdminPg.replace("%CP%", pos);
    AdminPg.replace("%PI%", H_MPI);
    AdminPg.replace("%BP%", H_MBP);

    // halt button
    // inline html

    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }

  debug_server_print(T_MOVE);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}

// ----------------------------------------------------------------------
// handler for /adminlinks
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::getlinks(void) {
  String AdminPg;
  String msg;

  AdminPg.reserve(3900);  // 306-05 3610

  debug_server_println(T_LINKS);

  if (!check_access()) {
    return;
  }

  if (SPIFFS.exists("/adminlinks.html")) {
    File file = SPIFFS.open("/adminlinks.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }

  debug_server_print(T_LINKS);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}

// ----------------------------------------------------------------------
// void checkreboot(void);
// checks POST data for request reboot controller and performs reboot if required
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::checkreboot(void) {
  if (!check_access()) {
    return;
  }

  String msg = mserver->arg("cntlr_reboot");
  if (msg != "") {
    mserver->send(NORMALWEBPAGE, TEXTPAGETYPE, RebootStr);
    reboot_esp32(REBOOTDELAY);
  }
}


// ----------------------------------------------------------------------
// void post_deletefile(void);
// handles delete file request
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::post_deletefile() {
  String AdminPg;
  AdminPg.reserve(2700);  // 303  2460

  debug_server_print(T_DELETEOK);

  if (!check_access()) {
    return;
  }

  // check server arguments, df has filename
  String df = mserver->arg("fname");
  if (df != "") {
    // Filesystem was started earlier when server was started so assume it has started
    // df = " / " + df;
    if (df[0] != '/') {
      df = '/' + df;
    }

    // load the deleteok.html file
    if (SPIFFS.exists("/deleteok.html")) {
      // open file for read
      File file = SPIFFS.open("/deleteok.html", "r");
      // read contents into string
      AdminPg = file.readString();
      file.close();

      AdminPg.replace("%PGT%", devicename);
      // Web page colors
      AdminPg.replace("%TIC%", titlecolor);
      AdminPg.replace("%STC%", subtitlecolor);
      AdminPg.replace("%HEC%", headercolor);
      AdminPg.replace("%TXC%", textcolor);
      AdminPg.replace("%BKC%", backcolor);

      AdminPg.replace("%FIL%", df);

      // add code to handle reboot controller
      AdminPg.replace("%REBT%", H_CREBOOT);
      AdminPg.replace("%NAM%", ControllerData->get_brdname());
      AdminPg.replace("%VER%", String(program_version));
      AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
      get_systemuptime();
      AdminPg.replace("%SUT%", systemuptime);

      if (SPIFFS.exists(df)) {
        if (SPIFFS.remove(df)) {
          AdminPg.replace("%STA%", "deleted.");
        } else {
          debug_server_print(T_ERROR);
          debug_server_print(T_DELETE);
          debug_server_println(df);
          AdminPg.replace("%STA%", "err cannot delete");
        }
      } else {
        AdminPg.replace("%STA%", "file !found");
      }
    } else {
      debug_server_println(T_NOTFOUND);
      AdminPg = "<html><head><title>Management Server</title></head><body><p>deleteok.html not found</p><p><form action=\"/\"method=\"GET\"><input type=\"submit\" value=\"HOMEPAGE\"></form></p></body></html>";
    }
  } else {
    // null argument has been passed
    AdminPg = "<html><head><title>Management Server</title></head><body><p>Null argument found</p><p><form action=\"/\" method=\"GET\"><input type=\"submit\" value=\"HOMEPAGE\"></form></p></body></html>";
  }

  debug_server_print(T_DELETEOK);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}

// ----------------------------------------------------------------------
// void get_deletefile(void);
// displays delete html page
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::get_deletefile() {
  String AdminPg;
  AdminPg.reserve(2700);  // 306-01 2446

  debug_server_println(T_DELETE);

  if (!check_access()) {
    return;
  }

  if (SPIFFS.exists("/delete.html")) {
    File file = SPIFFS.open("/delete.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }

  debug_server_print(T_DELETE);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}

// ----------------------------------------------------------------------
// lists all files in file system
// shorten format of filenames only, eg admin1.html, admin2.html, .... cntlr_config.jsn
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::get_filelist_short(void) {
  String path = "/";

  debug_server_println("get_filelist_short");

  if (!check_access()) {
    return;
  }

  // format looks like [] array of {"type":"file","name":"admin2.html"}
  // example code taken from FSBrowser
  File root = SPIFFS.open(path);
  path = String();

  String output = "";
  if (root.isDirectory()) {
    File file = root.openNextFile();

    while (file) {
      output += String(file.name());
      file = root.openNextFile();
      if (file) {
        output += ", ";
      }
    }
    file.close();
  }
  mserver->send(NORMALWEBPAGE, String(JSONTEXTPAGETYPE), output);
}

// ----------------------------------------------------------------------
// void get_filelist_long(void);
// lists all files in file system, long format
// {[{"type":"file","name":"admin1.html"}, ... ,{"type":"file","name":"cntlr_config.jsn"}]}
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::get_filelist_long(void) {
  // format looks like [] array of {"type":"file","name":"admin2.html"}
  // example code taken from FSBrowser
  String path = "/";

  debug_server_println("get_filelist_long");

  if (!check_access()) {
    return;
  }

  File root = SPIFFS.open(path);
  path = String();

  String output = "{[";
  if (root.isDirectory()) {
    File file = root.openNextFile();
    while (file) {
      output += "{\"type\":\"";
      output += (file.isDirectory()) ? "dir" : "file";
      output += "\",\"name\":\"";
      output += String(file.name());
      output += "\"}";
      file = root.openNextFile();
      if (file) {
        output += ',';
      }
    }
    file.close();
  }
  output += "]}";
  mserver->send(NORMALWEBPAGE, String(JSONTEXTPAGETYPE), output);
}

// ----------------------------------------------------------------------
// void get_notfound(void);
// displays not found html page
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::get_notfound(void) {
  String AdminPg;
  AdminPg.reserve(5800);  // in case the file exists

  debug_server_println("get_notfound");
  if (!check_access()) {
    return;
  }

  // can we get server args to determine the filename?
  String p = mserver->uri();
  debug_server_print("-uri !found ");
  debug_server_println(p);

  // get the MIME type
  String contenttype = get_contenttype(p);
  if (SPIFFS.exists(p)) {
    debug_server_print("-File ");
    debug_server_print(p);
    debug_server_println(" found");

    File file = SPIFFS.open(p, "r");
    AdminPg = file.readString();
    file.close();
    debug_server_print("-send file ");
    debug_server_print(p);
    debug_server_println(" to client now");
    if (AdminPg[0] == '{') {
      // a json file
      send_json(AdminPg);
      return;
    }
    // for non jsn types like .ico
    mserver->send(NORMALWEBPAGE, contenttype, AdminPg);
    return;
  } else {

    debug_server_println(T_ADMINNOTFOUND);

    // file definately does not exist, so use notfound html file
    if (SPIFFS.exists("/adminnotfound.html")) {
      // open file for read
      File file = SPIFFS.open("/adminnotfound.html", "r");
      // read contents into string
      AdminPg = file.readString();
      file.close();

      AdminPg.replace("%PGT%", devicename);
      // Web page colors
      AdminPg.replace("%TIC%", titlecolor);
      AdminPg.replace("%STC%", subtitlecolor);
      AdminPg.replace("%HEC%", headercolor);
      AdminPg.replace("%TXC%", textcolor);
      AdminPg.replace("%BKC%", backcolor);

      // add handler for reboot controller
      AdminPg.replace("%REBT%", H_CREBOOT);
      AdminPg.replace("%NAM%", ControllerData->get_brdname());
      AdminPg.replace("%VER%", String(program_version));
      AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
      get_systemuptime();
      AdminPg.replace("%SUT%", systemuptime);
    } else {
      debug_server_println(T_NOTFOUND);
      AdminPg = H_FILENOTFOUNDSTR;
    }

    debug_server_print(T_ADMINNOTFOUND);
    debug_server_println(AdminPg.length());
    mserver->client().println("HTTP/1.1 200 OK");
    mserver->client().println("Content-type:text/html");
    mserver->client().println();
    mserver->client().print(AdminPg);
    mserver->client().println();
  }
}

// ----------------------------------------------------------------------
// handler when page or url not found
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::handlenotfound(void) {
  get_notfound();
}

// ----------------------------------------------------------------------
// String geturi(void);
// ----------------------------------------------------------------------
String MANAGEMENT_SERVER::get_uri(void) {
  String p = mserver->uri();
  debug_server_print("get uri() ");
  debug_server_println(p);
  return p;
}

// ----------------------------------------------------------------------
// void saveconfig(void);
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::saveconfig(void) {

  debug_server_println(T_SAVE);

  if (!check_access()) {
    return;
  }

  // save the focuser settings immediately
  if (ControllerData->SaveNow(driverboard->getposition(), driverboard->getdirection()) == true) {
    String AdminPg;
    AdminPg.reserve(3000);  // 300 		2047

    if (SPIFFS.exists("/configsaved.html")) {
      // open file for read
      File file = SPIFFS.open("/configsaved.html", "r");
      // read contents into string
      AdminPg = file.readString();
      file.close();

      // Web page colors
      AdminPg.replace("%PGT%", devicename);
      // Web page colors
      AdminPg.replace("%TIC%", titlecolor);
      AdminPg.replace("%STC%", subtitlecolor);
      AdminPg.replace("%HEC%", headercolor);
      AdminPg.replace("%TXC%", textcolor);
      AdminPg.replace("%BKC%", backcolor);

      // add handler for reboot controller
      AdminPg.replace("%REBT%", H_CREBOOT);
      AdminPg.replace("%NAM%", ControllerData->get_brdname());
      AdminPg.replace("%VER%", String(program_version));
      AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
      get_systemuptime();
      AdminPg.replace("%SUT%", systemuptime);
    } else {
      debug_server_println(T_NOTFOUND);
      AdminPg = H_FILENOTFOUNDSTR;
    }

    debug_server_print(T_CONFIGSAVED);
    debug_server_println(AdminPg.length());
    mserver->client().println("HTTP/1.1 200 OK");
    mserver->client().println("Content-type:text/html");
    mserver->client().println();
    mserver->client().print(AdminPg);
    mserver->client().println();
    return;
  } else {
    String AdminPg;
    AdminPg.reserve(3000);  // 300 		2049

    if (SPIFFS.exists("/confignotsaved.html")) {
      // open file for read
      File file = SPIFFS.open("/confignotsaved.html", "r");
      // read contents into string
      AdminPg = file.readString();
      file.close();

      AdminPg.replace("%PGT%", devicename);
      // Web page colors
      AdminPg.replace("%TIC%", titlecolor);
      AdminPg.replace("%STC%", subtitlecolor);
      AdminPg.replace("%HEC%", headercolor);
      AdminPg.replace("%TXC%", textcolor);
      AdminPg.replace("%BKC%", backcolor);

      // add handler for reboot controller
      AdminPg.replace("%REBT%", H_CREBOOT);
      AdminPg.replace("%NAM%", ControllerData->get_brdname());
      AdminPg.replace("%VER%", String(program_version));
      AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
      get_systemuptime();
      AdminPg.replace("%SUT%", systemuptime);
    } else {
      debug_server_println(T_NOTFOUND);
      AdminPg = H_FILENOTFOUNDSTR;
    }

    debug_server_print(T_CONFIGNOTSAVED);
    debug_server_println(AdminPg.length());
    mserver->client().println("HTTP/1.1 200 OK");
    mserver->client().println("Content-type:text/html");
    mserver->client().println();
    mserver->client().print(AdminPg);
    mserver->client().println();
  }
}

// ----------------------------------------------------------------------
// void upload_file(void);
// displays upload html page
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::upload_file(void) {
  String AdminPg;
  AdminPg.reserve(2500);  // 306-01 2307

  debug_server_println(T_UPLOAD);

  if (!check_access()) {
    return;
  }

  if (SPIFFS.exists("/upload.html")) {
    File file = SPIFFS.open("/upload.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }

  debug_server_print(T_UPLOAD);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}


// ----------------------------------------------------------------------
// void post_uploadstart(void);
// handles request to upload file
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::post_uploadstart(void) {
  if (this->_loaded == false) {
    return;
  }
  //debug_server_println("post_uploadstart: inform client to send data now");
  mserver->send(200);
}

// ----------------------------------------------------------------------
// void post_uploadfile(void);
// writes the upload file
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::post_uploadfile(void) {
  if (!check_access()) {
    return;
  }

  HTTPUpload &upload = mserver->upload();
  if (upload.status == UPLOAD_FILE_START) {
    String filename = upload.filename;
    if (!filename.startsWith("/")) {
      filename = "/" + filename;
    }
    debug_server_print("MS-FileUpload ");
    debug_server_println(filename);
    _fsUploadFile = SPIFFS.open(filename, "w");
    filename = String();
  } else if (upload.status == UPLOAD_FILE_WRITE) {
    if (_fsUploadFile) {
      _fsUploadFile.write(upload.buf, upload.currentSize);
    }
  } else if (upload.status == UPLOAD_FILE_END) {
    if (_fsUploadFile) {
      // If the file was successfully created
      _fsUploadFile.close();
      debug_server_print("-size ");
      debug_server_println(upload.totalSize);
      send_redirect("/success");
      return;
    } else {
      mserver->send(INTERNALSERVERERROR, String(PLAINTEXTPAGETYPE), "Err: create file");
    }
  }
}

// ----------------------------------------------------------------------
// void success(void);
// if requested operation was successful, display success html page
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::success(void) {
  String AdminPg;
  AdminPg.reserve(2500);  // 306-01 		2228

  debug_server_println(T_SUCCESS);

  if (!check_access()) {
    return;
  }

  if (SPIFFS.exists("/success.html")) {
    File file = SPIFFS.open("/success.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }
  debug_server_print(T_SUCCESS);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}

// ----------------------------------------------------------------------
// void fail(void);
// requested operation failed, display fail html page
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::fail(void) {
  String AdminPg;
  AdminPg.reserve(2500);  // 306-01 2211

  if (!check_access()) {
    return;
  }

  debug_server_println(T_FAIL);

  if (SPIFFS.exists("/fail.html")) {
    File file = SPIFFS.open("/fail.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    // For future use
    AdminPg.replace("%ERRORMSG%", "");

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }

  debug_server_println(T_FAIL);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}

// ----------------------------------------------------------------------
// void handlecmds(void);
// list all get and set commands
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::handlecmds(void) {
  if (!check_access()) {
    return;
  }

  debug_server_println(T_CMDS);

  String AdminPg;
  AdminPg.reserve(7000);  // 306-01 6742

  if (SPIFFS.exists("/cmds.html")) {
    File file = SPIFFS.open("/cmds.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }

  debug_server_print(T_CMDS);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}

// ----------------------------------------------------------------------
// void handleget(void);
// generic get handler for client requests
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::handleget(void) {

  debug_server_println("MS-get()");

  if (!check_access()) {
    return;
  }

  // return json string of state, on or off or value
  String jsonstr;

  debug_server_print("-get ");
  debug_server_println(mserver->argName(0));

  // get?ascom=
  if (mserver->argName(0) == "ascomserver") {
    // ascom alpaca server
    jsonstr = "{\"ascomserver\":";
    if (ControllerData->get_ascomsrvr_enable()) {
      jsonstr = jsonstr + "\"enabled\", ";
    } else {
      jsonstr = jsonstr + "\"notenabled\", ";
    }
    jsonstr = jsonstr + "\"ascomstatus\":";
    if (ascomsrvr_status == V_RUNNING) {
      jsonstr = jsonstr + "\"running\", ";
    } else {
      jsonstr = jsonstr + "\"stopped\", ";
    }
    jsonstr = jsonstr + "\"ascomport\":" + String(ControllerData->get_ascomsrvr_port()) + " }";
    send_json(jsonstr);
    return;
  }

  // get?boardconfig=
  else if (mserver->argName(0) == "boardconfig") {
    // Problem: this is the file. If var was just set then after 30s it
    // is written to file. We are not reading from cache
    File bfile = SPIFFS.open("/board_config.jsn", "r");
    if (!bfile) {
      jsonstr = "{ \"err\":\"unable to read file\" }";
    } else {
      jsonstr = bfile.readString();
      bfile.close();
    }
    send_json(jsonstr);
    return;
  }
  // get?coilpower=
  else if (mserver->argName(0) == "coilpower") {
    if (ControllerData->get_coilpower_enable() == true) {
      jsonstr = "{ \"coilpower\":\"enabled\" }";
    } else {
      jsonstr = "{ \"coilpower\":\"notenabled\" }";
    }
    send_json(jsonstr);
    return;
  }
  // get?cntlrconfig=
  else if (mserver->argName(0) == "cntlrconfig") {
    // Problem: this is the file. If var was just set then after 30s it
    // is written to file. We are not reading from cache
    File bfile = SPIFFS.open("/cntlr_config.jsn", "r");
    if (!bfile) {
      jsonstr = "{ \"err\":\"unable to read file\" }";
    } else {
      jsonstr = bfile.readString();
      bfile.close();
    }
    send_json(jsonstr);
    return;
  }
  // get?display=
  else if (mserver->argName(0) == "display") {
    if (ControllerData->get_display_enable() == true) {
      jsonstr = "{ \"display\":\"enabled\", ";
    } else {
      jsonstr = "{ \"display\":\"notenabled\", ";
    }
    if (display_status == V_RUNNING) {
      jsonstr = jsonstr + "\"displaystatus\":\"running\" }";
    } else {
      jsonstr = jsonstr + "\"displaystatus\":\"stopped\" }";
    }
    send_json(jsonstr);
    return;
  }
  // get?fixedstepmode=
  else if (mserver->argName(0) == "fixedstepmode") {
    jsonstr = "{ \"fixedstepmode\":" + String(ControllerData->get_brdfixedstepmode()) + " }";
    send_json(jsonstr);
    return;
  }
  // get?hpsw=
  else if (mserver->argName(0) == "hpsw") {
    if (ControllerData->get_hpswitch_enable() == true) {
      jsonstr = "{ \"hpsw\":\"enabled\", ";
    } else {
      jsonstr = "{ \"hpsw\":\"notenabled\", ";
    }
    send_json(jsonstr);
    return;
  }
  // get?ismoving=
  else if (mserver->argName(0) == "ismoving") {
    jsonstr = "{ \"ismoving\":" + String(isMoving) + " }";
    send_json(jsonstr);
    return;
  }
  // get?leds=
  else if (mserver->argName(0) == "leds") {
    if (ControllerData->get_inoutled_enable() == true) {
      jsonstr = "{ \"leds\":\"enabled\", ";
    } else {
      jsonstr = "{ \"leds\":\"notenabled\", ";
    }
    if (ControllerData->get_inoutled_mode() == LEDPULSE) {
      jsonstr = jsonstr + "\"ledmode\":\"pulse\" }";
    } else {
      jsonstr = jsonstr + "\"ledmode\":\"move\" }";
    }
    send_json(jsonstr);
    return;
  }
  // get?motorspeed=
  else if (mserver->argName(0) == "motorspeed") {
    jsonstr = "{ \"motorspeed\":" + String(ControllerData->get_motorspeed()) + ", ";
    jsonstr = jsonstr + "\"motorspeeddelay\":" + String(ControllerData->get_brdmsdelay()) + " }";
    send_json(jsonstr);
    return;
  }
  // get?park=
  else if (mserver->argName(0) == "park") {
    if (ControllerData->get_park_enable() == true) {
      jsonstr = "{ \"park\":\"enabled\", \"parktime\":" + String(ControllerData->get_parktime()) + " }";
    } else {
      jsonstr = "{ \"park\":\"notenabled\", \"parktime\":" + String(ControllerData->get_parktime()) + " }";
    }
    send_json(jsonstr);
    return;
  }
  // get?position=
  else if (mserver->argName(0) == "position") {
    jsonstr = "{ \"position\":" + String(ControllerData->get_fposition())
              + ", \"maxsteps\":" + String(ControllerData->get_maxstep())
              + ", \"ismoving\":" + String(isMoving) + " }";
    send_json(jsonstr);
    return;
  }
  // get?reverse=
  else if (mserver->argName(0) == "reverse") {
    if (ControllerData->get_reverse_enable() == true) {
      jsonstr = "{ \"reverse\":\"enabled\" }";
    } else {
      jsonstr = "{ \"reverse\":\"notenabled\" }";
    }
    send_json(jsonstr);
    return;
  }
  // get?rssi=
  else if (mserver->argName(0) == "rssi") {
    long ri = getrssi();
    jsonstr = "{ \"rssi\":" + String(ri) + " }";
    send_json(jsonstr);
    return;
  }
  // get?stepmode=
  else if (mserver->argName(0) == "stepmode") {
    jsonstr = "{ \"stepmode\":" + String(ControllerData->get_brdstepmode()) + " }";
    send_json(jsonstr);
    return;
  }
  // get?stallguard=
  else if (mserver->argName(0) == "stallguard") {
    byte sgval = STALL_VALUE;
    sgval = driverboard->getstallguardvalue();
    tmc2209stallguard sg = ControllerData->get_stallguard_state();
    if (sg == Use_Stallguard) {
      jsonstr = "{ \"stallguardstate\":\"Use_Stallguard\", ";
    } else if (sg == Use_Physical_Switch) {
      jsonstr = "{ \"stallguardstate\":\"Use_Physical_Switch\", ";
    } else if (sg == Use_None) {
      jsonstr = "{ \"stallguardstate\":\"Use_None\", ";
    } else {
      // error
      jsonstr = "{ \"sg_state\":\"error\", ";
    }
    jsonstr = jsonstr + "\"sg_value\":" + String(sgval) + " }";
    send_json(jsonstr);
    return;
  }
  // get?temp=
  else if (mserver->argName(0) == "temp") {
    if (ControllerData->get_tempprobe_enable() == true) {
      jsonstr = "{ \"tprobe\":\"enabled\", ";
    } else {
      jsonstr = "{ \"tprobe\":\"notenabled\", ";
    }
    if (tempprobe->get_state() == V_RUNNING) {
      jsonstr = jsonstr + "\"tprobestatus\":\"running\", ";
    } else {
      jsonstr = jsonstr + "\"tprobestatus\":\"stopped\", ";
    }
    jsonstr = jsonstr + "\"temperature\":" + String(temp, 2) + " }";
    send_json(jsonstr);
    return;
  }
  // get?tcpipserver=
  else if (mserver->argName(0) == "tcpipserver") {
    jsonstr = "{ \"tcpipserver\":";
    if (ControllerData->get_tcpipsrvr_enable() == true) {
      jsonstr = jsonstr + "\"enabled\", ";
    } else {
      jsonstr = jsonstr + "\"notenabled\", ";
    }
    jsonstr = jsonstr + "\"tcpipstatus\":";
    if (tcpipsrvr_status == V_RUNNING) {
      jsonstr = jsonstr + "\"running\", \"tcpipport\":" + String(ControllerData->get_tcpipsrvr_port()) + " }";
    } else {
      jsonstr = jsonstr + "\"stopped\", \"tcpipport\":" + String(ControllerData->get_tcpipsrvr_port()) + " }";
    }
    send_json(jsonstr);
    return;
  }
  // get?tmc2209current=
  else if (mserver->argName(0) == "tmc2209current") {
    jsonstr = "{\"tmc2209current\":" + String(ControllerData->get_tmc2209current()) + " }";
    send_json(jsonstr);
    return;
  }
  // get?tmc2225current=
  else if (mserver->argName(0) == "tmc2225current") {
    jsonstr = "{\"tmc2225current\":" + String(ControllerData->get_tmc2225current()) + " }";
    send_json(jsonstr);
    return;
  }
  // get?webserver=
  else if (mserver->argName(0) == "webserver") {
    jsonstr = "{ \"webserver\":";
    if (ControllerData->get_websrvr_enable() == true) {
      jsonstr = jsonstr + "\"enabled\", ";
    } else {
      jsonstr = jsonstr + "\"notenabled\", ";
    }
    jsonstr = jsonstr + "\"webstatus\":";
    if (websrvr_status == V_RUNNING) {
      jsonstr = jsonstr + "\"running\", \"webport\":" + String(ControllerData->get_websrvr_port()) + " }";
    } else {
      jsonstr = jsonstr + "\"stopped\", \"webport\":" + String(ControllerData->get_websrvr_port()) + " }";
    }
    send_json(jsonstr);
    return;
  } else {
    debug_server_print("-err arg ");
    debug_server_println(mserver->argName(0));
    jsonstr = "{ \"error\":\"unknown-command\" }";
    send_json(jsonstr);
    return;
  }
}

// ----------------------------------------------------------------------
// void handleset(void);
// generic set handler for client commands
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::handleset(void) {

  debug_server_println("MS-set()");

  if (!check_access()) {
    return;
  }

  // get parameter after ?
  String jsonstr;
  String va;
  String drvbrd = ControllerData->get_brdname();

  // ascom alpaca server
  va = mserver->arg("ascomserver");
  if (va != "") {
    if (va == "enable") {
      ControllerData->set_ascomsrvr_enable(V_ENABLED);
      jsonstr = "{ \"ascomserver\":\"enabled\" }";
    } else if (va == "disable") {
      if (ascomsrvr_status == V_RUNNING) {
        ascomsrvr->stop();
      }
      ascomsrvr_status = V_STOPPED;
      ControllerData->set_ascomsrvr_enable(V_NOTENABLED);
      jsonstr = "{ \"ascomserver\":\"notenabled\" }";
    } else if (va == "start") {
      if (ascomsrvr->start() == true) {
        ascomsrvr_status = V_RUNNING;
        jsonstr = "{ \"ascomstatus\":\"running\" }";
      } else {
        jsonstr = "{ \"ascomstatus\":\"stopped\" }";
      }
    } else if (va == "stop") {
      ascomsrvr->stop();
      ascomsrvr_status = V_STOPPED;
      jsonstr = "{ \"ascomstatus\":\"stopped\" }";
    }
    send_json(jsonstr);
    return;
  }

  va = mserver->arg("ascomport");
  if (va != "") {
    if (ascomsrvr_status == V_STOPPED) {
      unsigned long tmp = va.toInt();
      ControllerData->set_ascomsrvr_port(tmp);
      jsonstr = "{ \"ascomport\":" + String(tmp) + " }";
    } else {
      jsonstr = "{ \"ascomport\": \"error-not-set\" }";
    }
    send_json(jsonstr);
    return;
  }

  // coilpower
  va = mserver->arg("coilpower");
  if (va != "") {
    if (va == "enable") {
      ControllerData->set_coilpower_enable(V_ENABLED);
      driverboard->enablemotor();
      jsonstr = "{ \"coilpower\":\"enabled\" }";
    } else if (va == "disable") {
      ControllerData->set_coilpower_enable(V_NOTENABLED);
      driverboard->releasemotor();
      jsonstr = "{ \"coilpower\":\"notenabled\" }";
    }
    send_json(jsonstr);
    return;
  }

  // display enable/disable
  va = mserver->arg("display");
  if (va != "") {
    if (va == "enable") {
      ControllerData->set_display_enable(V_ENABLED);
      jsonstr = "{ \"display\":\"enabled\" }";
    } else if (va == "disable") {
      // stop the display if running
      if (display_status == V_RUNNING) {
        display_stop();
      }
      ControllerData->set_display_enable(V_NOTENABLED);
      jsonstr = "{ \"display\":\"notenabled\" }";
    }
    send_json(jsonstr);
    return;
  }

  // display status V_RUNNING, V_STOPPED, start, stop
  va = mserver->arg("displaystatus");
  if (va != "") {
    if (va == "start") {
      if (ControllerData->get_display_enable() == V_ENABLED) {
        if (display_start() == true) {
          jsonstr = "{ \"displaystatus\":\"running\" }";
        } else {
          jsonstr = "{ \"displaystatus\":\"stopped\" }";
        }
      } else {
        jsonstr = "{ \"displaystatus\":\"stopped\" }";
      }
    } else if (va == "stop") {
      // stop the display
      display_stop();
      jsonstr = "{ \"display\":\"stopped\" }";
    }
    send_json(jsonstr);
    return;
  }

  // fixedstepmode for uln2003, l298n, l293d-mini etc
  va = mserver->arg("fixedstepmode");
  if (va != "") {
    int tmp = va.toInt();
    ControllerData->set_brdfixedstepmode(tmp);
    jsonstr = "{ \"fixedstepmode\":" + String(tmp) + " }";
  }

  // halt
  va = mserver->arg("halt");
  if (va != "") {
    if (va == "yes") {
      portENTER_CRITICAL(&halt_alertMux);
      halt_alert = true;
      portEXIT_CRITICAL(&halt_alertMux);
    }
    jsonstr = "{ \"halt\":" + String(driverboard->getposition()) + " }";
    send_json(jsonstr);
    return;
  }

  // home position switch enable
  va = mserver->arg("hpsw");
  if (va != "") {
    if (va == "enable") {
      ControllerData->set_hpswitch_enable(V_ENABLED);
      if (driverboard->init_hpsw() == true) {
        jsonstr = "{ \"hpsw\":\"enabled\" }";
      } else {
        jsonstr = "{ \"hpsw\":\"notenabled\" }";
      }
    } else if (va == "disable") {
      ControllerData->set_hpswitch_enable(V_NOTENABLED);
      jsonstr = "{ \"hpsw\":\"off\" }";
    }
    send_json(jsonstr);
    return;
  }

  // leds in out enable
  va = mserver->arg("leds");
  if (va != "") {
    if (va == "enable") {
      if (driverboard->set_leds(true) == true) {
        jsonstr = "{ \"leds\":\"enabled\" }";
      } else {
        // error could not be set
        jsonstr = "{ \"leds\":\"notenabled\" }";
      }
    } else if (va == "disable") {
      if (driverboard->set_leds(false) == true) {
        jsonstr = "{ \"leds\":\"notenabled\" }";
      } else {
        // error could not be set
        jsonstr = "{ \"leds\":\"enabled\" }";  // TODO enabled is wrong response
      }
    }
    send_json(jsonstr);
    return;
  }

  // leds mode, pulse or move
  va = mserver->arg("ledmode");
  if (va != "") {
    if (va == "pulse") {
      ControllerData->set_inoutled_mode(LEDPULSE);
      jsonstr = "{ \"ledmode\":\"pulse\" }";
    } else if (va == "move") {
      ControllerData->set_inoutled_mode(LEDMOVE);
      jsonstr = "{ \"ledmode\":\"move\" }";
    }
    send_json(jsonstr);
    return;
  }

  // motorspeed
  va = mserver->arg("motorspeed");
  if (va != "") {
    int tmp = va.toInt();
    if (tmp < SLOW) {
      tmp = SLOW;
    }
    if (tmp > FAST) {
      tmp = FAST;
    }
    ControllerData->set_motorspeed(tmp);
    jsonstr = "{ \"motorspeed\":\"" + String(tmp) + " }";
    send_json(jsonstr);
    return;
  }

  // motorspeeddelay value
  va = mserver->arg("motorspeeddelay");
  if (va != "") {
    unsigned long tmp = va.toInt();
    ControllerData->set_brdmsdelay(tmp);
    jsonstr = "{ \"motorspeeddelay\":\"" + String(tmp) + " }";
    send_json(jsonstr);
    return;
  }

  // move - moves focuser position
  va = mserver->arg("move");
  if (va != "") {
    long tmp = va.toInt();
    tmp = (tmp < 0) ? 0 : tmp;
    tmp = (tmp > ControllerData->get_maxstep()) ? ControllerData->get_maxstep() : tmp;
    ftargetPosition = tmp;
    jsonstr = "{ \"move\":" + String(ftargetPosition) + " }";
    send_json(jsonstr);
    return;
  }

  // park enabled state
  va = mserver->arg("park");
  if (va != "") {
    if (va == "enable") {
      ControllerData->set_park_enable(V_ENABLED);
      jsonstr = "{ \"park\":\"enabled\" }";
    } else if (va == "disable") {
      ControllerData->set_park_enable(V_NOTENABLED);
      jsonstr = "{ \"park\":\"notenabled\" }";
    }
    send_json(jsonstr);
    return;
  }

  // Park time
  va = mserver->arg("parktime");
  if (va != "") {
    int pt = va.toInt();
    // range check 0 - 300 (5m)
    pt = (pt < 0) ? 0 : pt;
    pt = (pt > 600) ? 600 : pt;
    ControllerData->set_parktime(pt);
    // update park_maxcount
    portENTER_CRITICAL(&parkMux);
    park_maxcount = pt * 10;  // convert to timeslices
    portEXIT_CRITICAL(&parkMux);
    jsonstr = "{ \"parktime\":" + String(pt) + " }";
    send_json(jsonstr);
    return;
  }

  // position - does not move focuser
  va = mserver->arg("position");
  if (va != "") {
    long tmp = va.toInt();
    tmp = (tmp < 0) ? 0 : tmp;
    tmp = (tmp > ControllerData->get_maxstep()) ? ControllerData->get_maxstep() : tmp;
    ftargetPosition = tmp;
    ControllerData->set_fposition(ftargetPosition);  // current position in SPIFFS
    driverboard->setposition(ftargetPosition);       // current position in driver board
    jsonstr = "{ \"position\":" + String(ftargetPosition) + " }";
    send_json(jsonstr);
    return;
  }

  // reverse direction
  va = mserver->arg("reverse");
  if (va != "") {
    if (va == "enable") {
      ControllerData->set_reverse_enable(V_ENABLED);
      jsonstr = "{ \"reverse\":\"enabled\" }";
    } else if (va == "disable") {
      ControllerData->set_reverse_enable(V_NOTENABLED);
      jsonstr = "{ \"reverse\":\"notenabled\" }";
    }
    send_json(jsonstr);
    return;
  }

  va = mserver->arg("stallguardstate");
  if (va != "") {
    jsonstr = "{ \"state\": \"not implemented yet\" }";
    //Use_Stallguard, Use_Physical_Switch, Use_None
    if (va == "stallguard") {
      tmc2209stallguard thisstate = Use_Stallguard;
      // save stallguard source
      ControllerData->set_stallguard_state(thisstate);
      // set stallguard value
      driverboard->setstallguardvalue(ControllerData->get_stallguard_value());
      // enable hpsw
      ControllerData->set_hpswitch_enable(V_ENABLED);
      // reset hpsw
      driverboard->init_hpsw();
      debug_server_println(T_STALLGUARD);
      jsonstr = "{ \"state\": \"Use_Stallguard\" }";
    } else if (va == "switch") {
      tmc2209stallguard thisstate = Use_Physical_Switch;
      // save stallguard source
      ControllerData->set_stallguard_state(thisstate);
      // enable hpsw
      ControllerData->set_hpswitch_enable(V_ENABLED);
      // disable stallguard for tmc2209
      driverboard->setstallguardvalue(0);
      // reset hpsw
      driverboard->init_hpsw();
      debug_server_println(T_PHYSICALSWITCH);
      jsonstr = "{ \"state\": \"Use_Physical_Switch\" }";
    } else if (va == "none") {
      tmc2209stallguard thisstate = Use_None;
      // save state
      ControllerData->set_stallguard_state(thisstate);
      // disable hpsw
      ControllerData->set_hpswitch_enable(V_NOTENABLED);
      // disable stallguard for tmc2209
      driverboard->setstallguardvalue(0);
      // reset hpsw
      driverboard->init_hpsw();
      debug_server_println(T_NONE);
      jsonstr = "{ \"state\": \"Use_None\" }";
    } else {
      // error
    }
    send_json(jsonstr);
    return;
  }

  // stall guard value
  va = mserver->arg("stallguardvalue");
  if (va != "") {
    int tmp = va.toInt();
    // write value to 2209 registers and update ControllerData
    driverboard->setstallguardvalue((byte)tmp);
    tmp = ControllerData->get_stallguard_value();
    jsonstr = "{ \"sg_value\":" + String(tmp) + " }";
    send_json(jsonstr);
    return;
  }

  // stepmode
  va = mserver->arg("stepmode");
  if (va != "") {
    int tmp = va.toInt();
    // write to pins and update mySetupData
    driverboard->setstepmode(tmp);
    // read actual stepmode set by driverboard
    tmp = ControllerData->get_brdstepmode();
    jsonstr = "{ \"stepmode\":" + String(tmp) + " }";
    send_json(jsonstr);
    return;
  }

  // tcpip server
  va = mserver->arg("tcpipserver");
  if (va != "") {
    if (va == "enable") {
      ControllerData->set_tcpipsrvr_enable(V_ENABLED);
      jsonstr = "{ \"tcpipserver\":\"enabled\" }";
    } else if (va == "disable") {
      if (tcpipsrvr_status == V_RUNNING) {
        tcpipsrvr->stop();
      }
      tcpipsrvr_status = V_STOPPED;
      ControllerData->set_tcpipsrvr_enable(V_NOTENABLED);
      jsonstr = "{ \"tcpipserver\":\"notenabled\" }";
    } else if (va == "start") {
      if (tcpipsrvr->start(ControllerData->get_tcpipsrvr_port()) == true) {
        tcpipsrvr_status = V_RUNNING;
        jsonstr = "{ \"tcpipstatus\":\"running\" }";
      } else {
        jsonstr = "{ \"tcpipstatus\":\"stopped\" }";
      }
    } else if (va == "stop") {
      tcpipsrvr->stop();
      tcpipsrvr_status = V_STOPPED;
      jsonstr = "{ \"tcpipstatus\":\"stopped\" }";
    }
    send_json(jsonstr);
    return;
  }

  va = mserver->arg("tcpipport");
  if (va != "") {
    if (tcpipsrvr_status == V_STOPPED) {
      unsigned long tmp = va.toInt();
      ControllerData->set_tcpipsrvr_port(tmp);
      jsonstr = "{ \"tcpipport\":" + String(tmp) + " }";
    } else {
      // cannot change port when srvr is running
      jsonstr = "{ \"tcpipport\":\"error-not-set\" }";
    }
  }

  // temperature probe enable/disable
  va = mserver->arg("tempprobe");
  if (va != "") {
    if (va == "enable") {
      // start probe
      if (tempprobe->get_loaded() != V_RUNNING) {
        if (tempprobe->start() == true) {
          // probe was started
          jsonstr = "{ \"tempprobe\":\"enabled\" }";
        } else {
          // did not start
          jsonstr = "{ \"tempprobe\":\"notenabled\" }";
        }
      } else {
        jsonstr = "{ \"tempprobe\":\"enabled\" }";
      }
    } else if (va == "disable") {
      // is it already off ?
      if (tempprobe->get_loaded() != V_RUNNING) {
        jsonstr = "{ \"tempprobe\":\"notenabled\" }";
      } else {
        // is loaded so stop probe
        tempprobe->stop();
        // there is no destructor call
        jsonstr = "{ \"tempprobe\":\"notenabled\" }";
      }
    }
    send_json(jsonstr);
    return;
  }

  // tmc2209current
  va = mserver->arg("tmc2209current");
  if (va != "") {
    int tmp = va.toInt();
    // write current value to tmc22xx, call ControllerData->set_tmc2209current(temp);
    driverboard->settmc2209current(tmp);
    jsonstr = "{ \"tmc2209current\":" + String(tmp) + " }";
    send_json(jsonstr);
    return;
  }

  // tmc2225current
  va = mserver->arg("tmc2225current");
  if (va != "") {
    int tmp = va.toInt();
    // write current value to tmc22xx, call ControllerData->set_tmc2225current(temp);
    driverboard->settmc2225current(tmp);
    jsonstr = "{ \"tmc2225current\":" + String(tmp) + " }";
    send_json(jsonstr);
    return;
  }

  // web server
  va = mserver->arg("webserver");
  if (va != "") {
    if (va == "enable") {
      ControllerData->set_websrvr_enable(V_ENABLED);
      jsonstr = "{ \"webserver\":\"enabled\" }";
    } else if (va == "disable") {
      if (websrvr_status == V_RUNNING) {
        websrvr->stop();
      }
      websrvr_status = V_STOPPED;
      ControllerData->set_websrvr_enable(V_NOTENABLED);
      jsonstr = "{ \"webserver\":\"notenabled\" }";
    } else if (va == "start") {
      if (websrvr_status == V_STOPPED) {
        // attempt to start the web server
        websrvr->start(ControllerData->get_websrvr_port());
        // did webserver start? then websrvr_status would be V_RUNNING
        if (websrvr_status == V_RUNNING) {
          jsonstr = "{ \"webstatus\":\"running\" }";
        } else {
          // did not start
          jsonstr = "{ \"webstatus\":\"stopped\" }";
        }
      } else {
        // already on
        jsonstr = "{ \"webstatus\":\"running\" }";
      }
    } else if (va == "stop") {
      // is it on?
      if (websrvr_status == V_RUNNING) {
        websrvr->stop();
        jsonstr = "{ \"webstatus\":\"stopped\" }";
      } else {
        // already off
        jsonstr = "{ \"webstatus\":\"stopped\" }";
      }
    }
    send_json(jsonstr);
    return;
  }

  // web server
  va = mserver->arg("webport");
  if (va != "") {
    if (websrvr_status == V_STOPPED) {
      unsigned long tmp = va.toInt();
      ControllerData->set_websrvr_port(tmp);
      jsonstr = "{ \"webport\":" + String(tmp) + " }";
    } else {
      // cannot change port when srvr is running

      jsonstr = "{ \"webport\":\"error-not-set\" }";
    }
    send_json(jsonstr);
    return;
  }

  if (jsonstr != "") {
    send_json(jsonstr);
  } else {
    send_json("{ \"error\":\"not set\" }");
  }
}

// XHTML
// ----------------------------------------------------------------------
// get position and send to web client
// xhtml
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::getposition() {
  // Send position value only to client ajax request
  mserver->send(NORMALWEBPAGE, PLAINTEXTPAGETYPE, String(driverboard->getposition()));
}

// ----------------------------------------------------------------------
// get ismoving and send to web client
// xhtml
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::getismoving() {
  // Send isMoving value only to client ajax request
  if (isMoving == true) {
    mserver->send(NORMALWEBPAGE, PLAINTEXTPAGETYPE, "True");
  } else {
    mserver->send(NORMALWEBPAGE, PLAINTEXTPAGETYPE, "False");
  }
}

// ----------------------------------------------------------------------
// get target position and send to web client
// xhtml
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::gettargetposition() {
  //Send targetPosition value only to client ajax request
  mserver->send(NORMALWEBPAGE, PLAINTEXTPAGETYPE, String(ftargetPosition));
}


// ----------------------------------------------------------------------
// void rssi(void);
// get network signal strength
// xhtml
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::rssi(void) {
  if (!check_access()) {
    return;
  }

  long rssi = getrssi();
  mserver->send(NORMALWEBPAGE, PLAINTEXTPAGETYPE, String(rssi));
}


// ----------------------------------------------------------------------
// DRIVER BOARD MANAGEMENT
// ----------------------------------------------------------------------
// EDIT THE CURRENT BOARD CONFIG SETTINGS
// void brdedit(void);
// Displays the current board config data in use, using editable text fields
// Save option:
// If saved
//		1. the board file will be written back to /data/boards/xx.jsn where
// 		    xx is the boardnumber (existing file deleted)
//		2. the new board settings will be written to board_config.jsn
//		   (exisitng file deleted)
// ----------------------------------------------------------------------
void MANAGEMENT_SERVER::brdedit() {
  String AdminPg;
  AdminPg.reserve(4200);  // 306-01 		3989

  debug_server_println("MS-brdedit ");

  if (!check_access()) {
    return;
  }

  // to handle reboot option
  checkreboot();

  if (brdedit_type == PosT) {
    // post handler
    String jsonstr;
    jsonstr.reserve(1000);
    String va = mserver->arg("wrbrd");

    // write the config to a file /boards/xx.jsn
    // call controllerdata to load the new board file, save it in board_config.jsn file

    if (va != "") {
      // get the board data that was passed from brdedit
      // and build a JSON string representing the board configuration
      jsonstr = "{ ";
      String tmp = mserver->arg("brd");
      jsonstr = jsonstr + "\"board\":\"" + tmp + "\",";
      tmp = mserver->arg("max");
      jsonstr = jsonstr + "\"maxstepmode\":" + tmp + ",";
      tmp = mserver->arg("stm");
      jsonstr = jsonstr + "\"stepmode\":" + tmp + ",";
      tmp = mserver->arg("enp");
      jsonstr = jsonstr + "\"enpin\":" + tmp + ",";
      tmp = mserver->arg("stp");
      jsonstr = jsonstr + "\"steppin\":" + tmp + ",";
      tmp = mserver->arg("dip");
      jsonstr = jsonstr + "\"dirpin\":" + tmp + ",";
      tmp = mserver->arg("tep");
      jsonstr = jsonstr + "\"temppin\":" + tmp + ",";
      tmp = mserver->arg("hpp");
      jsonstr = jsonstr + "\"hpswpin\":" + tmp + ",";
      tmp = mserver->arg("inp");
      jsonstr = jsonstr + "\"inledpin\":" + tmp + ",";
      tmp = mserver->arg("oup");
      jsonstr = jsonstr + "\"outledpin\":" + tmp + ",";
      tmp = mserver->arg("pb1");
      jsonstr = jsonstr + "\"pb1pin\":" + tmp + ",";
      tmp = mserver->arg("pb2");
      jsonstr = jsonstr + "\"pb2pin\":" + tmp + ",";
      tmp = mserver->arg("irp");
      jsonstr = jsonstr + "\"irpin\":" + tmp + ",";
      tmp = mserver->arg("bnu");
      jsonstr = jsonstr + "\"brdnum\":" + tmp + ",";
      myboardnumber = tmp.toInt();  // if .toInt() fails then value is 0
      tmp = mserver->arg("str");
      jsonstr = jsonstr + "\"stepsrev\":" + tmp + ",";
      tmp = mserver->arg("fim");
      jsonstr = jsonstr + "\"fixedsmode\":" + tmp + ",";
      tmp = mserver->arg("brp");
      jsonstr = jsonstr + "\"brdpins\":" + tmp + ",";
      tmp = mserver->arg("msd");
      jsonstr = jsonstr + "\"msdelay\":" + tmp + " }";
    }

    debug_server_print("-brd len ");
    debug_server_println(jsonstr.length());
    // save board config
    if (myboardnumber != 0) {
      String brdfile = "/boards/" + String(myboardnumber) + ".jsn";
      debug_server_print("-save brd ");
      debug_server_println(brdfile);

      // if board file exists then remove it
      if (SPIFFS.exists(brdfile)) {
        // delete existing custom file
        SPIFFS.remove(brdfile);
      }

      // now write the board config file
      File bfile = SPIFFS.open(brdfile, "w");
      if (!bfile) {
        debug_server_println(T_OPENERROR);
        send_redirect("/fail");
        //bfile.close();
        return;
      } else {
        bfile.print(jsonstr);
        bfile.close();
        debug_server_println(T_SAVED);

        // NEXT we have to update board_config.jsn file with the new board config
        // since the brdfile has been saved we could just use ControllerData->LoadDefaultBoardData() to load
        // the new board - this uses myboardnumber which we have already set
        debug_server_println("-create new board file");
        ControllerData->LoadDefaultBoardData();

        // Setup up the all ok in the universe page, and send to user
        send_redirect("/success");
        return;
      }
    } else {
      debug_server_println("-err Board number invalid");
      send_redirect("/fail");
      return;
    }
  }

  // get handler
  if (SPIFFS.exists("/brdedit.html")) {
    File file = SPIFFS.open("/brdedit.html", "r");
    AdminPg = file.readString();
    file.close();

    AdminPg.replace("%PGT%", devicename);
    // Web page colors
    AdminPg.replace("%TIC%", titlecolor);
    AdminPg.replace("%STC%", subtitlecolor);
    AdminPg.replace("%HEC%", headercolor);
    AdminPg.replace("%TXC%", textcolor);
    AdminPg.replace("%BKC%", backcolor);

    // put all the board settings into a table, so that the form will submit all elements to board save
    AdminPg.replace("%STA%", "<table><tr><td><form action=\"/brdedit\" method=\"post\"><input type=\"hidden\" name=\"wrbrd\" value=\"true\"></td><td> </td></tr>");
    AdminPg.replace("%BRD%", "<tr><td>Board Name </td><td><input type=\"text\" name=\"brd\" value=\"" + ControllerData->get_brdname() + "\"></td></tr>");
    AdminPg.replace("%MAX%", "<tr><td>MaxStepMode </td><td><input type=\"text\" name=\"max\" value=\"" + String(ControllerData->get_brdmaxstepmode()) + "\"></td></tr>");
    AdminPg.replace("%STM%", "<tr><td>Step Mode </td><td><input type=\"text\" name=\"stm\" value=\"" + String(ControllerData->get_brdstepmode()) + "\"></td></tr>");
    AdminPg.replace("%ENP%", "<tr><td>Enable pin </td><td><input type=\"text\" name=\"enp\" value=\"" + String(ControllerData->get_brdenablepin()) + "\"></td></tr>");
    AdminPg.replace("%STP%", "<tr><td>Step pin </td><td><input type=\"text\" name=\"stp\" value=\"" + String(ControllerData->get_brdsteppin()) + "\"></td></tr>");
    AdminPg.replace("%DIP%", "<tr><td>Dir pin </td><td><input type=\"text\" name=\"dip\" value=\"" + String(ControllerData->get_brddirpin()) + "\"></td></tr>");
    AdminPg.replace("%TEP%", "<tr><td>Temp pin </td><td><input type=\"text\" name=\"tep\" value=\"" + String(ControllerData->get_brdtemppin()) + "\"></td></tr>");
    AdminPg.replace("%HPP%", "<tr><td>HPSW pin </td><td><input type=\"text\" name=\"hpp\" value=\"" + String(ControllerData->get_brdhpswpin()) + "\"></td></tr>");
    AdminPg.replace("%INP%", "<tr><td>In led pin </td><td><input type=\"text\" name=\"inp\" value=\"" + String(ControllerData->get_brdinledpin()) + "\"></td></tr>");
    AdminPg.replace("%OUP%", "<tr><td>Out led pin </td><td><input type=\"text\" name=\"oup\" value=\"" + String(ControllerData->get_brdoutledpin()) + "\"></td></tr>");
    AdminPg.replace("%PB1%", "<tr><td>PB1 pin </td><td><input type=\"text\" name=\"pb1\" value=\"" + String(ControllerData->get_brdpb1pin()) + "\"></td></tr>");
    AdminPg.replace("%PB2%", "<tr><td>PB2 pin </td><td><input type=\"text\" name=\"pb2\" value=\"" + String(ControllerData->get_brdpb2pin()) + "\"></td></tr>");
    AdminPg.replace("%IRP%", "<tr><td>IR pin </td><td><input type=\"text\" name=\"irp\" value=\"" + String(ControllerData->get_brdirpin()) + "\"></td></tr>");
    AdminPg.replace("%STR%", "<tr><td>Steps per rev </td><td><input type=\"text\" name=\"str\" value=\"" + String(ControllerData->get_brdstepsperrev()) + "\"></td></tr>");
    AdminPg.replace("%FIM%", "<tr><td>Fixed Step Mode </td><td><input type=\"text\" name=\"fim\" value=\"" + String(ControllerData->get_brdfixedstepmode()) + "\"></td></tr>");
    String boardpins;
    boardpins.reserve(20);
    boardpins = "[";
    for (int i = 0; i < 4; i++) {
      boardpins = boardpins + String(ControllerData->get_brdboardpins(i));
      if (i < 3) {
        boardpins = boardpins + ", ";
      }
    }
    boardpins = boardpins + "]";
    AdminPg.replace("%BRP%", "<tr><td>Board pins : </td><td><input type=\"text\" name=\"brp\" value=\"" + boardpins + "\"></td></tr>");
    AdminPg.replace("%BRN%", "<tr><td>Board Num </td><td><input type=\"text\" name=\"bnu\" value=\"" + String(ControllerData->get_brdnumber()) + "\"</td></tr>");
    AdminPg.replace("%MSD%", "<tr><td>MS Delay : </td><td><input type=\"text\" name=\"msd\" value=\"" + String(ControllerData->get_brdmsdelay()) + "\"></td></tr>");
    AdminPg.replace("%END%", "<tr><td><input type=\"submit\" value=\"SAVE BOARD CONFIG\"></form></td><td> </td></tr></table>");

    // When user clicks on SAVE BOARD CONFIG it will call POST brdedit with "wrbrd" containing all the board settings

    // add handler for reboot controller
    AdminPg.replace("%REBT%", H_CREBOOT);
    AdminPg.replace("%NAM%", ControllerData->get_brdname());
    AdminPg.replace("%VER%", String(program_version));
    AdminPg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    AdminPg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    AdminPg = H_FILENOTFOUNDSTR;
  }
  debug_server_print(T_BOARDEDIT);
  debug_server_println(AdminPg.length());
  mserver->client().println("HTTP/1.1 200 OK");
  mserver->client().println("Content-type:text/html");
  mserver->client().println();
  mserver->client().print(AdminPg);
  mserver->client().println();
}
