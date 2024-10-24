// ----------------------------------------------------------------------
// myFP2ESP32 FOCUSER CONFIGURATION CLASS
// Copyright Robert Brown 2014-2023. All Rights Reserved.
// Copyright Holger M, 2019-2021. All Rights Reserved.
// controller_data.cpp
// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
// Includes
// ----------------------------------------------------------------------
#include <Arduino.h>
#include "controller_config.h"
#include <ArduinoJson.h>
#include "SPIFFS.h"

// DEFAULT CONFIGURATION
// ControllerData
// Controller Persistant Data  cntlr_config.jsn
// Controller Variable Data    cntlr_var.jsn
// Controller Board Data       board_config.jsn


// -----------------------------------------------------------------------
// DEBUGGING
// DO NOT ENABLE DEBUGGING INFORMATION. CANNOT USE DEBUG SERVER HERE
// -----------------------------------------------------------------------
// Remove comment to enable messages to Serial port
//#define CNTLRDATA_PRINT 1

#ifdef CNTLRDATA_PRINT
#define CNTLRDATA_print(...) Serial.print(__VA_ARGS__)
#define CNTLRDATA_println(...) Serial.println(__VA_ARGS__)
#else
#define CNTLRDATA_print(...)
#define CNTLRDATA_println(...)
#endif


// ----------------------------------------------------------------------
// CLASSES
// ----------------------------------------------------------------------
#include "controller_data.h"
extern CONTROLLER_DATA *ControllerData;

#include "driver_board.h"
extern DRIVER_BOARD *driverboard;


// ----------------------------------------------------------------------
// OTA DATA
// ----------------------------------------------------------------------
extern const char *OTAName;
extern const char *OTAPassword;
extern const char *OTAID;
#include "defines/ota_defines.h"


// ----------------------------------------------------------------------
// TMC STEPPER DEFINES
// ----------------------------------------------------------------------
#include "defines/tmcstepper_defines.h"


// ----------------------------------------------------------------------
// EXTERNS
// ----------------------------------------------------------------------
extern const char *project_name;
extern int myboardnumber;
extern int myfixedstepmode;
extern int mystepsperrev;
extern char duckdnsdomain[];
extern char duckdnstoken[];
extern char devicename[];
extern enum Display_Types displaytype;
extern bool isMoving;
extern bool filesystemloaded;

// task timer
extern volatile int save_var_flag;
extern volatile int save_board_flag;
extern volatile int save_cntlr_flag;
extern portMUX_TYPE varMux;
extern portMUX_TYPE boardMux;
extern portMUX_TYPE cntlrMux;

extern unsigned int display_maxcount;
extern portMUX_TYPE displaytimeMux;


// ----------------------------------------------------------------------
// DEFINES
// ----------------------------------------------------------------------
#define DEFAULT_FALSE 0
#define DEFAULT_TRUE 1
#define DEFAULT_OFF 0
#define DEFAULT_ON 1
#define DEFAULTCELSIUS 1
#define DEFAULTFAHREN 0
#define DEFAULTPOSITION 42000L
#define DEFAULTMAXSTEPS 84000L
// Configuration file raw 917, JSON assistant Total Deserialize 2048
#define DEFAULTCONFIGSIZE 3072
// Board data raw 264, ArduinoJson Assistant 768
#define BOARDDATASIZE 1024
// Position and Direction raw data 22, ArduinoJson Assistant 32
#define BOARDVARDATASIZE 64


// ----------------------------------------------------------------------
// CONTROLLER_DATA CLASS
// ----------------------------------------------------------------------
CONTROLLER_DATA::CONTROLLER_DATA(void) {
  // task timer has not started yet, so this is safe
  save_var_flag = -1;
  save_board_flag = -1;
  save_cntlr_flag = -1;

  // mount SPIFFS
  CNTLRDATA_print(T_CONTROLLERDATA);
  CNTLRDATA_println(T_START);
  if (!SPIFFS.begin()) {
    CNTLRDATA_println("Formatting");
    SPIFFS.format();
    filesystemloaded = false;
  } else {
    CNTLRDATA_println(T_LOADED);
    filesystemloaded = true;
  }
  LoadConfiguration();
};


