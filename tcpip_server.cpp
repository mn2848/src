// ----------------------------------------------------------------------
// myFP2ESP32 TCP/IP SERVER ROUTINES AND DEFINITIONS
// Copyright Robert Brown 2014-2023. All Rights Reserved.
// Copyright Holger M, 2019-2021. All Rights Reserved.
// tcpipserver.cpp
// Default Configuration
// For ASCOM client via myFP2ESPASOM driver
// For Windows and Linux applications
// For INDI clients via myFP2 INDI driver using TCPIP (no serial support)
// ----------------------------------------------------------------------


// ----------------------------------------------------------------------
// INCLUDES
// ----------------------------------------------------------------------
#include <Arduino.h>
#include <WiFiServer.h>
#include <WiFiClient.h>
#include "SPIFFS.h"
#include <SPI.h>

#include "controller_defines.h"
#include "controller_config.h"


// ----------------------------------------------------------------------
// EXTERNS
// ----------------------------------------------------------------------
// ControllerData
#include "controller_data.h"
extern CONTROLLER_DATA *ControllerData;

// Driver board
#include "driver_board.h"
extern DRIVER_BOARD *driverboard;

// temp probe
#include "temp_probe.h"
extern TEMP_PROBE *tempprobe;

// ASCOM server
#include "ascom_server.h"
extern ASCOM_SERVER *ascomsrvr;

// MANAGEMENT server
#include "management_server.h"
extern MANAGEMENT_SERVER *mngsrvr;

// web server
#include "web_server.h"
extern WEB_SERVER *websrvr;

#include "tcpip_server.h"

extern byte ascomsrvr_status;
extern byte mngsrvr_status;
extern byte websrvr_status;
extern volatile unsigned int park_maxcount;
extern volatile unsigned int display_maxcount;
extern portMUX_TYPE parkMux;
extern portMUX_TYPE displaytimeMux;


// ----------------------------------------------------------------------
// EXTERN HELPER FUNCTIONS
// ----------------------------------------------------------------------
extern bool display_on(void);
extern bool display_off(void);
extern void reboot_esp32(int);
extern long getrssi(void);


// ----------------------------------------------------------------------
// EXTERNS VARS
// ----------------------------------------------------------------------
extern volatile bool halt_alert;
extern portMUX_TYPE halt_alertMux;

extern char ipStr[];
extern char mySSID[];

extern byte display_status;
extern long ftargetPosition;
extern bool isMoving;
extern bool filesystemloaded;
extern float temp;


// ----------------------------------------------------------------------
// CLASS: TCPIP Server
// ----------------------------------------------------------------------
TCPIP_SERVER::TCPIP_SERVER() {
  // clear lists for client connections
  for (int lp = 0; lp < MAXCONNECTIONS; lp++) {
    _myclientsfreeslot[lp] = false;
  }
  _totalclients = 0;
}


// ----------------------------------------------------------------------
// bool start(void);
// Create and start the TCP/IP Server
// ----------------------------------------------------------------------
bool TCPIP_SERVER::start(unsigned long port) {
  debug_server_print(T_TCPIPSERVER);
  debug_server_println(T_START);

  // if the server is not enabled then return
  if (ControllerData->get_tcpipsrvr_enable() == V_NOTENABLED) {
    debug_server_println(T_NOTENABLED);
    return false;
  }

  // prevent any attempt to start if already started
  if (_loaded == true) {
    return true;
  }

  _port = port;

  // check if port has changed
  if (_port != ControllerData->get_tcpipsrvr_port()) {
    ControllerData->set_tcpipsrvr_port(_port);
  }

  // cache the presets
  cachepresets();

  // check if server already created, if not, create one
  if (_loaded == false) {
    _myserver = new WiFiServer(_port);
  }

  _myserver->begin(_port);
  _loaded = true;
  _state = V_RUNNING;
  debug_server_println(T_RUNNING);
  return _loaded;
}

// ----------------------------------------------------------------------
// void stop(void);
// Stop the TCPIP SERVER
// This will stop and delete _myserver
// This must be done because start() creates _myserver
// ----------------------------------------------------------------------
void TCPIP_SERVER::stop(void) {
  debug_server_print(T_TCPIPSERVER);
  debug_server_println(T_STOP);
  // can only stop a server that is _loaded
  if (_loaded == true) {
    _myserver->stop();

    delete _myserver;
    _loaded = false;
    _state = V_STOPPED;
  }
}

// ----------------------------------------------------------------------
// void loop(void);
// Checks for any new clients or existing client requests
// ----------------------------------------------------------------------
void TCPIP_SERVER::loop(bool parkstate) {
  // avoid a crash
  if (_loaded == false) {
    return;
  }

  _parked = parkstate;
  // check if any new connections
  WiFiClient newclient = _myserver->available();
  if (newclient)  // if there is a new tcp/ip client
  {
    debug_server_print(T_TCPIPSERVER);
    debug_server_print("new client ");
    debug_server_println(T_FOUND);
    // search for free slot, return first free slot found
    int lp = 0;
    for (lp = 0; lp < MAXCONNECTIONS; lp++) {
      if (_myclientsfreeslot[lp] == false) {
        break;
      }
    }

    // check if there is a free slot
    if (lp < MAXCONNECTIONS) {
      debug_server_print(T_TCPIPSERVER);
      debug_server_print("client ");
      debug_server_println(T_CONNECTED);
      // save new client to client list
      _myclients[lp] = new WiFiClient(newclient);
      // get IP of client
      _myclientsIPAddressList[lp] = newclient.remoteIP();
      // indicate slot is in use
      _myclientsfreeslot[lp] = true;
      _totalclients++;
      // newClient will dispose at end of loop()
      newclient.stop();

      // TODO turn oled_state true to start display for this client ?

    } else {
      debug_server_print(T_TCPIPSERVER);
      debug_server_print(T_ERROR);
      debug_server_println("No connection");
      newclient.stop();
    }
  }

  // cycle through each tcp/ip client connection
  // faster to avoid for loop if there are no clients
  if (_totalclients > 0) {
    // check all connected wifi client slots for data
    for (int lp = 0; lp < MAXCONNECTIONS; lp++) {
      // if there is a client
      if (_myclientsfreeslot[lp] == true) {
        // if client is connected
        if (_myclients[lp]->connected()) {
          // if client has send request
          while (_myclients[lp]->available()) {
            // process request and send client number
            process_command(lp);
          }
        } else {
          // not connected, stop client
          _myclients[lp]->stop();
          // free client space
          _myclientsfreeslot[lp] = false;
          // release client
          delete _myclients[lp];
          _myclients[lp] = NULL;
          _totalclients--;
          if (_totalclients < 0) {
            _totalclients = 0;
          }
        }  // if (myclients[lp].connected())
      }    // if ( myclientsfreeslot[lp] == true )
    }      // for ( int lp = 0; lp < MAXCONNECTIONS; lp++ )
  }        // if ( totalclients > 0 )
}

// ----------------------------------------------------------------------
// Determine if there is at least 1 client connected
// ----------------------------------------------------------------------
bool TCPIP_SERVER::get_clients(void) {
  if (_totalclients == 0) {
    return false;
  } else {
    return true;
  }
}

// ----------------------------------------------------------------------
// Cache focuser presets for faster access
// ----------------------------------------------------------------------
void TCPIP_SERVER::cachepresets(void) {
  for (int lp = 0; lp < 10; lp++) {
    _presets[lp] = ControllerData->get_focuserpreset(lp);
  }
}

// ----------------------------------------------------------------------
// Convert float to ascii string
// ----------------------------------------------------------------------
char *TCPIP_SERVER::ftoa(char *a, double f, int precision) {
  const long p[] = { 0, 10, 100, 1000, 10000, 100000, 1000000, 10000000, 100000000 };

  char *ret = a;
  long heiltal = (long)f;
  itoa(heiltal, a, 10);
  while (*a != '\0') a++;
  *a++ = '.';
  long desimal = abs((long)((f - heiltal) * p[precision]));
  itoa(desimal, a, 10);
  return ret;
}

// ----------------------------------------------------------------------
// Send reply to client
// ----------------------------------------------------------------------
void TCPIP_SERVER::send_reply(const char *str, int clientnum) {
  // if client is still connected
  if (_myclients[clientnum]->connected()) {
    // send reply
    _myclients[clientnum]->print(str);
  }
}

// ----------------------------------------------------------------------
// Build a char array reply to a client
// ----------------------------------------------------------------------
void TCPIP_SERVER::build_reply(const char token, const char *str, int clientnum) {
  char buff[32];
  snprintf(buff, sizeof(buff), "%c%s%c", token, str, _EOFSTR);
  send_reply(buff, clientnum);
}

// ----------------------------------------------------------------------
// Build a unsigned char reply to a client
// ----------------------------------------------------------------------
void TCPIP_SERVER::build_reply(const char token, unsigned char data_val, int clientnum) {
  char buff[32];
  snprintf(buff, sizeof(buff), "%c%u%c", token, data_val, _EOFSTR);
  send_reply(buff, clientnum);
}

// ----------------------------------------------------------------------
// Build a float reply to a client
// i = decimal places
// ----------------------------------------------------------------------
void TCPIP_SERVER::build_reply(const char token, float data_val, int i, int clientnum) {
  char buff[32];
  char tmp[10];
  // Note Arduino snprintf does not support .2f
  ftoa(tmp, data_val, i);
  snprintf(buff, sizeof(buff), "%c%s%c", token, tmp, _EOFSTR);
  send_reply(buff, clientnum);
}

// ----------------------------------------------------------------------
// Build a integer reply to a client
// ----------------------------------------------------------------------
void TCPIP_SERVER::build_reply(const char token, int data_val, int clientnum) {
  char buff[32];
  snprintf(buff, sizeof(buff), "%c%i%c", token, data_val, _EOFSTR);
  send_reply(buff, clientnum);
}

// ----------------------------------------------------------------------
// Build a string reply to a client
// ----------------------------------------------------------------------
void TCPIP_SERVER::build_reply(const char token, String str, int clientnum) {
  char buff[32];
  char tmp[30];
  str.toCharArray(tmp, str.length());
  snprintf(buff, str.length() + 1, "%c%s%c", token, tmp, _EOFSTR);
  send_reply(buff, clientnum);
}

// ----------------------------------------------------------------------
// Build a long reply to a client
// ----------------------------------------------------------------------
void TCPIP_SERVER::build_reply(const char token, long data_val, int clientnum) {
  char buff[32];
  snprintf(buff, sizeof(buff), "%c%ld%c", token, data_val, _EOFSTR);
  send_reply(buff, clientnum);
}