// ----------------------------------------------------------------------
// Loads the configuration from files (cntlr, board, var)
// If configuration files are not found, or cannot be deserialised, then
// create Default Configurations for each
// ----------------------------------------------------------------------
bool CONTROLLER_DATA::LoadConfiguration() {
  CNTLRDATA_println("CD LoadConfiguration ");
  CNTLRDATA_print(T_LOAD);
  CNTLRDATA_println(file_cntlr_config);
  // Focuser persistant data - Open cntlr_config.jsn file for reading
  if (SPIFFS.exists(file_cntlr_config) == false) {
    CNTLRDATA_println(T_NOTFOUND);
    LoadDefaultPersistantData();
  } else {
    // file exists so open it
    File cfile = SPIFFS.open(file_cntlr_config, "r");

    String cdata;
    cdata.reserve(DEFAULTCONFIGSIZE);
    cdata = cfile.readString();
    cfile.close();
    CNTLRDATA_print("size ");
    CNTLRDATA_println(cdata.length());
    CNTLRDATA_print("data ");
    CNTLRDATA_println(cdata);

    // Allocate a temporary JsonDocument
    DynamicJsonDocument doc_per(DEFAULTCONFIGSIZE);
    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc_per, cdata);
    if (error) {
      CNTLRDATA_println(T_DESERIALISEERROR);
      LoadDefaultPersistantData();
    } else {
      // maxstep
      this->maxstep = doc_per["maxstep"];
      // presets
      for (int i = 0; i < 10; i++) {
        this->focuserpreset[i] = doc_per["preset"][i];
      }
      // SERVERS - SERVICES
      this->ascomsrvr_enable = doc_per["ascom_en"];
      this->ascomsrvr_port = doc_per["ascom_port"];
      this->debugsrvr_enable = doc_per["dbg_en"];
      this->debugsrvr_port = doc_per["dbg_port"];
      this->debugsrvr_out = doc_per["dbg_out"];
      this->duckdns_enable = doc_per["ddns_en"];
      this->duckdns_domain = doc_per["ddns_d"].as<const char *>();
      this->duckdns_refreshtime = doc_per["ddns_r"];
      this->duckdns_token = doc_per["ddns_t"].as<const char *>();
      this->mngsrvr_enable = doc_per["mngt_en"];
      this->mngsrvr_port = doc_per["mngt_port"];
      this->ota_id = doc_per["ota_id"].as<const char *>();
      this->ota_name = doc_per["ota_name"].as<const char *>();
      this->ota_password = doc_per["ota_pwd"].as<const char *>();
      this->tcpipsrvr_enable = doc_per["tcp_en"];
      this->tcpipsrvr_port = doc_per["tcp_port"];
      this->websrvr_enable = doc_per["ws_en"];
      this->websrvr_port = doc_per["ws_port"];
      // DEVICES
      // display
      this->display_enable = doc_per["d_en"];
      this->display_pageoption = doc_per["d_pgopt"].as<const char *>();
      this->display_pagetime = doc_per["d_pgtime"];
      this->display_updateonmove = doc_per["d_updmove"];
      // hpsw
      this->hpswitch_enable = doc_per["hpsw_en"];
      this->hpswmsg_enable = doc_per["hpswmsg_en"];
      this->stallguard_state = doc_per["stall_st"];
      this->stallguard_value = doc_per["stall_val"];
      this->tmc2209current = doc_per["tmc2209mA"];
      this->tmc2225current = doc_per["tmc2225mA"];
      // leds
      this->inoutled_enable = doc_per["led_en"];
      this->inoutled_mode = doc_per["led_mode"];
      // joysticks
      this->joystick1_enable = doc_per["joy1_en"];
      this->joystick2_enable = doc_per["joy2_en"];
      // pushbuttons
      this->pushbutton_enable = doc_per["pb_en"];
      this->pushbutton_steps = doc_per["pb_steps"];
      // temperature probe enable
      this->tempprobe_enable = doc_per["t_en"];
      // steps per degree temperature coefficient value
      this->tempcoefficient = doc_per["t_coe"];
      // indicates if temperature compensation is enabled
      this->tempcomp_enable = doc_per["t_comp_en"];
      // temperature display mode, Celcius=1, Fahrenheit=0
      this->tempmode = doc_per["t_mod"];
      // 9 - 12
      this->tempresolution = doc_per["t_res"];
      this->tcavailable = doc_per["t_tcavail"];
      this->tcdirection = doc_per["t_tcdir"];
      // backlash
      this->backlash_in_enable = doc_per["blin_en"];
      this->backlashsteps_in = doc_per["blin_steps"];
      this->backlash_out_enable = doc_per["blout_en"];
      this->backlashsteps_out = doc_per["blout_steps"];
      // coil power
      this->coilpower_enable = doc_per["cp_en"];
      // delay after move
      this->delayaftermove_enable = doc_per["dam-en"];
      this->delayaftermove_time = doc_per["dam_time"];
      // devicename
      this->devicename = doc_per["devname"].as<const char *>();
      // file list format
      this->filelistformat = doc_per["filelist"];
      // motorspeed slow, med, fast
      this->motorspeed = doc_per["mspeed"];
      // park
      this->park_enable = doc_per["park_en"];
      this->park_time = doc_per["park_time"];
      // reverse
      this->reverse_enable = doc_per["rdir_en"];
      // stepsize, if 1, controller returns step size
      this->stepsize_enable = doc_per["ss_en"];
      // stepsize in microns. ie 7.2 - value * 10, so real stepsize = stepsize / 10 (maxval = 25.6)
      this->stepsize = doc_per["ss_val"];
      // web page colors
      this->titlecolor = doc_per["ticol"].as<const char *>();
      this->subtitlecolor = doc_per["scol"].as<const char *>();
      this->headercolor = doc_per["hcol"].as<const char *>();
      this->textcolor = doc_per["tcol"].as<const char *>();
      this->backcolor = doc_per["bcol"].as<const char *>();
      CNTLRDATA_println(T_LOADED);
    }
  }

  // LOAD CONTROLLER BOARD DATA
  CNTLRDATA_print(T_LOAD);
  CNTLRDATA_println(file_board_config);
  if (SPIFFS.exists(file_board_config) == false) {
    CNTLRDATA_println(T_NOTFOUND);
    LoadDefaultBoardData();
  } else {
    // board_config.jsn board data
    // file exists so open it
    File bfile = SPIFFS.open(file_board_config, "r");
    String bdata;
    bdata.reserve(BOARDDATASIZE);

    bdata = bfile.readString();
    bfile.close();
    CNTLRDATA_print("-size ");
    CNTLRDATA_println(bdata.length());
    CNTLRDATA_print("-data ");
    CNTLRDATA_println(bdata);

    // Allocate a temporary JsonDocument
    DynamicJsonDocument doc_brd(BOARDDATASIZE);

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc_brd, bdata);
    if (error) {
      CNTLRDATA_println(T_DESERIALISEERROR);
      LoadDefaultBoardData();
    } else {
      /*
        { "board":"PRO2ESP32DRV8825","maxstepmode":32,"stepmode":1,"enpin":14,"steppin":33,
        "dirpin":32,"temppin":13,"hpswpin":4,"inledpin":18,"outledpin":19,"pb1pin":34,"pb2pin":35,"irpin":15,
        "brdnum":60, "stepsrev":-1,"fixedsmode":-1,"brdpins":[27,26,25,-1],"msdelay":4000 }
      */
      this->board = doc_brd["board"].as<const char *>();
      this->maxstepmode = doc_brd["maxstepmode"];
      this->stepmode = doc_brd["stepmode"];
      this->enablepin = doc_brd["enpin"];
      this->steppin = doc_brd["steppin"];
      this->dirpin = doc_brd["dirpin"];
      this->temppin = doc_brd["temppin"];
      this->hpswpin = doc_brd["hpswpin"];
      this->inledpin = doc_brd["inledpin"];
      this->outledpin = doc_brd["outledpin"];
      this->pb1pin = doc_brd["pb1pin"];
      this->pb2pin = doc_brd["pb2pin"];
      this->irpin = doc_brd["irpin"];
      this->boardnumber = doc_brd["brdnum"];
      this->stepsperrev = doc_brd["stepsrev"];
      this->fixedstepmode = doc_brd["fixedsmode"];
      for (int i = 0; i < 4; i++) {
        this->boardpins[i] = doc_brd["brdpins"][i];
      }
      this->msdelay = doc_brd["msdelay"];
      CNTLRDATA_println(T_LOADED);
    }
  }

  // LOAD CONTROLLER VAR DATA : POSITION : DIRECTION
  // this uses stepmode which is in boardconfig file so this must come after loading the board config
  CNTLRDATA_print(T_LOAD);
  CNTLRDATA_println(file_cntlr_var);

  if (SPIFFS.exists(file_cntlr_var) == false) {
    CNTLRDATA_println(T_NOTFOUND);
    LoadDefaultVariableData();
  } else {
    // controller variable data (position, direction)
    // file exists so open it
    File vfile = SPIFFS.open(file_cntlr_var, "r");
    String vdata;
    vdata.reserve(BOARDVARDATASIZE);

    vdata = vfile.readString();
    vfile.close();
    CNTLRDATA_print("-size ");
    CNTLRDATA_println(vdata.length());
    CNTLRDATA_print("-data ");
    CNTLRDATA_println(vdata);

    // Allocate a temporary JsonDocument
    DynamicJsonDocument doc_var(BOARDVARDATASIZE);

    // Deserialize the JSON document
    DeserializationError error = deserializeJson(doc_var, vdata);
    if (error) {
      CNTLRDATA_println(T_DESERIALISEERROR);
      LoadDefaultVariableData();
    } else {
      // get last focuser position and last focuser move direction
      this->fposition = doc_var["fpos"];
      this->focuserdirection = doc_var["fdir"];

      if (displaytype == Type_Graphic) {
        // round position to fullstep motor position, holgers code
        // only applicable if using a GRAPHICS Display
        this->fposition = (this->fposition + this->stepmode / 2) / this->stepmode * this->stepmode;
      }
      CNTLRDATA_println(T_LOADED);
    }
  }
  return true;
}


// ----------------------------------------------------------------------
// Load Focuser Variable Data - Position and Direction
// ----------------------------------------------------------------------
void CONTROLLER_DATA::LoadDefaultVariableData() {
  // last focuser position and last focuser move direction
  this->fposition = DEFAULTPOSITION;
  this->focuserdirection = moving_in;
  SaveVariableConfiguration();
}