// ----------------------------------------------------------------------
// Build a unsigned long reply to a client
// ----------------------------------------------------------------------
void TCPIP_SERVER::build_reply(const char token, unsigned long data_val, int clientnum) {
  char buff[32];
  snprintf(buff, sizeof(buff), "%c%lu%c", token, data_val, _EOFSTR);
  send_reply(buff, clientnum);
}

// ----------------------------------------------------------------------
// Process a client command request
// ----------------------------------------------------------------------
void TCPIP_SERVER::process_command(int clientnum) {
  // compatibility
  static byte joggingstate = 0;
  static byte joggingdirection = 0;
  static byte delayeddisplayupdatestatus = 0;

  String receiveString = "";
  String WorkString = "";
  int cmdvalue;
  int paramval;
  long tmppos;

  String drvbrd = ControllerData->get_brdname();
  receiveString = _myclients[clientnum]->readStringUntil(_EOFSTR);
  receiveString = receiveString + '#' + "";

  String cmdstr = receiveString.substring(1, 3);

  if (cmdstr[0] == 'A') {
    cmdvalue = 100 + (cmdstr[1] - '0');  // can only use digits A0-A9
  } else if (cmdstr[0] == 'B') {
    cmdvalue = 110 + (cmdstr[1] - '0');  // can only use digits B0-B9
  } else if (cmdstr[0] == 'C') {
    cmdvalue = 120 + (cmdstr[1] - '0');  // can only use digits C0-C9
  } else {
    cmdvalue = cmdstr.toInt();
  }

  debug_server_print(T_TCPIPSERVER);
  debug_server_print(", recstr ");
  debug_server_print(receiveString);
  debug_server_print(", cmdstr ");
  debug_server_println(cmdstr);
  switch (cmdvalue) {
    case 0:  // get focuser position
      build_reply('P', driverboard->getposition(), clientnum);
      break;
    case 1:  // ismoving
      build_reply('I', isMoving, clientnum);
      break;
    case 2:  // get controller status
      build_reply('E', "OK", clientnum);
      break;
    case 3:  // get firmware version
      build_reply('F', program_version, clientnum);
      break;
    case 4:  // get get_brdname + version number
      {
        char buff[32];
        char tempstr[20];
        String brdname = ControllerData->get_brdname();
        brdname.toCharArray(tempstr, brdname.length() + 1);
        snprintf(buff, sizeof(buff), "%s%c%c%s", tempstr, '\r', '\n', program_version);
        build_reply('F', buff, clientnum);
      }
      break;
    case 5:  // Set new target position to xxxxxx (and focuser initiates immediate move to xxxxxx)
      // only if not already moving
      if (isMoving == 0) {
        WorkString = receiveString.substring(3, receiveString.length() - 1);
        ftargetPosition = WorkString.toInt();
        ftargetPosition = (ftargetPosition < 0) ? 0 : ftargetPosition;
        ftargetPosition = (ftargetPosition > ControllerData->get_maxstep()) ? ControllerData->get_maxstep() : ftargetPosition;
        isMoving = 1;
      }
      break;
    case 6:  // get temperature
      build_reply('Z', temp, 3, clientnum);
      break;
    case 7:  // Set maxsteps
      WorkString = receiveString.substring(3, receiveString.length() - 1);
      tmppos = WorkString.toInt();
      // check to make sure not above largest value for maxstep
      tmppos = (tmppos > FOCUSERUPPERLIMIT) ? FOCUSERUPPERLIMIT : tmppos;
      // check if below lowest set valueue for maxstep
      tmppos = (tmppos < FOCUSERLOWERLIMIT) ? FOCUSERLOWERLIMIT : tmppos;
      // check to make sure its not less than current focuser position
      tmppos = (tmppos < driverboard->getposition()) ? driverboard->getposition() : tmppos;
      ControllerData->set_maxstep(tmppos);
      break;
    case 8:  // get maxStep
      build_reply('M', ControllerData->get_maxstep(), clientnum);
      break;
    case 9:  // myFP2ESP32 get _inoutledmode, pulse or move
      build_reply('$', ControllerData->get_inoutled_mode(), clientnum);
      break;
    case 10:  // get maxIncrement
      build_reply('Y', ControllerData->get_maxstep(), clientnum);
      break;
    case 11:  // get coil power enable
      build_reply('O', ControllerData->get_coilpower_enable(), clientnum);
      break;
    case 12:  // set coil power enable
      paramval = receiveString[3] - '0';
      // if 1, enable coilpower, set coilpowerstate true, enable motor
      // if 0, disable coilpower, set coilpowerstate false; release motor
      (paramval == 1) ? driverboard->enablemotor() : driverboard->releasemotor();
      (paramval == 1) ? ControllerData->set_coilpower_enable(V_ENABLED) : ControllerData->set_coilpower_enable(V_NOTENABLED);
      break;
    case 13:  // get reverse direction setting, 00 off, 01 on
      build_reply('R', ControllerData->get_reverse_enable(), clientnum);
      break;
    case 14:  // set reverse direction
      if (isMoving == 0) {
        paramval = receiveString[3] - '0';
        (paramval == 1) ? ControllerData->set_reverse_enable(V_ENABLED) : ControllerData->set_reverse_enable(V_NOTENABLED);
      }
      break;
    case 15:  // set motor speed
      WorkString = receiveString.substring(3, receiveString.length() - 1);
      paramval = WorkString.toInt() & 3;
      ControllerData->set_motorspeed((byte)paramval);
      break;
    case 16:  // set temperature display setting to celsius (0)
      ControllerData->set_tempmode(V_CELSIUS);
      break;
    case 17:  // set temperature display setting to fahrenheit (1)
      ControllerData->set_tempmode(V_FAHRENHEIT);
      break;
    case 18:  // set Stepsize enable state
      // :180#    None    Set stepsize to be OFF - default
      // :181#    None    stepsize to be ON - reports what user specified as stepsize
      //WorkString = receiveString.substring(3, receiveString.length() - 1);
      paramval = receiveString[3] - '0';
      ControllerData->set_stepsize_enable((byte)paramval);
      break;
    case 19:  // set the step size value - double type, eg 2.1
      {
        WorkString = receiveString.substring(3, receiveString.length() - 1);
        float tempstepsize = WorkString.toFloat();
        tempstepsize = (tempstepsize < MINIMUMSTEPSIZE) ? MINIMUMSTEPSIZE : tempstepsize;
        tempstepsize = (tempstepsize > MAXIMUMSTEPSIZE) ? MAXIMUMSTEPSIZE : tempstepsize;
        ControllerData->set_stepsize(tempstepsize);
      }
      break;
    case 20:  // set the temperature resolution setting for the DS18B20 temperature probe
      WorkString = receiveString.substring(3, receiveString.length() - 1);
      paramval = WorkString.toInt();
      paramval = (paramval < 9) ? 9 : paramval;
      paramval = (paramval > 12) ? 12 : paramval;
      ControllerData->set_tempresolution((byte)paramval);
      tempprobe->set_resolution((byte)paramval);
      break;
    case 21:  // get temp probe resolution
      build_reply('Q', ControllerData->get_tempresolution(), clientnum);
      break;
    case 22:  // set temperature coefficient steps value to xxx
      WorkString = receiveString.substring(3, receiveString.length() - 1);
      paramval = WorkString.toInt();
      ControllerData->set_tempcoefficient(paramval);
      break;
    case 23:  // set the temperature compensation ON (1) or OFF (0)
      if (tempprobe->get_state() == V_RUNNING) {
        paramval = receiveString[3] - '0';
        ControllerData->set_tempcomp_enable((byte)paramval);
      }
      break;
    case 24:  // get status of temperature compensation (enabled | disabled)
      build_reply('1', ControllerData->get_tempcomp_enable(), clientnum);
      break;
    case 25:  // get temperature compensation available
      build_reply('A', ControllerData->get_tcavailable(), clientnum);
      break;
    case 26:  // get temperature coefficient steps/degree
      build_reply('B', ControllerData->get_tempcoefficient(), clientnum);
      break;
    case 27:  // stop a move - like a Halt
      portENTER_CRITICAL(&halt_alertMux);
      halt_alert = true;
      portEXIT_CRITICAL(&halt_alertMux);
      break;
    case 28:  // home the motor to position 0
      if (isMoving == 0) {
        ftargetPosition = 0;
        isMoving = 1;
      }
      break;
    case 29:  // get stepmode
      build_reply('S', ControllerData->get_brdstepmode(), clientnum);
      break;

    // ----------------------------------------------------------------------
    // Basic rule for setting stepmode
    // set DRIVER_BOARD->setstepmode(xx);                         // this sets the physical pins
    // and this also saves ControllerData->set_brdstepmode(xx);   // this saves config setting
    case 30:  // set step mode
      {
        WorkString = receiveString.substring(3, receiveString.length() - 1);
        paramval = WorkString.toInt();
        int brdnum = ControllerData->get_brdnumber();
        if (brdnum == PRO2ESP32ULN2003 || brdnum == PRO2ESP32L298N || brdnum == PRO2ESP32L293DMINI || brdnum == PRO2ESP32L9110S) {
          paramval = (int)(paramval & 3);  // STEP1 - STEP2
        } else if (brdnum == PRO2ESP32DRV8825 || brdnum == PRO2ESP32R3WEMOS) {
          paramval = (paramval < STEP1) ? STEP1 : paramval;
          paramval = (paramval > STEP32) ? STEP32 : paramval;
        } else if (brdnum == PRO2ESP32TMC2225 || brdnum == PRO2ESP32TMC2209 || brdnum == PRO2ESP32TMC2209P) {
          paramval = (paramval < STEP1) ? STEP1 : paramval;
          paramval = (paramval > STEP256) ? STEP256 : paramval;
        } else {
          debug_server_print(T_TCPIPSERVER);
          debug_server_print(T_ERROR);
          debug_server_print(", invalid brd ");
          debug_server_println(brdnum);
        }
      }
      ControllerData->set_brdstepmode((int)paramval);
      driverboard->setstepmode((int)paramval);
      break;
    case 31:  // set focuser position
      if (isMoving == 0) {
        WorkString = receiveString.substring(3, receiveString.length() - 1);
        {
          long tpos = (long)WorkString.toInt();
          tpos = (tpos < 0) ? 0 : tpos;
          tpos = (tpos > ControllerData->get_maxstep()) ? ControllerData->get_maxstep() : tpos;
          ftargetPosition = tpos;
          driverboard->setposition(tpos);
          ControllerData->set_fposition(tpos);
        }
      }
      break;
    case 32:  // get if stepsize is enabled
      build_reply('U', ControllerData->get_stepsize_enable(), clientnum);
      break;
    case 33:  // get stepsize
      build_reply('T', ControllerData->get_stepsize(), 2, clientnum);
      break;
    case 34:  // get the time that a display page is shown for
      build_reply('X', ControllerData->get_display_pagetime(), clientnum);
      break;
    case 35:  // set the time a display page is displayed for in seconds, integer, 2-10
      WorkString = receiveString.substring(3, receiveString.length() - 1);
      paramval = WorkString.toInt();
      paramval = (paramval < V_DISPLAYPAGETIMEMIN) ? V_DISPLAYPAGETIMEMIN : paramval;
      paramval = (paramval > V_DISPLAYPAGETIMEMAX) ? V_DISPLAYPAGETIMEMAX : paramval;
      ControllerData->set_display_pagetime(paramval);
      // update display_maxcount
      portENTER_CRITICAL(&displaytimeMux);
      // convert to timeslices
      display_maxcount = paramval * 10;
      portEXIT_CRITICAL(&displaytimeMux);
      break;
    case 36:  // set display writing state, 0 = write not allowed, 1 = write text allowed
      // :360#    None    Blank the Display
      // :361#    None    UnBlank the Display
      //WorkString = receiveString.substring(3, receiveString.length() - 1);
      //paramval = (byte) WorkString.toInt() & 0x01;
      paramval = receiveString[3] - '0';
      (paramval == 1) ? display_on() : display_off();
      break;
    case 37:  // get display status (1=Running or 0=Stopped)
      build_reply('D', display_status, clientnum);
      break;
    case 38:  // get temperature mode 1=Celsius, 0=Fahrenheight
      build_reply('b', ControllerData->get_tempmode(), clientnum);
      break;
    case 39:  // get the new motor position (target) XXXXXX
      build_reply('N', ftargetPosition, clientnum);
      break;
    case 40:  // reboot controller with 2s delay
      reboot_esp32(2000);
      break;
    case 41:  // myFP2ESP32 set in-out-led-mode (pulsed or move)
      WorkString = receiveString.substring(3, receiveString.length() - 1);
      paramval = receiveString[3] - '0';
      ControllerData->set_inoutled_mode((byte)paramval);
      break;
    case 42:  // reset focuser defaults
      if (isMoving == 0) {
        ControllerData->SetFocuserDefaults();
        ftargetPosition = ControllerData->get_fposition();
        driverboard->setposition(ftargetPosition);
        ControllerData->set_fposition(ftargetPosition);
      }
      break;
    case 43:  // get motorspeed
      build_reply('C', ControllerData->get_motorspeed(), clientnum);
      break;
    case 44:  // myFP2ESP32 get park enable state
      build_reply('$', ControllerData->get_park_enable(), clientnum);
      break;
    case 45:  // myFP2ESP32 set park enable state
      paramval = receiveString[3] - '0';
      ControllerData->set_park_enable((byte)paramval);
      break;
    case 46:  // myFP2ESP32 get in-out led enable state
      build_reply('$', ControllerData->get_inoutled_enable(), clientnum);
      break;
    case 47:  // myFP2ESP32 set in-out led enable state
      paramval = receiveString[3] - '0';
      ControllerData->set_inoutled_enable((byte)paramval);
      break;
    case 48:  // save settings to file
      // do not do this if focuser is moving
      if (isMoving == 0) {
        // need to save position setting
        ControllerData->set_fposition(driverboard->getposition());
        // save the focuser settings immediately
        ControllerData->SaveNow(driverboard->getposition(), driverboard->getdirection());
      }
      break;
    case 49:  // aXXXXX
      build_reply('a', "b552efd", clientnum);
      break;
    case 50:  // get if Home Position Switch enabled, 0 = no, 1 = yes
      build_reply('l', ControllerData->get_hpswitch_enable(), clientnum);
      break;
    case 51:  // myFP2ESP32 get Wifi Controller IP Address
      build_reply('$', ipStr, clientnum);
      break;
    case 52:  // myFP2ESP32 get park state
      if (_parked) {
        build_reply('$', 1, clientnum);
      } else {
        build_reply('$', 0, clientnum);
      }
      break;
    case 54:  // myFP2ESP32 ESP32 Controller SSID
      build_reply('$', mySSID, clientnum);
      break;
    case 55:  // get motorspeed delay for current speed setting
      build_reply('0', ControllerData->get_brdmsdelay(), clientnum);
      break;
    case 56:  // set motorspeed delay for current speed setting
      WorkString = receiveString.substring(3, receiveString.length() - 1);
      paramval = WorkString.toInt();
      paramval = (paramval < 1000) ? 1000 : paramval;
      ControllerData->set_brdmsdelay(paramval);
      break;
    case 57:  // myFP2ESP32 get pushbutton enable state
      build_reply('$', driverboard->get_pushbuttons_loaded(), clientnum);
      break;
    case 58:  // myFP2ESP32 set pushbutton enable state
      paramval = receiveString[3] - '0';
      driverboard->set_pushbuttons((byte)paramval);
      break;
    case 59:  // myFP2ESP32 get park time
      build_reply('$', ControllerData->get_parktime(), clientnum);
      break;
    case 60:  // myFP2ESP32 set park time interval in seconds
      {
        // range check 30s to 300s (5m)
        WorkString = receiveString.substring(3, receiveString.length() - 1);
        paramval = WorkString.toInt();
        paramval = (paramval < 30) ? 30 : paramval;
        paramval = (paramval > 300) ? 300 : paramval;
        ControllerData->set_parktime(paramval);
        // update park_maxcount
        portENTER_CRITICAL(&parkMux);
        // convert to timeslices
        park_maxcount = paramval * 10;
        portEXIT_CRITICAL(&parkMux);
      }
      break;
    case 61:  // set update of position on oled when moving (0=disable, 1=enable)
      paramval = receiveString[3] - '0';
      ControllerData->set_display_updateonmove((byte)paramval);
      break;
    case 62:  // get update of position on oled when moving (00=disable, 01=enable)
      build_reply('L', ControllerData->get_display_updateonmove(), clientnum);
      break;
    case 63:  // get status of home position switch
      // if the hpsw is enabled
      if (ControllerData->get_hpswitch_enable() == V_RUNNING) {
        // get state of hpsw, return 1 if closed, 0 if open
        // myFP2ESP32  (hpsw pin 1=open, 0=closed)
        // if( driverboard->hpsw_alert() == true )
        build_reply('H', driverboard->hpsw_alert(), clientnum);
      } else {
        build_reply('H', 0, clientnum);
      }
      break;
    case 64:  // move a specified number of steps
      if (isMoving == 0) {
        WorkString = receiveString.substring(3, receiveString.length() - 1);
        long pos = WorkString.toInt() + driverboard->getposition();
        pos = (pos < 0) ? 0 : pos;
        ftargetPosition = (pos > ControllerData->get_maxstep()) ? ControllerData->get_maxstep() : pos;
        isMoving = 0;
      }
      break;
    case 65:  // set jogging state enable/disable
      paramval = receiveString[3] - '0';
      joggingstate = (byte)paramval;
      break;
    case 66:  // get jogging state enabled/disabled
      build_reply('K', joggingstate, clientnum);
      break;
    case 67:  // set jogging direction, 0=IN, 1=OUT
      paramval = receiveString[3] - '0';
      joggingdirection = (byte)paramval;
      break;
    case 68:  // get jogging direction, 0=IN, 1=OUT
      build_reply('V', joggingdirection, clientnum);
      break;
    case 69:  // get push button steps
      build_reply('?', ControllerData->get_pushbutton_steps(), clientnum);
      break;
    case 70:  // set push buttons steps [1-max] where max = stepsize / 2
      {
        WorkString = receiveString.substring(3, receiveString.length() - 1);
        paramval = WorkString.toInt();
        paramval = (paramval < 1) ? 1 : paramval;
        // set maximum steps to be 1/2 the step size
        int sz = (int)ControllerData->get_stepsize() / 2;
        sz = (sz < 1) ? 1 : sz;
        paramval = (paramval > sz) ? sz : paramval;
        ControllerData->set_pushbutton_steps((byte)paramval);
      }
      break;
    case 71:  // set delayaftermove time value in milliseconds [0-250]
      WorkString = receiveString.substring(3, receiveString.length() - 1);
      paramval = WorkString.toInt();
      paramval = (paramval < 0) ? 0 : paramval;
      paramval = (paramval > 250) ? 250 : paramval;
      ControllerData->set_delayaftermove_time((byte)paramval);
      break;
    case 72:  // get delayaftermove_state value in milliseconds
      build_reply('3', ControllerData->get_delayaftermove_time(), clientnum);
      break;
    case 73:  // set disable/enable backlash IN (going to lower focuser position)
      paramval = receiveString[3] - '0';
      ControllerData->set_backlash_in_enable((byte)paramval);
      break;
    case 74:  // get backlash in enabled status
      build_reply('4', ControllerData->get_backlash_in_enable(), clientnum);
      break;
    case 75:  // set disable/enable backlash OUT (going to lower focuser position)
      paramval = receiveString[3] - '0';
      ControllerData->set_backlash_out_enable((byte)paramval);
      break;
    case 76:  // get backlash OUT enabled status
      build_reply('5', ControllerData->get_backlash_out_enable(), clientnum);
      break;
    case 77:  // set backlash in steps [0-255]
      WorkString = receiveString.substring(3, receiveString.length() - 1);
      paramval = (byte)WorkString.toInt() & 0xff;
      ControllerData->set_backlashsteps_in((byte)paramval);
      break;
    case 78:  // get backlash steps IN
      build_reply('6', ControllerData->get_backlashsteps_in(), clientnum);
      break;
    case 79:  // set backlash OUT steps
      WorkString = receiveString.substring(3, receiveString.length() - 1);
      paramval = (byte)WorkString.toInt() & 0xff;
      ControllerData->set_backlashsteps_out((byte)paramval);
      break;
    case 80:  // get backlash steps OUT
      build_reply('7', ControllerData->get_backlashsteps_out(), clientnum);
      break;
    case 81:  // get STALL_VALUE (for TMC2209 stepper modules)
      build_reply('8', ControllerData->get_stallguard_value(), clientnum);
      break;
    case 82:  // myFP2ESP32 set STALL_VALUE (for TMC2209 stepper modules)
      WorkString = receiveString.substring(2, receiveString.length());
      driverboard->setstallguardvalue((byte)WorkString.toInt());
      break;
    case 83:  // get if there is a temperature probe
      if (tempprobe->get_found() == false) {
        build_reply('c', 0, clientnum);
      } else {
        build_reply('c', 1, clientnum);
      }
      break;

    case 84:  // myFP2N reserved
      break;

    case 85:  // myFP2ESP32 get delay after move enable state
      build_reply('$', ControllerData->get_delayaftermove_enable(), clientnum);
      break;
    case 86:  // myFP2ESP32 set delay after move enable state
      paramval = receiveString[3] - '0';
      ControllerData->set_delayaftermove_enable((byte)paramval);
      break;
    case 87:  // get tc direction
      build_reply('k', ControllerData->get_tcdirection(), clientnum);
      break;
    case 88:  // set tc direction
      paramval = receiveString[3] - '0';
      ControllerData->set_tcdirection((byte)paramval);
      break;
    case 89:  // get stepper power (reads from A7) - only valid if hardware circuit is added (1=stepperpower ON)
      build_reply('9', 1, clientnum);
      break;
    case 90:  // myFP2ESP32 set preset x [0-9] with position value yyyy [unsigned long]
      {
        byte preset = (byte)(receiveString[3] - '0');
        preset = (preset > 9) ? 9 : preset;
        WorkString = receiveString.substring(4, receiveString.length() - 1);
        tmppos = WorkString.toInt();
        tmppos = (tmppos < 0) ? 0 : tmppos;
        tmppos = (tmppos > ControllerData->get_maxstep()) ? ControllerData->get_maxstep() : tmppos;
        ControllerData->set_focuserpreset(preset, tmppos);
        // update cached copy
        _presets[preset] = tmppos;
      }
      break;
    case 91:  // myFP2ESP32 get focuserpreset [0-9]
      {
        WorkString = receiveString.substring(3, receiveString.length() - 1);
        byte preset = (byte)WorkString.toInt();
        build_reply('$', _presets[preset], clientnum);
      }
      break;
    case 92:  // set display page display option (8 digits, index of 0-7)
      {
        WorkString = receiveString.substring(3, receiveString.length() - 1);
        // If empty (no args) - fill with default display string
        if (WorkString == "") {
          WorkString = "11111111";
        }

        // if display option length less than 8, pad with leading 0's
        if (WorkString.length() < 8) {
          while (WorkString.length() < 8) {
            WorkString = '0' + WorkString;
          }
        }

        // do not allow display strings that exceed length of buffer (0-7, 8 digits)
        if (WorkString.length() > 8) {
          WorkString[8] = 0x00;
        }
        ControllerData->set_display_pageoption(WorkString);
      }
      break;
    case 93:  // get display page option
      {
        // return as string of 01's
        char buff[10];
        memset(buff, 0, 10);
        String answer = ControllerData->get_display_pageoption();
        // should always be 8 digits (0-7) due to set command (:92)
        // copy to buff
        int i;
        for (i = 0; i < answer.length(); i++) {
          buff[i] = answer[i];
        }
        buff[i] = 0x00;
        build_reply('l', buff, clientnum);
      }
      break;
    case 94:  // - set DelayedDisplayUpdate (0=disabled, 1-enabled)
      paramval = receiveString[3] - '0';
      delayeddisplayupdatestatus = (byte)paramval;
      break;
    case 95:  // - get DelayedDisplayUpdate (0=disabled, 1-enabled)
      build_reply('n', delayeddisplayupdatestatus, clientnum);
      break;

    case 96:  // not used
      break;

    case 97:  // not used
      break;

    case 98:  // myFP2ESP32 get network strength dbm
      {
        long rssi = getrssi();
        build_reply('$', rssi, clientnum);
      }
      break;
    case 99:  // myFP2ESP32 set home positon switch enable state, 0 or 1, disabled or enabled
      {
        paramval = receiveString[3] - '0';
        debug_server_print(T_TCPIPSERVER);
        debug_server_print(T_HPSW);
        if (ControllerData->get_brdhpswpin() == -1) {
          debug_server_println(T_NOTSUPPORTED);
        } else {
          if (paramval == 1) {
            // enable
            if (driverboard->init_hpsw() == true) {
              debug_server_println(T_ENABLED);
              ControllerData->set_hpswitch_enable((byte)paramval);
            } else {
              debug_server_println(T_ERROR);
            }
          } else {
            // disable
            ControllerData->set_hpswitch_enable((byte)paramval);
            debug_server_println(T_DISABLED);
          }
        }
      }
      break;

    // :A0-A9
    case 100:  // myFP2ESP32 get joystick1 enable state
      build_reply('$', driverboard->get_joystick1_loaded(), clientnum);
      break;
    case 101:  // myFP2ESP32 set joystick1 enable state (0=stopped, 1=started)
      paramval = receiveString[3] - '0';
      driverboard->set_joystick1((byte)paramval);
      break;
    case 102:  // myFP2ESP32 get joystick2 enable state
      build_reply('$', driverboard->get_joystick2_loaded(), clientnum);
      break;
    case 103:  // myFP2ESP32 set joystick2 enable state (0=stopped, 1=started)
      paramval = receiveString[3] - '0';
      driverboard->set_joystick2((byte)paramval);
      break;
    case 104:  // myFP2ESP32 get temp probe enabled state
      build_reply('$', ControllerData->get_tempprobe_enable(), clientnum);
      break;
    case 105:  // myFP2ESP32 set temp probe enabled state
      paramval = receiveString[3] - '0';
      ControllerData->set_tempprobe_enable((byte)paramval);
      break;
    case 106:  // myFP2ESP32 get ASCOM ALPACA Server enabled state
      build_reply('$', ControllerData->get_ascomsrvr_enable(), clientnum);
      break;
    case 107:  // myFP2ESP32 set ASCOM ALPACA Server enabled state
      paramval = receiveString[3] - '0';
      if (paramval == 1) {
        // enable the server
        ControllerData->set_ascomsrvr_enable(V_ENABLED);
      } else {
        // stop and disable
        ascomsrvr->stop();
        ascomsrvr_status = V_STOPPED;
        ControllerData->set_ascomsrvr_enable(V_NOTENABLED);
      }
      break;
    case 108:  // myFP2ESP32 get ASCOM ALPACA Server Start/Stop status
      build_reply('$', ascomsrvr_status, clientnum);
      break;
    case 109:  // myFP2ESP32 set ASCOM ALPACA Server Start/Stop - this will start or stop the ASCOM server
      paramval = receiveString[3] - '0';
      if (paramval == 1) {
        // start if enabled
        if (ControllerData->get_ascomsrvr_enable() == V_ENABLED) {
          ascomsrvr_status = ascomsrvr->start();
          if (ascomsrvr_status != V_RUNNING) {
            debug_server_print(T_ALPACA);
            debug_server_println(T_ERRSTART);
          }
        } else {
          debug_server_print(T_ALPACA);
          debug_server_println(T_NOTENABLED);
        }
      } else {
        // stop
        ascomsrvr->stop();
        ascomsrvr_status = V_STOPPED;
      }
      break;

    // :B0 to :B9
    case 110:  // myFP2ESP32 get Web Server enabled state
      build_reply('$', ControllerData->get_ascomsrvr_enable(), clientnum);
      break;
    case 111:  // myFP2ESP32 set Web Server enabled state
      paramval = receiveString[3] - '0';
      if (paramval == 1) {
        // enable the server
        ControllerData->set_websrvr_enable(V_ENABLED);
      } else {
        // stop and disable
        websrvr->stop();
        websrvr_status = V_STOPPED;
        ControllerData->set_websrvr_enable(V_NOTENABLED);
      }
      break;
    case 112:  // myFP2ESP32 get Web Server Start/Stop status
      build_reply('$', ascomsrvr_status, clientnum);
      break;
    case 113:  // myFP2ESP32 set Web Server Start/Stop - this will start or stop the ASCOM server
      paramval = receiveString[3] - '0';
      if (paramval == 1) {
        // start if enabled
        if (ControllerData->get_websrvr_enable() == V_ENABLED) {
          // enabled
          websrvr_status = websrvr->start(ControllerData->get_websrvr_port());
          if (websrvr_status != V_RUNNING) {
            debug_server_print(T_ERRSTART);
            debug_server_println(T_WEBSERVER);
          }
        }
      } else {
        // stop
        if (websrvr_status == V_RUNNING) {
          websrvr->stop();
          websrvr_status = V_STOPPED;
        }
      }
      break;

    case 114:  // myFP2ESP32 get Management Server enabled state
      build_reply('$', ControllerData->get_mngsrvr_enable(), clientnum);
      break;
    case 115:  // myFP2ESP32 set Management Server enabled state
      paramval = receiveString[3] - '0';
      if (paramval == 1) {
        // enable the server
        ControllerData->set_mngsrvr_enable(V_ENABLED);
      } else {
        // stop and disable
        mngsrvr->stop();
        mngsrvr_status = V_STOPPED;
        ControllerData->set_mngsrvr_enable(V_NOTENABLED);
      }
      break;
    case 116:  // myFP2ESP32 get Management Server Start/Stop status
      build_reply('$', mngsrvr_status, clientnum);
      break;
    case 117:  // myFP2ESP32 set Management Server Start/Stop - this will start or stop the Management server
      paramval = receiveString[3] - '0';
      if (paramval == 1) {
        // start if enabled
        if (ControllerData->get_mngsrvr_enable() == V_ENABLED) {
          // enabled
          mngsrvr_status = mngsrvr->start(ControllerData->get_mngsrvr_port());
          if (mngsrvr_status != V_RUNNING) {
            debug_server_print(T_ERRSTART);
            debug_server_println(T_MANAGEMENTSERVER);
          }
        }
      } else {
        // stop
        mngsrvr->stop();
        mngsrvr_status = V_STOPPED;
      }
      break;

    case 118:  // myFP2ESP32 get cntlr_config.jsn
      {
        if (SPIFFS.exists("/cntlr_config.jsn") == false) {
          send_reply("TCP-cntlr_config.jsn !found", clientnum);
        } else {
          // file exists so open it
          File dfile = SPIFFS.open("/cntlr_config.jsn", "r");
          if (!dfile) {
            send_reply("TCP-118-cntlr_config.jsn !found", clientnum);
          } else {
            String cdata = dfile.readString();
            dfile.close();
            int len = cdata.length();
            char cd[len + 2];
            snprintf(cd, len + 2, "%c%s%c", '$', cdata, _EOFSTR);
            send_reply(cd, clientnum);
          }
        }
      }
      send_reply("TCP-err 118", clientnum);
      break;

    case 119:  // myFP2ESP32 get coil power state :B9#
      //deprecated
      break;
    case 120:  // myFP2ESP32 set coil power state :C0# (change only the coilpowestate) :C0x#
      // deprecated
      break;

    default:
      debug_server_print(T_TCPIPSERVER);
      debug_server_print(T_ERROR);
      debug_server_print(", cmd ");
      debug_server_println(receiveString);
      break;
  }
}