// ----------------------------------------------------------------------
// Load Default Focuser Persistant Data Settings (when no config file exists eg; after an upload)
// Creates a default config setting file and saves it to spiffs
// ----------------------------------------------------------------------
void CONTROLLER_DATA::LoadDefaultPersistantData() {
  this->maxstep = DEFAULTMAXSTEPS;
  for (int i = 0; i < 10; i++) {
    this->focuserpreset[i] = 0;
  }

  // Set the initial defaults for a controller
  // SERVERS - SERVICES
  this->ascomsrvr_enable = V_NOTENABLED;
  this->ascomsrvr_port = ASCOMSERVERPORT;

  this->debugsrvr_enable = V_NOTENABLED;
  this->debugsrvr_port = debugsrvr_port;
  this->debugsrvr_out = DEBUGSERVEROUTPUTPORT;

  // DUCKDNS from /defines/duckdns_defines.h
  this->duckdns_enable = V_NOTENABLED;
  this->duckdns_domain = String(duckdnsdomain);
  this->duckdns_refreshtime = DUCKDNS_REFRESHRATE;
  this->duckdns_token = String(duckdnstoken);

  this->mngsrvr_enable = V_ENABLED;
  this->mngsrvr_port = MNGSERVERPORT;

  // OTA from /defines/otaupdates_defines.h
  this->ota_id = String(OTAID);
  this->ota_name = String(OTAName);
  this->ota_password = String(OTAPassword);

  this->tcpipsrvr_enable = V_ENABLED;
  this->tcpipsrvr_port = TCPIPSERVERPORT;

  this->websrvr_enable = V_NOTENABLED;
  this->websrvr_port = WEBSERVERPORT;

  // DISPLAY
  this->display_enable = V_NOTENABLED;
  this->display_pageoption = "11111111";
  this->display_pagetime = V_DISPLAYPAGETIMEMIN;
  this->display_updateonmove = V_ENABLED;

  // HPSW
  this->hpswitch_enable = V_NOTENABLED;
  this->hpswmsg_enable = V_NOTENABLED;

  // stall guard
  this->stallguard_state = Use_None;
  this->stallguard_value = STALL_VALUE;
  // tmc2209 and tmc2225 from boarddefs.h
  this->tmc2209current = TMC2209CURRENT;
  this->tmc2225current = TMC2225CURRENT;

  // LEDS
  this->inoutled_enable = V_NOTENABLED;
  this->inoutled_mode = LEDPULSE;

  // JOYSTICKS
  this->joystick1_enable = V_NOTENABLED;
  this->joystick2_enable = V_NOTENABLED;

  // PUSHBUTTONS
  this->pushbutton_enable = V_NOTENABLED;
  this->pushbutton_steps = PUSHBUTTON_STEPS;

  // TEMPERATURE PROBE
  this->tempprobe_enable = V_NOTENABLED;
  this->tempcoefficient = DEFAULT_FALSE;
  this->tempcomp_enable = V_NOTENABLED;
  this->tempmode = V_CELSIUS;
  this->tempresolution = DEFAULTTEMPRESOLUTION;
  this->tcavailable = V_NOTENABLED;
  this->tcdirection = TC_DIRECTION_IN;

  // BACKLASH
  this->backlash_in_enable = V_NOTENABLED;
  this->backlashsteps_in = DEFAULT_FALSE;
  this->backlash_out_enable = V_NOTENABLED;
  this->backlashsteps_out = DEFAULT_FALSE;
  // coil power
  this->coilpower_enable = V_NOTENABLED;
  // delay after move
  this->delayaftermove_enable = V_NOTENABLED;
  this->delayaftermove_time = 25;
  // devicename
  this->devicename = String(project_name);
  // file list format
  this->filelistformat = LISTLONG;
  // motorspeed
  this->motorspeed = FAST;
  // park
  this->park_enable = V_NOTENABLED;
  this->park_time = DEFAULTPARKTIME;
  // reverse
  this->reverse_enable = V_NOTENABLED;
  // stepsize
  this->stepsize_enable = V_NOTENABLED;
  this->stepsize = DEFAULTSTEPSIZE;
  // web page colors
  this->titlecolor = DEFAULTTITLECOLOR;
  this->subtitlecolor = DEFAULTSUBTITLECOLOR;
  this->headercolor = DEFAULTHEADERCOLOR;
  this->textcolor = DEFAULTTEXTCOLLOR;
  this->backcolor = DEFAULTBACKCOLOR;

  SavePersitantConfiguration();
}


// ----------------------------------------------------------------------
// Load Default Board Data Settings
// ----------------------------------------------------------------------
void CONTROLLER_DATA::LoadDefaultBoardData() {
  // we are here because board_config.jsn was not found
  // we can load the default board configuration from DRVBRD defined - DefaultBoardName in .ino file
  // Driver board data - Open the specific board config .jsn file for reading

  // cannot use this->boardnumber because the value has not been set yet
  // Load the board file from /boards, make up filename first
  String brdfile = "/boards/" + String(myboardnumber) + ".jsn";
  CNTLRDATA_println("CD LoadDefaultBoardData");
  CNTLRDATA_print(T_LOAD);
  CNTLRDATA_println(brdfile);

  // attempt to load the specified board config file from /boards
  if (LoadBrdConfigStart(brdfile) == true) {
    CNTLRDATA_println(T_LOADED);
  } else {
    // a board config file could not be loaded, so create a dummy one
    this->board = "Unknown";
    this->maxstepmode = -1;
    this->stepmode = 1;
    this->enablepin = -1;
    this->steppin = -1;
    this->dirpin = -1;
    this->temppin = -1;
    this->hpswpin = -1;
    this->inledpin = -1;
    this->outledpin = -1;
    this->pb1pin = -1;
    this->pb1pin = -1;
    this->irpin = -1;
    // captured from controller_config.h
    this->boardnumber = myboardnumber;
    this->fixedstepmode = myfixedstepmode;
    this->stepsperrev = mystepsperrev;
    for (int i = 0; i < 4; i++) {
      this->boardpins[i] = -1;
    }
    this->msdelay = 8000;
  }
  SaveBoardConfiguration();
}


// ----------------------------------------------------------------------
// Reset focuser settings to defaults : tcpip_server.cpp case 42:
// ----------------------------------------------------------------------
void CONTROLLER_DATA::SetFocuserDefaults(void) {
  if (SPIFFS.exists(file_cntlr_config)) {
    SPIFFS.remove(file_cntlr_config);
  }
  if (SPIFFS.exists(file_board_config)) {
    SPIFFS.remove(file_board_config);
  }
  if (SPIFFS.exists(file_cntlr_var)) {
    SPIFFS.remove(file_cntlr_var);
  }
  LoadDefaultPersistantData();
  LoadDefaultBoardData();
  LoadDefaultVariableData();
}


// ----------------------------------------------------------------------
// Saves the configurations to files
// Called externally from loop() to save config files  after time elapsed
// ----------------------------------------------------------------------
bool CONTROLLER_DATA::SaveConfiguration(long currentPosition, byte DirOfTravel) {
  bool state = false;

  if (this->fposition != currentPosition || this->focuserdirection != DirOfTravel)  // last focuser position
  {
    this->fposition = currentPosition;
    this->focuserdirection = DirOfTravel;
    // set flag to start 30s counter for saving var file
    portENTER_CRITICAL(&varMux);
    save_var_flag = 0;
    portEXIT_CRITICAL(&varMux);
  }

  // check the flags to determine what needs to be saved

  // only if the focuser is not moving
  if (isMoving == true) {
    CNTLRDATA_println("CD-Save delayed: isMoving");
    state = false;
  } else {
    int t_mux;

    // not moving, safe to save files
    // check var flag, if saving of file cntlr_var.jsn is required
    portENTER_CRITICAL(&varMux);
    t_mux = save_var_flag;
    portEXIT_CRITICAL(&varMux);
    if (t_mux == 1) {
      portENTER_CRITICAL(&varMux);
      save_var_flag = -1;
      portEXIT_CRITICAL(&varMux);
      if (ControllerData->SaveVariableConfiguration() == false) {
        state = false;
      } else {
        state = true;
      }
    }

    // check cntlr flag
    portENTER_CRITICAL(&cntlrMux);
    t_mux = save_cntlr_flag;
    portEXIT_CRITICAL(&cntlrMux);
    if (t_mux == 1) {
      portENTER_CRITICAL(&cntlrMux);
      save_cntlr_flag = -1;
      portEXIT_CRITICAL(&cntlrMux);
      if (ControllerData->SavePersitantConfiguration() == false) {
        state = false;
      } else {
        state = true;
      }
    }

    // check board flag
    portENTER_CRITICAL(&boardMux);
    t_mux = save_board_flag;
    portEXIT_CRITICAL(&boardMux);
    if (t_mux == 1) {
      portENTER_CRITICAL(&boardMux);
      save_board_flag = -1;
      portEXIT_CRITICAL(&boardMux);
      if (ControllerData->SaveBoardConfiguration() == false) {
        state = false;
      } else {
        state = true;
      }
    }
  }
  return state;
}


// ----------------------------------------------------------------------
// Save configuration files immediately (like in the case for reboot()
// ----------------------------------------------------------------------
bool CONTROLLER_DATA::SaveNow(long focuser_position, bool focuser_direction) {
  SaveBoardConfiguration();
  SaveVariableConfiguration();
  return SavePersitantConfiguration();
}


// ----------------------------------------------------------------------
// Save variable data (position, dir travel) settings to cntlr_var.jsn
// ----------------------------------------------------------------------
bool CONTROLLER_DATA::SaveVariableConfiguration() {
  CNTLRDATA_print("CD SaveVariableConfiguration ");
  CNTLRDATA_println(file_cntlr_var);

  // Delete existing file
  if (SPIFFS.exists(file_cntlr_var)) {
    SPIFFS.remove(file_cntlr_var);
  }

  // Open file for writing
  CNTLRDATA_print(T_OPENFILE);
  CNTLRDATA_println(file_cntlr_var);
  File vfile = SPIFFS.open(file_cntlr_var, "w");
  if (!vfile) {
    CNTLRDATA_println(T_ERROR);
    return false;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/assistant to compute the capacity.
  StaticJsonDocument<BOARDVARDATASIZE> doc;

  // Set the values in the document
  doc["fpos"] = this->fposition;
  doc["fdir"] = this->focuserdirection;

  // save settings to file
  if (serializeJson(doc, vfile) == 0) {
    CNTLRDATA_println(T_DESERIALISEERROR);
    vfile.close();
    return false;
  } else {
    CNTLRDATA_print(T_SAVED);
    CNTLRDATA_println(file_cntlr_var);
    vfile.close();
    return true;
  }
  return false;
}


// ----------------------------------------------------------------------
// Save Focuser Controller (persistent) Data to file cntlr_config.jsn
// ----------------------------------------------------------------------
bool CONTROLLER_DATA::SavePersitantConfiguration() {
  CNTLRDATA_println("CD SavePersitantConfiguration ");
  CNTLRDATA_println(file_cntlr_var);

  if (SPIFFS.exists(file_cntlr_config)) {
    SPIFFS.remove(file_cntlr_config);
  }

  // Open file for writing
  CNTLRDATA_println(T_OPENFILE);
  File file = SPIFFS.open(file_cntlr_config, "w");
  if (!file) {
    CNTLRDATA_println(T_OPENERROR);
    return false;
  }

  // Allocate a temporary JsonDocument
  // Don't forget to change the capacity to match your requirements.
  // Use arduinojson.org/assistant to compute the capacity.
  // 303 - 1170, Size 1536
  StaticJsonDocument<DEFAULTCONFIGSIZE> doc;

  doc["maxstep"] = this->maxstep;
  for (int i = 0; i < 10; i++) {
    doc["preset"][i] = this->focuserpreset[i];
  };
  // SERVERS - SERVICES
  doc["ascom_en"] = this->ascomsrvr_enable;
  doc["ascom_port"] = this->ascomsrvr_port;
  doc["dbg_en"] = this->debugsrvr_enable;
  doc["dbg_port"] = this->debugsrvr_port;
  doc["dbg_out"] = this->debugsrvr_out;
  doc["ddns_en"] = this->duckdns_enable;
  doc["ddns_d"] = this->duckdns_domain;
  doc["ddns_r"] = this->duckdns_refreshtime;
  doc["ddns_t"] = this->duckdns_token;
  doc["mngt_en"] = this->mngsrvr_enable;
  doc["mngt_port"] = this->mngsrvr_port;
  doc["ota_id"] = this->ota_id;
  doc["ota_name"] = this->ota_name;
  doc["ota_pwd"] = this->ota_password;
  doc["tcp_en"] = this->tcpipsrvr_enable;
  doc["tcp_port"] = this->tcpipsrvr_port;
  doc["ws_en"] = this->websrvr_enable;
  doc["ws_port"] = this->websrvr_port;

  // DEVICES
  // display
  doc["d_en"] = this->display_enable;
  doc["d_pgopt"] = this->display_pageoption;
  doc["d_pgtime"] = this->display_pagetime;
  doc["d_updmove"] = this->display_updateonmove;
  // hpsw
  doc["hpsw_en"] = this->hpswitch_enable;
  doc["hpswmsg_en"] = this->hpswitch_enable;
  // stall guard
  doc["stall_st"] = this->stallguard_state;
  doc["stall_val"] = this->stallguard_value;
  // tmc currents
  doc["tmc2209mA"] = this->tmc2209current;
  doc["tmc2225mA"] = this->tmc2225current;
  // leds
  doc["led_en"] = this->inoutled_enable;
  doc["led_mode"] = this->inoutled_mode;
  // joysticks
  doc["joy1_en"] = this->joystick1_enable;
  doc["joy2_en"] = this->joystick2_enable;
  // pushbuttons
  doc["pb_en"] = this->pushbutton_enable;
  doc["pb_steps"] = this->pushbutton_steps;
  // temperature probe
  doc["t_en"] = this->tempprobe_enable;
  doc["t_coe"] = this->tempcoefficient;
  doc["t_comp_en"] = this->tempcomp_enable;
  doc["t_mod"] = this->tempmode;
  doc["t_res"] = this->tempresolution;
  doc["t_tcavail"] = this->tcavailable;
  doc["t_tcdir"] = this->tcdirection;
  // backlash
  doc["blin_en"] = this->backlash_in_enable;
  doc["blin_steps"] = this->backlashsteps_in;
  doc["blout_en"] = this->backlash_out_enable;
  doc["blout_steps"] = this->backlashsteps_out;
  // coil power
  doc["cp_en"] = this->coilpower_enable;
  // delay after move
  doc["dam_en"] = this->delayaftermove_enable;
  doc["dam_time"] = this->delayaftermove_time;
  // devicename
  doc["devname"] = this->devicename;
  // file list format
  doc["filelist"] = this->filelistformat;
  // motorspeed
  doc["mspeed"] = this->motorspeed;
  // park
  doc["park_en"] = this->park_enable;
  doc["park_time"] = this->park_time;
  // reverse
  doc["rdir_en"] = this->reverse_enable;
  // stepsize
  doc["ss_en"] = this->stepsize_enable;
  doc["ss_val"] = this->stepsize;
  // web page colors
  doc["ticol"] = this->titlecolor;
  doc["scol"] = this->subtitlecolor;
  doc["hcol"] = this->headercolor;
  doc["tcol"] = this->textcolor;
  doc["bcol"] = this->backcolor;
  // Serialize JSON to file
  if (serializeJson(doc, file) == 0) {
    CNTLRDATA_println(T_DESERIALISEERROR);
    file.close();
    return false;
  } else {
    CNTLRDATA_print(T_SAVED);
    CNTLRDATA_println(file_cntlr_config);
    file.close();
    return true;
  }
}


// ----------------------------------------------------------------------
// Save Board Data to file board_config.jsn
// ----------------------------------------------------------------------
bool CONTROLLER_DATA::SaveBoardConfiguration() {

  CNTLRDATA_print("CD SaveBoardConfiguration ");
  CNTLRDATA_println(file_board_config);

  if (SPIFFS.exists(file_board_config)) {
    SPIFFS.remove(file_board_config);
  } else {
    CNTLRDATA_println(T_NOTFOUND);
  }

  // Open file for writing
  CNTLRDATA_println(T_OPENFILE);
  File bfile = SPIFFS.open(file_board_config, "w");
  if (!bfile) {
    CNTLRDATA_println(T_OPENERROR);
    return false;
  } else {
    // Allocate a temporary JsonDocument
    // Don't forget to change the capacity to match your requirements.
    // Use arduinojson.org/assistant to compute the capacity.
    StaticJsonDocument<BOARDDATASIZE> doc_brd;
    // Set the values in the document
    doc_brd["board"] = this->board;
    doc_brd["maxstepmode"] = this->maxstepmode;
    doc_brd["stepmode"] = this->stepmode;
    doc_brd["enpin"] = this->enablepin;
    doc_brd["steppin"] = this->steppin;
    doc_brd["dirpin"] = this->dirpin;
    doc_brd["temppin"] = this->temppin;
    doc_brd["hpswpin"] = this->hpswpin;
    doc_brd["inledpin"] = this->inledpin;
    doc_brd["outledpin"] = this->outledpin;
    doc_brd["pb1pin"] = this->pb1pin;
    doc_brd["pb2pin"] = this->pb2pin;
    doc_brd["irpin"] = this->irpin;
    doc_brd["brdnum"] = this->boardnumber;
    doc_brd["stepsrev"] = this->stepsperrev;
    doc_brd["fixedsmode"] = this->fixedstepmode;
    for (int i = 0; i < 4; i++) {
      doc_brd["brdpins"][i] = this->boardpins[i];
    }
    doc_brd["msdelay"] = this->msdelay;

    // Serialize JSON to file
    if (serializeJson(doc_brd, bfile) == 0) {
      CNTLRDATA_println(T_DESERIALISEERROR);
      bfile.close();
      return false;
    } else {
      CNTLRDATA_print(T_SAVED);
      CNTLRDATA_println(file_board_config);
      bfile.close();
    }
  }
  return true;
}


// ----------------------------------------------------------------------
// Controller_Data Methods
// ----------------------------------------------------------------------

// FOCUSER POSITION
long CONTROLLER_DATA::get_fposition() {
  return this->fposition;
}

void CONTROLLER_DATA::set_fposition(long fposition) {
  this->fposition = fposition;
  portENTER_CRITICAL(&varMux);
  save_var_flag = 0;
  portEXIT_CRITICAL(&varMux);
}

// FOCUSER DIRECTION
byte CONTROLLER_DATA::get_focuserdirection() {
  return this->focuserdirection;
}

void CONTROLLER_DATA::set_focuserdirection(byte newdir) {
  this->focuserdirection = newdir;
}

// MAXSTEPS
long CONTROLLER_DATA::get_maxstep() {
  return this->maxstep;
}

void CONTROLLER_DATA::set_maxstep(long newval) {
  this->StartDelayedUpdate(this->maxstep, newval);
}

// PRESETS
long CONTROLLER_DATA::get_focuserpreset(byte idx) {
  return this->focuserpreset[idx % 10];
}

void CONTROLLER_DATA::set_focuserpreset(byte idx, long pos) {
  this->StartDelayedUpdate(this->focuserpreset[idx % 10], pos);
}

// ASCOM ALPACA SERVER
byte CONTROLLER_DATA::get_ascomsrvr_enable(void) {
  return this->ascomsrvr_enable;
}

void CONTROLLER_DATA::set_ascomsrvr_enable(byte newstate) {
  this->StartDelayedUpdate(this->ascomsrvr_enable, newstate);
}

unsigned long CONTROLLER_DATA::get_ascomsrvr_port(void) {
  return this->ascomsrvr_port;
}

void CONTROLLER_DATA::set_ascomsrvr_port(unsigned long newport) {
  this->StartDelayedUpdate(this->ascomsrvr_port, newport);
}

// BACKLASH
byte CONTROLLER_DATA::get_backlash_in_enable(void) {
  return this->backlash_in_enable;
  // apply backlash when moving in [0=!enabled, 1=enabled]
}

void CONTROLLER_DATA::set_backlash_in_enable(byte newstate) {
  this->StartDelayedUpdate(this->backlash_in_enable, newstate);
}

byte CONTROLLER_DATA::get_backlash_out_enable(void) {
  return this->backlash_out_enable;
  // apply backlash when moving out [0=!enabled, 1=enabled]
}

void CONTROLLER_DATA::set_backlash_out_enable(byte newstate) {
  this->StartDelayedUpdate(this->backlash_out_enable, newstate);
}

byte CONTROLLER_DATA::get_backlashsteps_in(void) {
  return this->backlashsteps_in;
}

void CONTROLLER_DATA::set_backlashsteps_in(byte newval) {
  this->StartDelayedUpdate(this->backlashsteps_in, newval);
}

byte CONTROLLER_DATA::get_backlashsteps_out(void) {
  return this->backlashsteps_out;
}

void CONTROLLER_DATA::set_backlashsteps_out(byte newval) {
  this->StartDelayedUpdate(this->backlashsteps_out, newval);
}

// COILPOWER
byte CONTROLLER_DATA::get_coilpower_enable(void) {
  return this->coilpower_enable;
  // state of coil power, 0 = !enabled, 1= enabled
}

void CONTROLLER_DATA::set_coilpower_enable(byte newstate) {
  this->StartDelayedUpdate(this->coilpower_enable, newstate);
}

// DELAY AFTER MOVE
byte CONTROLLER_DATA::get_delayaftermove_enable(void) {
  return this->delayaftermove_enable;
}

void CONTROLLER_DATA::set_delayaftermove_enable(byte newstate) {
  this->StartDelayedUpdate(this->delayaftermove_enable, newstate);
}

byte CONTROLLER_DATA::get_delayaftermove_time(void) {
  return this->delayaftermove_time;
}

void CONTROLLER_DATA::set_delayaftermove_time(byte newtime) {
  this->StartDelayedUpdate(this->delayaftermove_time, newtime);
}

// DEBUG SERVER
byte CONTROLLER_DATA::get_debugsrvr_enable(void) {
  return this->debugsrvr_enable;
}

void CONTROLLER_DATA::set_debugsrvr_enable(byte newstate) {
  this->StartDelayedUpdate(this->debugsrvr_enable, newstate);
}

unsigned long CONTROLLER_DATA::get_debugsrvr_port() {
  return this->debugsrvr_port;
}

void CONTROLLER_DATA::set_debugsrvr_port(unsigned long newport) {
  this->StartDelayedUpdate(this->debugsrvr_port, newport);
}

byte CONTROLLER_DATA::get_debugsrvr_out() {
  return this->debugsrvr_out;
}

void CONTROLLER_DATA::set_debugsrvr_out(byte newout) {
  this->StartDelayedUpdate(this->debugsrvr_out, newout);
}

// DEVICENAME
String CONTROLLER_DATA::get_devicename(void) {
  return this->devicename;
}

void CONTROLLER_DATA::set_devicename(String newname) {
  this->StartDelayedUpdate(this->devicename, newname);
}

// DISPLAY
byte CONTROLLER_DATA::get_display_enable(void) {
  return this->display_enable;
}

void CONTROLLER_DATA::set_display_enable(byte newstate) {
  this->StartDelayedUpdate(this->display_enable, newstate);
}

int CONTROLLER_DATA::get_display_pagetime(void) {
  return this->display_pagetime;
}

void CONTROLLER_DATA::set_display_pagetime(int newtime) {
  portENTER_CRITICAL(&displaytimeMux);
  display_maxcount = newtime * 10;
  portEXIT_CRITICAL(&displaytimeMux);
  this->StartDelayedUpdate(this->display_pagetime, newtime);
}

String CONTROLLER_DATA::get_display_pageoption(void) {
  String tmp = this->display_pageoption;
  while (tmp.length() < 8) {
    tmp = tmp + "0";
  }
  tmp = tmp + "";
  this->display_pageoption = tmp;
  return this->display_pageoption;
}

void CONTROLLER_DATA::set_display_pageoption(String newoption) {
  String tmp = newoption;
  while (tmp.length() < 8) {
    tmp = tmp + "0";
  }
  tmp = tmp + "";
  this->display_pageoption = tmp;
  this->StartDelayedUpdate(this->display_pageoption, newoption);
}

byte CONTROLLER_DATA::get_display_updateonmove(void) {
  return this->display_updateonmove;
}

void CONTROLLER_DATA::set_display_updateonmove(byte newstate) {
  this->StartDelayedUpdate(this->display_updateonmove, newstate);
}

// DUCKDNS
byte CONTROLLER_DATA::get_duckdns_enable(void) {
  return this->duckdns_enable;
}

void CONTROLLER_DATA::set_duckdns_enable(byte newstate) {
  this->StartDelayedUpdate(this->duckdns_enable, newstate);
}

String CONTROLLER_DATA::get_duckdns_domain(void) {
  return this->duckdns_domain;
}

void CONTROLLER_DATA::set_duckdns_domain(String newdomain) {
  this->StartDelayedUpdate(this->duckdns_domain, newdomain);
}

String CONTROLLER_DATA::get_duckdns_token(void) {
  return this->duckdns_token;
}

void CONTROLLER_DATA::set_duckdns_token(String newtoken) {
  this->StartDelayedUpdate(this->duckdns_token, newtoken);
}

unsigned int CONTROLLER_DATA::get_duckdns_refreshtime(void) {
  return this->duckdns_refreshtime;
}

void CONTROLLER_DATA::set_duckdns_refreshtime(unsigned int newtime) {
  this->StartDelayedUpdate(this->duckdns_refreshtime, newtime);
}

// FILE LIST FORMAT
byte CONTROLLER_DATA::get_filelistformat(void) {
  return this->filelistformat;
}

void CONTROLLER_DATA::set_filelistformat(byte newlistformat) {
  this->StartDelayedUpdate(this->filelistformat, newlistformat);
}

// HOME POSITION SWITCH
byte CONTROLLER_DATA::get_hpswitch_enable(void) {
  return this->hpswitch_enable;
}

void CONTROLLER_DATA::set_hpswitch_enable(byte newstate) {
  this->StartDelayedUpdate(this->hpswitch_enable, newstate);
}

byte CONTROLLER_DATA::get_hpswmsg_enable(void) {
  return this->hpswmsg_enable;
}

void CONTROLLER_DATA::set_hpswmsg_enable(byte newstate) {
  this->StartDelayedUpdate(this->hpswmsg_enable, newstate);
}

// JOYSTICKS
byte CONTROLLER_DATA::get_joystick1_enable(void) {
  return this->joystick1_enable;
}

void CONTROLLER_DATA::set_joystick1_enable(byte newstate) {
  this->StartDelayedUpdate(this->joystick1_enable, newstate);
}

byte CONTROLLER_DATA::get_joystick2_enable(void) {
  return this->joystick2_enable;
}

void CONTROLLER_DATA::set_joystick2_enable(byte newstate) {
  this->StartDelayedUpdate(this->joystick2_enable, newstate);
}

// LEDS
byte CONTROLLER_DATA::get_inoutled_enable(void) {
  return this->inoutled_enable;
}

void CONTROLLER_DATA::set_inoutled_enable(byte newstate) {
  this->StartDelayedUpdate(this->inoutled_enable, newstate);
}

byte CONTROLLER_DATA::get_inoutled_mode(void) {
  return this->inoutled_mode;
}

void CONTROLLER_DATA::set_inoutled_mode(byte newmode) {
  this->StartDelayedUpdate(this->inoutled_mode, newmode);
}

// MANAGEMENT SERVER
byte CONTROLLER_DATA::get_mngsrvr_enable(void) {
  return this->mngsrvr_enable;
}

void CONTROLLER_DATA::set_mngsrvr_enable(byte newstate) {
  this->StartDelayedUpdate(this->mngsrvr_enable, newstate);
}

unsigned long CONTROLLER_DATA::get_mngsrvr_port(void) {
  return this->mngsrvr_port;
}

void CONTROLLER_DATA::set_mngsrvr_port(unsigned long newport) {
  this->StartDelayedUpdate(this->mngsrvr_port, newport);
}

// MOTOR SPEED
byte CONTROLLER_DATA::get_motorspeed(void) {
  return this->motorspeed;
}

void CONTROLLER_DATA::set_motorspeed(byte newval) {
  this->StartDelayedUpdate(this->motorspeed, newval);
}

// OTA
String CONTROLLER_DATA::get_ota_name(void) {
  return this->ota_name;
}

void CONTROLLER_DATA::set_ota_name(String newname) {
  this->StartDelayedUpdate(this->ota_name, newname);
}

String CONTROLLER_DATA::get_ota_password(void) {
  return this->ota_password;
}

void CONTROLLER_DATA::set_ota_password(String newpwd) {
  this->StartDelayedUpdate(this->ota_password, newpwd);
}

String CONTROLLER_DATA::get_ota_id(void) {
  return this->ota_id;
}

void CONTROLLER_DATA::set_ota_id(String newid) {
  this->StartDelayedUpdate(this->ota_id, newid);
}

// PARK
byte CONTROLLER_DATA::get_park_enable(void) {
  return this->park_enable;
}

void CONTROLLER_DATA::set_park_enable(byte newstate) {
  this->StartDelayedUpdate(this->park_enable, newstate);
}

int CONTROLLER_DATA::get_parktime(void) {
  return this->park_time;
}

void CONTROLLER_DATA::set_parktime(int newtime) {
  this->StartDelayedUpdate(this->park_time, newtime);
}

// PUSH BUTTONS
byte CONTROLLER_DATA::get_pushbutton_enable(void) {
  return this->pushbutton_enable;
}

void CONTROLLER_DATA::set_pushbutton_enable(byte newstate) {
  this->StartDelayedUpdate(this->pushbutton_enable, newstate);
}

int CONTROLLER_DATA::get_pushbutton_steps(void) {
  return this->pushbutton_steps;
}

void CONTROLLER_DATA::set_pushbutton_steps(int newval) {
  this->StartDelayedUpdate(this->pushbutton_steps, newval);
}

// REVERSE
byte CONTROLLER_DATA::get_reverse_enable(void) {
  return this->reverse_enable;
  // state for reverse direction, 0 = !enabled, 1= enabled
}

void CONTROLLER_DATA::set_reverse_enable(byte newstate) {
  this->StartDelayedUpdate(this->reverse_enable, newstate);
}

// STEPMODE


// STEPSIZE
byte CONTROLLER_DATA::get_stepsize_enable(void) {
  return this->stepsize_enable;
  // if 1, controller returns step size
}

void CONTROLLER_DATA::set_stepsize_enable(byte newstate) {
  this->StartDelayedUpdate(this->stepsize_enable, newstate);
}

float CONTROLLER_DATA::get_stepsize(void) {
  return this->stepsize;
  // this is the actual measured focuser stepsize in microns amd is reported to ASCOM, so must be valid
  // the amount in microns that the focuser tube moves in one step of the motor
}

void CONTROLLER_DATA::set_stepsize(float newval) {
  this->StartDelayedUpdate(this->stepsize, newval);
  // the step size in microns, ie 7.2 - value * 10, so real stepsize = stepsize / 10 (maxval = 25.6)
}

// TCPIP SERVER
byte CONTROLLER_DATA::get_tcpipsrvr_enable(void) {
  return this->tcpipsrvr_enable;
}

void CONTROLLER_DATA::set_tcpipsrvr_enable(byte newstate) {
  this->StartDelayedUpdate(this->tcpipsrvr_enable, newstate);
}

unsigned long CONTROLLER_DATA::get_tcpipsrvr_port(void) {
  return this->tcpipsrvr_port;
}

void CONTROLLER_DATA::set_tcpipsrvr_port(unsigned long newport) {
  this->StartDelayedUpdate(this->tcpipsrvr_port, newport);
}

// TEMPERATURE
byte CONTROLLER_DATA::get_tempmode(void) {
  return this->tempmode;
  // temperature display mode, Celcius=1, Fahrenheit=0
}

void CONTROLLER_DATA::set_tempmode(byte newmode) {
  this->StartDelayedUpdate(this->tempmode, newmode);
  // temperature display mode, Celcius=1, Fahrenheit=0
}

byte CONTROLLER_DATA::get_tempprobe_enable(void) {
  return this->tempprobe_enable;
}

void CONTROLLER_DATA::set_tempprobe_enable(byte newstate) {
  this->StartDelayedUpdate(this->tempprobe_enable, newstate);
}

byte CONTROLLER_DATA::get_tempcomp_enable(void) {
  return this->tempcomp_enable;
  // indicates if temperature compensation is enabled
}

void CONTROLLER_DATA::set_tempcomp_enable(byte newstate) {
  this->StartDelayedUpdate(this->tempcomp_enable, newstate);
}

int CONTROLLER_DATA::get_tempcoefficient(void) {
  return this->tempcoefficient;
  // steps per degree temperature coefficient value (maxval=256)
}

void CONTROLLER_DATA::set_tempcoefficient(int newval) {
  this->StartDelayedUpdate(this->tempcoefficient, newval);
  // steps per degree temperature coefficient value (maxval=256)
}

byte CONTROLLER_DATA::get_tempresolution(void) {
  return this->tempresolution;
  // resolution of temperature measurement 9-12
}

void CONTROLLER_DATA::set_tempresolution(byte newval) {
  this->StartDelayedUpdate(this->tempresolution, newval);
}

byte CONTROLLER_DATA::get_tcdirection(void) {
  return this->tcdirection;
  // indicates the direction in which temperature compensation is applied
}

void CONTROLLER_DATA::set_tcdirection(byte newdirection) {
  this->StartDelayedUpdate(this->tcdirection, newdirection);
}

byte CONTROLLER_DATA::get_tcavailable(void) {
  return this->tcavailable;
}

void CONTROLLER_DATA::set_tcavailable(byte newval) {
  this->StartDelayedUpdate(this->tcavailable, newval);
}

// TMC STEPPERS
tmc2209stallguard CONTROLLER_DATA::get_stallguard_state(void) {
  return this->stallguard_state;
}

void CONTROLLER_DATA::set_stallguard_state(tmc2209stallguard newstate) {
  this->StartDelayedUpdate(this->stallguard_state, newstate);
}

byte CONTROLLER_DATA::get_stallguard_value(void) {
  return this->stallguard_value;
}

void CONTROLLER_DATA::set_stallguard_value(byte newval) {
  this->StartDelayedUpdate(this->stallguard_value, newval);
}

int CONTROLLER_DATA::get_tmc2225current(void) {
  return this->tmc2225current;
}

void CONTROLLER_DATA::set_tmc2225current(int newval) {
  this->StartDelayedUpdate(this->tmc2225current, newval);
}

int CONTROLLER_DATA::get_tmc2209current(void) {
  return this->tmc2209current;
}

void CONTROLLER_DATA::set_tmc2209current(int newval) {
  this->StartDelayedUpdate(this->tmc2209current, newval);
}

// WEB SERVER
byte CONTROLLER_DATA::get_websrvr_enable(void) {
  return this->websrvr_enable;
}

void CONTROLLER_DATA::set_websrvr_enable(byte newstate) {
  this->StartDelayedUpdate(this->websrvr_enable, newstate);
}

unsigned long CONTROLLER_DATA::get_websrvr_port(void) {
  return this->websrvr_port;
}

void CONTROLLER_DATA::set_websrvr_port(unsigned long newport) {
  this->StartDelayedUpdate(this->websrvr_port, newport);
}

// WEB PAGE COLORS
String CONTROLLER_DATA::get_wp_backcolor(void) {
  return this->backcolor;
}

void CONTROLLER_DATA::set_wp_backcolor(String newcolor) {
  this->StartDelayedUpdate(this->backcolor, newcolor);
}

String CONTROLLER_DATA::get_wp_textcolor(void) {
  return this->textcolor;
}

void CONTROLLER_DATA::set_wp_textcolor(String newcolor) {
  this->StartDelayedUpdate(this->textcolor, newcolor);
}

String CONTROLLER_DATA::get_wp_headercolor(void) {
  return this->headercolor;
}

void CONTROLLER_DATA::set_wp_headercolor(String newcolor) {
  this->StartDelayedUpdate(this->headercolor, newcolor);
}

String CONTROLLER_DATA::get_wp_titlecolor(void) {
  return this->titlecolor;
}

void CONTROLLER_DATA::set_wp_titlecolor(String newcolor) {
  this->StartDelayedUpdate(this->titlecolor, newcolor);
}

String CONTROLLER_DATA::get_wp_subtitlecolor(void) {
  return this->subtitlecolor;
}

void CONTROLLER_DATA::set_wp_subtitlecolor(String newcolor) {
  this->StartDelayedUpdate(this->subtitlecolor, newcolor);
}


// ----------------------------------------------------------------------
// Delayed Write routines which update the focuser setting with the
// new value, then sets a flag for when the data should be written to file
// ----------------------------------------------------------------------
void CONTROLLER_DATA::StartDelayedUpdate(int &org_data, int new_data) {
  if (org_data != new_data) {
    org_data = new_data;
    this->set_cntlr_flags();
  }
}

void CONTROLLER_DATA::StartDelayedUpdate(tmc2209stallguard &org_data, tmc2209stallguard new_data) {
  if (org_data != new_data) {
    org_data = new_data;
    this->set_cntlr_flags();
  }
}

void CONTROLLER_DATA::StartDelayedUpdate(unsigned int &org_data, unsigned int new_data) {
  if (org_data != new_data) {
    org_data = new_data;
    this->set_cntlr_flags();
  }
}

void CONTROLLER_DATA::StartDelayedUpdate(long &org_data, long new_data) {
  if (org_data != new_data) {
    org_data = new_data;
    this->set_cntlr_flags();
  }
}

void CONTROLLER_DATA::StartDelayedUpdate(unsigned long &org_data, unsigned long new_data) {
  if (org_data != new_data) {
    org_data = new_data;
    this->set_cntlr_flags();
  }
}

void CONTROLLER_DATA::StartDelayedUpdate(float &org_data, float new_data) {
  if (org_data != new_data) {
    org_data = new_data;
    this->set_cntlr_flags();
  }
}

void CONTROLLER_DATA::StartDelayedUpdate(byte &org_data, byte new_data) {
  if (org_data != new_data) {
    org_data = new_data;
    this->set_cntlr_flags();
  }
}

void CONTROLLER_DATA::StartDelayedUpdate(String &org_data, String new_data) {
  if (org_data != new_data) {
    org_data = new_data;
    this->set_cntlr_flags();
  }
}


// ----------------------------------------------------------------------
// Focuser Persistent Data: Driver Board
// ----------------------------------------------------------------------
bool CONTROLLER_DATA::LoadBrdConfigStart(String brdfile) {

  CNTLRDATA_println("CD-LoadBrdConfigStart");

  CNTLRDATA_print(T_OPENFILE);
  CNTLRDATA_println(brdfile);
  File bfile = SPIFFS.open(brdfile, "r");
  if (!bfile) {
    CNTLRDATA_print(T_OPENERROR);
    return false;
  } else {
    // read file and deserialize
    String fdata;
    fdata.reserve(BOARDDATASIZE);
    fdata = bfile.readString();
    bfile.close();

    // Allocate a temporary JsonDocument
    DynamicJsonDocument doc_brd(BOARDDATASIZE);

    // Deserialize the JSON document
    DeserializationError jerror = deserializeJson(doc_brd, fdata);
    if (jerror) {
      CNTLRDATA_println(T_DESERIALISEERROR);
      return false;
    } else {
      // save the brd_data just read from board config file (brdfile) into board_config.jsn
      // Set the board values from doc_brd
      this->board = doc_brd["board"].as<const char *>();
      this->maxstepmode = doc_brd["maxstepmode"];
      this->stepmode = doc_brd["stepmode"];
      this->enablepin = doc_brd["enpin"];
      this->steppin = doc_brd["steppin"];
      this->dirpin = doc_brd["dirpin"];
      this->temppin = doc_brd["temppin"];
      this->hpswpin = doc_brd["hpswpin"];
      this->inledpin = doc_brd["inledpin"];
      this->outledpin = doc_brd["outledpin"];
      this->pb1pin = doc_brd["pb1pin"];
      this->pb2pin = doc_brd["pb2pin"];
      this->irpin = doc_brd["irpin"];
      this->boardnumber = doc_brd["brdnum"];
      // brdstepsperrev comes from STEPSPERREVOLUTION and will be different so must override the default setting in the board files
      switch (myboardnumber) {
        case PRO2ESP32ULN2003:
        case PRO2ESP32L298N:
        case PRO2ESP32L293DMINI:
        case PRO2ESP32L9110S:
          this->stepsperrev = mystepsperrev;
          // override STEPSPERREVOLUTION from controller_config.h FIXEDSTEPMODE
          break;
        default:
          this->stepsperrev = doc_brd["stepsrev"];
          break;
      }
      // myfixedstepmode comes from FIXEDSTEPMODE and will be different so must override the default setting in the board files
      switch (myboardnumber) {
        case PRO2ESP32R3WEMOS:
        case PRO2ESP32ST6128:
          this->fixedstepmode = myfixedstepmode;
          // override fixedstepmode from controller_config.h FIXEDSTEPMODE
          break;
        default:
          this->fixedstepmode = doc_brd["fixedsmode"];
          break;
      }
      for (int i = 0; i < 4; i++) {
        this->boardpins[i] = doc_brd["brdpins"][i];
      }
      this->msdelay = doc_brd["msdelay"];
      return true;
    }
  }
  return false;
}

// legacy orphaned code
// kept here in case I change my mind about something
bool CONTROLLER_DATA::CreateBoardConfigfromjson(String jsonstr) {
  // generate board configuration from json string

  CNTLRDATA_print("CD-CreateBoardConfigfromjson() ");
  CNTLRDATA_println(jsonstr);
  // Allocate a temporary Json Document
  DynamicJsonDocument doc_brd(BOARDDATASIZE);

  // Deserialize the JSON document
  DeserializationError jerror = deserializeJson(doc_brd, jsonstr);
  if (jerror) {
    CNTLRDATA_println(T_DESERIALISEERROR);
    LoadDefaultBoardData();
    return false;
  } else {
    /*
      { "board":"PRO2ESP32DRV8825","maxstepmode":32,"stepmode":1,"enpin":14,"steppin":33,
      "dirpin":32,"temppin":13,"hpswpin":4,"inledpin":18,"outledpin":19,"pb1pin":34,"pb2pin":35,"irpin":15,
      "brdnum":60,"stepsrev":-1,"fixedsmode":-1,"brdpins":[27,26,25,-1],"msdelay":4000 }
    */
    this->board = doc_brd["board"].as<const char *>();
    this->maxstepmode = doc_brd["maxstepmode"];
    this->stepmode = doc_brd["stepmode"];
    this->enablepin = doc_brd["enpin"];
    this->steppin = doc_brd["steppin"];
    this->dirpin = doc_brd["dirpin"];
    this->temppin = doc_brd["temppin"];
    this->hpswpin = doc_brd["hpswpin"];
    this->inledpin = doc_brd["inledpin"];
    this->outledpin = doc_brd["outledpin"];
    this->pb1pin = doc_brd["pb1pin"];
    this->pb2pin = doc_brd["pb2pin"];
    this->irpin = doc_brd["irpin"];
    this->boardnumber = doc_brd["brdnum"];
    // brdstepsperrev comes from STEPSPERREVOLUTION and will be different so must override the default setting in the board files
    switch (myboardnumber) {
      case PRO2ESP32ULN2003:
      case PRO2ESP32L298N:
      case PRO2ESP32L293DMINI:
      case PRO2ESP32L9110S:
        this->stepsperrev = mystepsperrev;
        // override STEPSPERREVOLUTION from controller_config.h FIXEDSTEPMODE
        break;
      default:
        this->stepsperrev = doc_brd["stepsrev"];
        break;
    }
    // myfixedstepmode comes from FIXEDSTEPMODE and will be different so must override the default setting in the board files
    switch (myboardnumber) {
      case PRO2ESP32R3WEMOS:
      case PRO2ESP32ST6128:
        this->fixedstepmode = myfixedstepmode;
        // override fixedstepmode from controller_config.h FIXEDSTEPMODE
        break;
      default:
        this->fixedstepmode = doc_brd["fixedsmode"];
        break;
    }
    for (int i = 0; i < 4; i++) {
      this->boardpins[i] = doc_brd["brdpins"][i];
    }
    this->msdelay = doc_brd["msdelay"];
    SaveBoardConfiguration();
    return true;
  }
}

// get
String CONTROLLER_DATA::get_brdname() {
  return this->board;
}

int CONTROLLER_DATA::get_brdmaxstepmode() {
  return this->maxstepmode;
}

int CONTROLLER_DATA::get_brdstepmode() {
  return this->stepmode;
}

int CONTROLLER_DATA::get_brdenablepin() {
  return this->enablepin;
}

int CONTROLLER_DATA::get_brdsteppin() {
  return this->steppin;
}

int CONTROLLER_DATA::get_brddirpin() {
  return this->dirpin;
}

int CONTROLLER_DATA::get_brdtemppin() {
  return this->temppin;
}

int CONTROLLER_DATA::get_brdhpswpin() {
  return this->hpswpin;
}

int CONTROLLER_DATA::get_brdinledpin() {
  return this->inledpin;
}

int CONTROLLER_DATA::get_brdoutledpin() {
  return this->outledpin;
}

int CONTROLLER_DATA::get_brdirpin() {
  return this->irpin;
}

int CONTROLLER_DATA::get_brdboardpins(int pinnum) {
  return this->boardpins[pinnum];
}

int CONTROLLER_DATA::get_brdnumber() {
  return this->boardnumber;
}

int CONTROLLER_DATA::get_brdstepsperrev() {
  return this->stepsperrev;
}

int CONTROLLER_DATA::get_brdfixedstepmode() {
  return this->fixedstepmode;
}

int CONTROLLER_DATA::get_brdpb1pin() {
  return this->pb1pin;
}

int CONTROLLER_DATA::get_brdpb2pin() {
  return this->pb2pin;
}

unsigned long CONTROLLER_DATA::get_brdmsdelay() {
  return this->msdelay;
}

// set
void CONTROLLER_DATA::set_brdname(String newstr) {
  this->StartBoardDelayedUpdate(this->board, newstr);
}

void CONTROLLER_DATA::set_brdmaxstepmode(int newval) {
  this->StartBoardDelayedUpdate(this->maxstepmode, newval);
}

void CONTROLLER_DATA::set_brdstepmode(int newval) {
  this->StartBoardDelayedUpdate(this->stepmode, newval);
}

void CONTROLLER_DATA::set_brdenablepin(int pinnum) {
  this->StartBoardDelayedUpdate(this->enablepin, pinnum);
}

void CONTROLLER_DATA::set_brdsteppin(int pinnum) {
  this->StartBoardDelayedUpdate(this->steppin, pinnum);
}

void CONTROLLER_DATA::set_brddirpin(int pinnum) {
  this->StartBoardDelayedUpdate(this->dirpin, pinnum);
}

void CONTROLLER_DATA::set_brdtemppin(int pinnum) {
  this->StartBoardDelayedUpdate(this->temppin, pinnum);
}

void CONTROLLER_DATA::set_brdhpswpin(int pinnum) {
  this->StartBoardDelayedUpdate(this->hpswpin, pinnum);
}

void CONTROLLER_DATA::set_brdinledpin(int pinnum) {
  this->StartBoardDelayedUpdate(this->inledpin, pinnum);
}

void CONTROLLER_DATA::set_brdoutledpin(int pinnum) {
  this->StartBoardDelayedUpdate(this->outledpin, pinnum);
}

void CONTROLLER_DATA::set_brdirpin(int pinnum) {
  this->StartBoardDelayedUpdate(this->irpin, pinnum);
}

void CONTROLLER_DATA::set_brdboardpins(int pinnum) {
  this->StartBoardDelayedUpdate(this->boardpins[pinnum], pinnum);
}

void CONTROLLER_DATA::set_brdnumber(int newval) {
  this->StartBoardDelayedUpdate(this->boardnumber, newval);
}

void CONTROLLER_DATA::set_brdfixedstepmode(int newval) {
  this->StartBoardDelayedUpdate(this->fixedstepmode, newval);
}

void CONTROLLER_DATA::set_brdstepsperrev(int stepsrev) {
  this->StartBoardDelayedUpdate(this->stepsperrev, stepsrev);
}

void CONTROLLER_DATA::set_brdpb1pin(int newpin) {
  this->StartBoardDelayedUpdate(this->pb1pin, newpin);
}

void CONTROLLER_DATA::set_brdpb2pin(int newpin) {
  this->StartBoardDelayedUpdate(this->pb2pin, newpin);
}

void CONTROLLER_DATA::set_brdmsdelay(unsigned long newval) {
  this->StartBoardDelayedUpdate(this->msdelay, newval);
}

void CONTROLLER_DATA::StartBoardDelayedUpdate(int &org_data, int new_data) {
  if (org_data != new_data) {
    org_data = new_data;
    this->set_board_flags();
  }
}

void CONTROLLER_DATA::StartBoardDelayedUpdate(unsigned long &org_data, unsigned long new_data) {
  if (org_data != new_data) {
    org_data = new_data;
    this->set_board_flags();
  }
}

void CONTROLLER_DATA::StartBoardDelayedUpdate(byte &org_data, byte new_data) {
  if (org_data != new_data) {
    org_data = new_data;
    this->set_board_flags();
  }
}

void CONTROLLER_DATA::StartBoardDelayedUpdate(String &org_data, String new_data) {
  if (org_data != new_data) {
    org_data = new_data;
    this->set_board_flags();
  }
}

void CONTROLLER_DATA::set_cntlr_flags(void) {
  portENTER_CRITICAL(&cntlrMux);
  save_cntlr_flag = 0;
  portEXIT_CRITICAL(&cntlrMux);
}

void CONTROLLER_DATA::set_board_flags(void) {
  portENTER_CRITICAL(&boardMux);
  save_board_flag = 0;
  portEXIT_CRITICAL(&boardMux);
}


// ----------------------------------------------------------------------
// Misc
// ----------------------------------------------------------------------
void CONTROLLER_DATA::ListDir(const char *dirname, uint8_t levels) {
  File root = SPIFFS.open(dirname);
  CNTLRDATA_println("CD-ListDir { ");
  if (!root) {
    CNTLRDATA_println("-err open directory");
  } else {
    if (!root.isDirectory()) {
      CNTLRDATA_println("- not a directory");
    } else {
      File file = root.openNextFile();
      int i = 0;
      while (file) {
        if (file.isDirectory()) {
          CNTLRDATA_print("  DIR : ");
          CNTLRDATA_println(file.name());
          if (levels) {
            this->ListDir(file.name(), levels - 1);
          }
        } else {
          CNTLRDATA_print(file.name());
          CNTLRDATA_print(":");
          CNTLRDATA_print(file.size());
          if ((++i % 6) == 0) {
            CNTLRDATA_println("");
          } else {
            CNTLRDATA_print("  ");
          }
        }
        file = root.openNextFile();
      }
      CNTLRDATA_println("}");
    }
  }
}
