// ----------------------------------------------------------------------
// myFP2ESP32 ASCOM ALPACA SERVER CLASS
// Copyright Robert Brown 2014-2023. All Rights Reserved.
// Copyright Holger M, 2019-2021. All Rights Reserved.
// ascom_server.cpp
// Included by default
// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
// INCLUDES:
// ----------------------------------------------------------------------
#include <Arduino.h>
#include "controller_config.h"

#include "SPIFFS.h"
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
// ASCOM ALPACA DISCOVERY PROTOCOL USES UDP
#include <WiFiUdp.h>

// DEFAULT CONFIGURATION
// ASCOM ALPACA Server
// Discovery
// Setup
// Management API


// ----------------------------------------------------------------------
// CLASSES
// ----------------------------------------------------------------------
#include "controller_data.h"
extern CONTROLLER_DATA *ControllerData;

#include "driver_board.h"
extern DRIVER_BOARD *driverboard;

#include "ascom_server.h"
extern ASCOM_SERVER *ascomsrvr;

#include "temp_probe.h"
extern TEMP_PROBE *tempprobe;


// ----------------------------------------------------------------------
// EXTERNS
// ----------------------------------------------------------------------
extern void get_systemuptime();
extern char ipStr[];
extern volatile bool halt_alert;
extern portMUX_TYPE halt_alertMux;
extern long ftargetPosition;
extern byte isMoving;
extern float temp;
extern bool filesystemloaded;
// system uptime days:hours:minutes
extern char systemuptime[12];


// ----------------------------------------------------------------------
// DATA AND DEFINITIONS
// ----------------------------------------------------------------------
// ASCOM WEB PAGES
// stepmode
#define AS_SM1CHECKED "<input type=\"radio\" name=\"sm\" value=\"1\" Checked> Full"
#define AS_SM1UNCHECKED "<input type=\"radio\" name=\"sm\" value=\"1\"> Full"
#define AS_SM2CHECKED "<input type=\"radio\" name=\"sm\" value=\"2\" Checked> 2"
#define AS_SM2UNCHECKED "<input type=\"radio\" name=\"sm\" value=\"2\"> 2"
#define AS_SM4CHECKED "<input type=\"radio\" name=\"sm\" value=\"4\" Checked> 4"
#define AS_SM4UNCHECKED "<input type=\"radio\" name=\"sm\" value=\"4\"> 4"
#define AS_SM8CHECKED "<input type=\"radio\" name=\"sm\" value=\"8\" Checked> 8"
#define AS_SM8UNCHECKED "<input type=\"radio\" name=\"sm\" value=\"8\"> 8"
#define AS_SM16CHECKED "<input type=\"radio\" name=\"sm\" value=\"16\" Checked> 16"
#define AS_SM16UNCHECKED "<input type=\"radio\" name=\"sm\" value=\"16\"> 16"
#define AS_SM32CHECKED "<input type=\"radio\" name=\"sm\" value=\"32\" Checked> 32"
#define AS_SM32UNCHECKED "<input type=\"radio\" name=\"sm\" value=\"32\"> 32"
// motorspeed
#define AS_MSSLOWCHECKED "<input type=\"radio\" name=\"ms\" value=\"0\" Checked> Slow"
#define AS_MSSLOWUNCHECKED "<input type=\"radio\" name=\"ms\" value=\"0\"> Slow"
#define AS_MSMEDCHECKED "<input type=\"radio\" name=\"ms\" value=\"1\" Checked> Medium"
#define AS_MSMEDUNCHECKED "<input type=\"radio\" name=\"ms\" value=\"1\"> Medium"
#define AS_MSFASTCHECKED "<input type=\"radio\" name=\"ms\" value=\"2\" Checked> Fast"
#define AS_MSFASTUNCHECKED "<input type=\"radio\" name=\"ms\" value=\"2\"> Fast"

// ASCOM CONST VARS
#define ASCOMGUID "7e239e71-d304-4e7e-acda-3ff2e2b68515"
#define ASCOMMAXIMUMARGS 10
#define ASCOMNOTIMPLEMENTED 0x400

// ASCOM MESSAGES
#define ASCOMDESCRIPTION "\"ASCOM driver for myFP2ESP32 controllers\""
#define ASCOMDRIVERINFO "\"myFP2ESP32 ALPACA SERVER (c) R. Brown. 2020-2023\""
#define ASCOMMANAGEMENTINFO "{\"ServerName\":\"myFP2ESP32\",\"Manufacturer\":\"R. Brown\",\"ManufacturerVersion\":\"v1.1\",\"Location\":\"New Zealand\"}"
#define ASCOMNAME "\"myFP2ESPASCOMR\""
#define ASCOMSERVERNOTFOUNDSTR "<html><head><title>ASCOM ALPACA Server</title></head><body><p>File system not started</p><p><a href=\"/setup/v1/focuser/0/setup\">Setup page</a></p></body></html>"
#define T_NOTIMPLEMENTED "not implemented"

#define GeT true
#define PosT false
bool setup_type;

// instance of ASCOM Discovery via UDP. Does not work if inside class
WiFiUDP _ASCOMDISCOVERYUdp;


// ----------------------------------------------------------------------
// HELPERS
// ----------------------------------------------------------------------
// / or /setup
void ascomget_setup() {
  ascomsrvr->get_setup();
}

void ascomget_notfound() {
  ascomsrvr->get_notfound();
}

// MANAGEMENT

void ascomget_man_version() {
  ascomsrvr->get_man_versions();
}

void ascomget_man_description() {
  ascomsrvr->get_man_description();
}

void ascomget_man_configureddevices() {
  ascomsrvr->get_man_configureddevices();
}

// /setup/v1/focuser/0/setup
// get
void ascomget_focusersetup() {
  setup_type = GeT;
  ascomsrvr->get_focusersetup();
}

// /setup/v1/focuser/0/setup
// put
void ascomset_focusersetup() {
  setup_type = PosT;
  ascomsrvr->get_focusersetup();
}

void ascomset_connected() {
  ascomsrvr->set_connected();
}

void ascomget_interfaceversion() {
  ascomsrvr->get_interfaceversion();
}

void ascomget_name() {
  ascomsrvr->get_name();
}

void ascomget_description() {
  ascomsrvr->get_description();
}

void ascomget_driverinfo() {
  ascomsrvr->get_driverinfo();
}

void ascomget_driverversion() {
  ascomsrvr->get_driverversion();
}

void ascomget_absolute() {
  ascomsrvr->get_absolute();
}

void ascomget_maxstep() {
  ascomsrvr->get_maxstep();
}

void ascomget_maxincrement() {
  ascomsrvr->get_maxincrement();
}

void ascomget_temperature() {
  ascomsrvr->get_temperature();
}

void ascomget_position() {
  ascomsrvr->get_position();
}

void ascomset_halt() {
  ascomsrvr->set_halt();
}

void ascomget_ismoving() {
  ascomsrvr->get_ismoving();
}

void ascomget_stepsize() {
  ascomsrvr->get_stepsize();
}

void ascomget_connected() {
  ascomsrvr->get_connected();
}

void ascomget_tempcomp() {
  ascomsrvr->get_tempcomp();
}

void ascomset_tempcomp() {
  ascomsrvr->set_tempcomp();
}

void ascomget_tempcompavailable() {
  ascomsrvr->get_tempcompavailable();
}

void ascomset_move() {
  ascomsrvr->set_move();
}

void ascomget_supportedactions() {
  ascomsrvr->get_supportedactions();
}


// ----------------------------------------------------------------------
// ASCOM ALPACA REMOTE SERVER CLASS
// ----------------------------------------------------------------------
ASCOM_SERVER::ASCOM_SERVER() {
}

// ----------------------------------------------------------------------
// bool start(void);
// Create and start the ASCOM REMOTE SERVER
// ----------------------------------------------------------------------
bool ASCOM_SERVER::start(void) {
  debug_server_print(T_START);
  debug_server_println(T_ALPACA);

  // if the server is not enabled then return
  if (ControllerData->get_ascomsrvr_enable() == V_NOTENABLED) {
    debug_server_println(T_NOTENABLED);
    return false;
  }

  // prevent any attempt to start if already started
  if ((this->_loaded == true) && (this->_state == V_RUNNING)) {
    return true;
  }

  // check access to ASCOM web html files
  if (!filesystemloaded) {
    debug_server_println(T_ERRORFILESYSTEM);
    file_sys_error();
    return false;
  }

  // if _ascomserver has not already been created
  if (this->_loaded == false) {
    // create instance of an ASCOM server
    _ascomserver = new WebServer(ControllerData->get_ascomsrvr_port());
  }

  // check alpaca discovery state: ensure it is running
  if (this->_discoverystate == V_STOPPED) {
    debug_server_println("-start discovery");
    _ASCOMDISCOVERYUdp.begin(ASCOMDISCOVERYPORT);
    this->_discoverystate = V_RUNNING;
  }

  // setup all the url requests
  _ascomserver->on("/", ascomget_setup);
  _ascomserver->on("/setup", ascomget_setup);
  // handle Management requests
  _ascomserver->on("/management/apiversions", ascomget_man_version);
  _ascomserver->on("/management/v1/description", ascomget_man_description);
  _ascomserver->on("/management/v1/configureddevices", ascomget_man_configureddevices);
  // handle ASCOM driver client requests
  _ascomserver->on("/setup/v1/focuser/0/setup", HTTP_GET, ascomget_focusersetup);
  _ascomserver->on("/setup/v1/focuser/0/setup", HTTP_POST, ascomget_focusersetup);
  _ascomserver->on("/api/v1/focuser/0/connected", HTTP_PUT, ascomset_connected);
  _ascomserver->on("/api/v1/focuser/0/interfaceversion", HTTP_GET, ascomget_interfaceversion);
  _ascomserver->on("/api/v1/focuser/0/name", HTTP_GET, ascomget_name);
  _ascomserver->on("/api/v1/focuser/0/description", HTTP_GET, ascomget_description);
  _ascomserver->on("/api/v1/focuser/0/driverinfo", HTTP_GET, ascomget_driverinfo);
  _ascomserver->on("/api/v1/focuser/0/driverversion", HTTP_GET, ascomget_driverversion);
  _ascomserver->on("/api/v1/focuser/0/absolute", HTTP_GET, ascomget_absolute);
  _ascomserver->on("/api/v1/focuser/0/maxstep", HTTP_GET, ascomget_maxstep);
  _ascomserver->on("/api/v1/focuser/0/maxincrement", HTTP_GET, ascomget_maxincrement);
  _ascomserver->on("/api/v1/focuser/0/temperature", HTTP_GET, ascomget_temperature);
  _ascomserver->on("/api/v1/focuser/0/position", HTTP_GET, ascomget_position);
  _ascomserver->on("/api/v1/focuser/0/halt", HTTP_PUT, ascomset_halt);
  _ascomserver->on("/api/v1/focuser/0/ismoving", HTTP_GET, ascomget_ismoving);
  _ascomserver->on("/api/v1/focuser/0/stepsize", HTTP_GET, ascomget_stepsize);
  _ascomserver->on("/api/v1/focuser/0/connected", HTTP_GET, ascomget_connected);
  _ascomserver->on("/api/v1/focuser/0/tempcomp", HTTP_GET, ascomget_tempcomp);
  _ascomserver->on("/api/v1/focuser/0/tempcomp", HTTP_PUT, ascomset_tempcomp);
  _ascomserver->on("/api/v1/focuser/0/tempcompavailable", HTTP_GET, ascomget_tempcompavailable);
  _ascomserver->on("/api/v1/focuser/0/move", HTTP_PUT, ascomset_move);
  _ascomserver->on("/api/v1/focuser/0/supportedactions", HTTP_GET, ascomget_supportedactions);
  // handle url not found 404
  _ascomserver->onNotFound(ascomget_notfound);
  _ascomserver->begin();

  this->_loaded = true;
  this->_state = V_RUNNING;
  return this->_loaded;
}

// ----------------------------------------------------------------------
// void stop(void);
// Stop the ASCOM REMOTE SERVER
// This will stop and delete the _ascomserver
// This must be done because start() creates _ascomserver
// ----------------------------------------------------------------------
void ASCOM_SERVER::stop(void) {
  debug_server_print(T_STOP);
  debug_server_println(T_ALPACA);

  if (this->_loaded == true) {
    // stop discovery if running
    if (this->_discoverystate == true) {
      debug_server_print(T_STOP);
      debug_server_println(T_DISCOVERY);
      _ASCOMDISCOVERYUdp.stop();
      this->_discoverystate = false;
    }
    // stop ASCOM ALPACA Serverr
    _ascomserver->stop();
  }
  delete _ascomserver;
  this->_loaded = false;
  this->_state = V_STOPPED;
}

// ----------------------------------------------------------------------
// Checks for any new clients or existing client requests
// ----------------------------------------------------------------------
void ASCOM_SERVER::loop() {
  // avoid a crash
  if (this->_loaded == false) {
    return;
  }
  _ascomserver->handleClient();
  // check for ASCOM discovery received packets
  ascomsrvr->checkASCOMALPACADiscovery();
}

// ----------------------------------------------------------------------
// Get state, returns V_RUNNING or V_STOPPED
// ----------------------------------------------------------------------
byte ASCOM_SERVER::get_state(void) {
  return this->_state;
}

// ----------------------------------------------------------------------
// Get loaded, returns true (1) is loaded and running, else false (0)
// ----------------------------------------------------------------------
byte ASCOM_SERVER::get_loaded(void) {
  return this->_loaded;
}

// ----------------------------------------------------------------------
// Print not loaded msg
// ----------------------------------------------------------------------
void ASCOM_SERVER::notloaded(void) {
  debug_server_print(T_ALPACA);
  debug_server_println(T_NOTLOADED);
}

// ----------------------------------------------------------------------
// File System Not Loaded - send not found page to client
// ----------------------------------------------------------------------
void ASCOM_SERVER::file_sys_error(void) {
  // file does not exist
  debug_server_print(T_ALPACA);
  debug_server_println(T_ERRORFILESYSTEM);
  String msg = H_FSNOTLOADEDSTR;
  _ascomserver->send(NOTFOUNDWEBPAGE, PLAINTEXTPAGETYPE, msg);
}

// ----------------------------------------------------------------------
// Send reponse header to client
// ----------------------------------------------------------------------
void ASCOM_SERVER::sendmyheader() {
  _ascomserver->client().println("HTTP/1.1 200 OK");
  _ascomserver->client().println("Content-type:text/html");
  _ascomserver->client().println();
}

// ----------------------------------------------------------------------
// Send response body to client
// ----------------------------------------------------------------------
void ASCOM_SERVER::sendmycontent(String pg) {
  _ascomserver->client().print(pg);
}

// ----------------------------------------------------------------------
// ASCOM ALPCACA DISCOVERY
// ----------------------------------------------------------------------
// void checkASCOMALPACADiscovery(void);
// Grunt code for handling of Discovery packets
// ----------------------------------------------------------------------
void ASCOM_SERVER::checkASCOMALPACADiscovery(void) {
  // (c) Daniel VanNoord
  // https://github.com/DanielVanNoord/AlpacaDiscoveryTests/blob/master/Alpaca8266/Alpaca8266.ino
  // if there's data available, read a packet
  debug_server_print(T_ALPACA);
  debug_server_println(T_DISCOVERY);
  int packetSize = _ASCOMDISCOVERYUdp.parsePacket();
  if (packetSize) {
    char ipaddr[16];
    IPAddress remoteIp = _ASCOMDISCOVERYUdp.remoteIP();
    snprintf(ipaddr, sizeof(ipaddr), "%i.%i.%i.%i", remoteIp[0], remoteIp[1], remoteIp[2], remoteIp[3]);
    debug_server_print("-request from ");
    debug_server_println(ipaddr);
    //debug_server_print(" port ");
    //debug_server_println(String(_ASCOMDISCOVERYUdp.remotePort()));
    // read the packet into packetBufffer
    int len = _ASCOMDISCOVERYUdp.read(_packetBuffer, 255);
    if (len > 0) {
      // Ensure that it is null terminated
      _packetBuffer[len] = 0;
    }
    // No undersized packets allowed
    if (len < 16) {
      debug_server_println("-err pkt undersized");
      return;
    }

    // 0-14 "alpacadiscovery", 15 ASCII Version number of 1
    if (strncmp("alpacadiscovery1", _packetBuffer, 16) != 0) {
      debug_server_println("-err pkt header");
      return;
    }

    String strresponse = "{\"alpacaport\":" + String(ControllerData->get_ascomsrvr_port()) + "}";
    uint8_t response[36] = { 0 };
    len = strresponse.length();
    debug_server_print("-response ");
    debug_server_println(strresponse);
    // copy to response
    for (int i = 0; i < len; i++) {
      response[i] = (uint8_t)strresponse[i];
    }
    _ASCOMDISCOVERYUdp.beginPacket(_ASCOMDISCOVERYUdp.remoteIP(), _ASCOMDISCOVERYUdp.remotePort());
    _ASCOMDISCOVERYUdp.write(response, len);
    _ASCOMDISCOVERYUdp.endPacket();
  }
}


// ----------------------------------------------------------------------
// sendreply
// send a reply to client
// ----------------------------------------------------------------------
void ASCOM_SERVER::sendreply(int replycode, String contenttype, String jsonstr) {
  // ascomserver.send builds the http header, jsonstr will be in the body
  _ascomserver->send(replycode, contenttype, jsonstr);
}

// ----------------------------------------------------------------------
// getURLParameters
// get all args client sent as part of request
// ----------------------------------------------------------------------
void ASCOM_SERVER::getURLParameters() {
  String str;
  // get server args, translate server args to lowercase, they can be mixed case
  debug_server_print("Alpaca-getURLParameters ");
  debug_server_println(_ascomserver->args());
  for (int i = 0; i < _ascomserver->args(); i++) {
    if (i >= ASCOMMAXIMUMARGS) {
      break;
    }
    str = _ascomserver->argName(i);
    str.toLowerCase();
    if (str.equals("clientid")) {
      _ASCOMClientID = (unsigned int)_ascomserver->arg(i).toInt();
      debug_server_print("-clientID ");
      debug_server_println(_ASCOMClientID);
    }
    if (str.equals("clienttransactionid")) {
      _ASCOMClientTransactionID = (unsigned int)_ascomserver->arg(i).toInt();
      debug_server_print("-clienttransactionID ");
      debug_server_println(_ASCOMClientTransactionID);
    }
    if (str.equals("tempcomp")) {
      String strtmp = _ascomserver->arg(i);
      strtmp.toLowerCase();
      if (strtmp.equals("true")) {
        debug_server_println("-tempcomp true");
        _ASCOMTempCompState = 1;
      } else {
        debug_server_println("-tempcomp false");
        _ASCOMTempCompState = 0;
      }
    }
    if (str.equals("position")) {
      String str1 = _ascomserver->arg(i);
      debug_server_print("-position ");
      debug_server_println(str1);
      // this returns a long data type
      _ASCOMpos = _ascomserver->arg(i).toInt();
    }
    if (str.equals("connected")) {
      String strtmp = _ascomserver->arg(i);
      strtmp.toLowerCase();
      debug_server_print("-connected ");
      debug_server_println(strtmp);
      if (strtmp.equals("true")) {
        _ASCOMConnectedState = 1;
      } else {
        _ASCOMConnectedState = 0;
      }
    }
  }
}

// ----------------------------------------------------------------------
// addclientinfo
// Adds client info to reply
// ----------------------------------------------------------------------
String ASCOM_SERVER::addclientinfo(String str) {
  String str1 = str;
  // add clientid
  str1 = str1 + "\"ClientID\":" + String(_ASCOMClientID) + ",";
  // add clienttransactionid
  str1 = str1 + "\"ClientTransactionID\":" + String(_ASCOMClientTransactionID) + ",";
  // add ServerTransactionID
  str1 = str1 + "\"ServerTransactionID\":" + String(_ASCOMServerTransactionID) + ",";
  // add errornumber
  str1 = str1 + "\"ErrorNumber\":" + String(_ASCOMErrorNumber) + ",";
  // add errormessage
  str1 = str1 + "\"ErrorMessage\":\"" + _ASCOMErrorMessage + "\"}";
  return str1;
}

// ----------------------------------------------------------------------
// get_setup()
// Send page to client
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_setup() {
  // url /setup [mapped to /ascomhome]
  // The web page must describe the overall device, including name, manufacturer and version number.
  // content-type: text/html
  String _ASpg;
  _ASpg.reserve(1900);  // 306-02  1592

  debug_server_println("Alpaca-get_setup()");

  if (this->_loaded == false) {
    notloaded();
    return;
  }

  // spiffs was started earlier when server was started so assume it has started
  if (SPIFFS.exists("/ascomhome.html")) {
    debug_server_println("-get /ascomhome.html");

    File file = SPIFFS.open("/ascomhome.html", "r");
    _ASpg = file.readString();
    file.close();

    // process for dynamic data
    String bcol = ControllerData->get_wp_backcolor();
    _ASpg.replace("%BKC%", bcol);
    String txtcol = ControllerData->get_wp_textcolor();
    _ASpg.replace("%TXC%", txtcol);
    String ticol = ControllerData->get_wp_titlecolor();
    _ASpg.replace("%TIC%", ticol);
    String hcol = ControllerData->get_wp_headercolor();
    _ASpg.replace("%HEC%", hcol);
    // IP Address
    _ASpg.replace("%IPS%", ipStr);
    // Alpaca port number
    _ASpg.replace("%ALP%", String(ControllerData->get_ascomsrvr_port()));
    // Discovery state
    if (_discoverystate == V_STOPPED) {
      _ASpg.replace("%DIS%", T_STOPPED);
    } else {
      _ASpg.replace("%DIS%", T_RUNNING);
    }
    // Discovery Port
    _ASpg.replace("%DIP%", String(ASCOMDISCOVERYPORT));
    // Project URL - this is inline in .html file
    // footer
    _ASpg.replace("%NAM%", ControllerData->get_brdname());
    _ASpg.replace("%VER%", String(program_version));
    _ASpg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    _ASpg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    _ASpg = ASCOMSERVERNOTFOUNDSTR;
  }
  _ASCOMServerTransactionID++;

  debug_server_print("/ascomhome.html ");
  debug_server_println(_ASpg.length());
  sendmyheader();
  sendmycontent(_ASpg);
}

// ----------------------------------------------------------------------
// get_focusersetup()
// constructs ASCOM setup server page url:/setup/v1/focuser/0/setup
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_focusersetup() {
  debug_server_println("Alpaca-get_focusersetup()");

  // setup/v1/focuser/0/setup
  if (this->_loaded == false) {
    notloaded();
    return;
  }

  String _ASpg;
  _ASpg.reserve(3500);  // 306-02  3157

  if (setup_type == PosT) {
    // if set focuser position
    String fpos_str = _ascomserver->arg("setpos");
    if (fpos_str != "") {
      debug_server_print("-set position ");
      debug_server_println(fpos_str);
      String fp = _ascomserver->arg("fp");
      if (fp != "") {
        long tp = 0;
        tp = fp.toInt();
        tp = (tp < 0) ? 0 : tp;
        tp = (tp > ControllerData->get_maxstep()) ? ControllerData->get_maxstep() : tp;
        ftargetPosition = tp;
        driverboard->setposition(ftargetPosition);
        ControllerData->set_fposition(ftargetPosition);
      }
      goto Get_Handler;
    }

    // if update of maxsteps
    String fmax_str = _ascomserver->arg("fm");
    if (fmax_str != "") {
      long tp = 0;
      debug_server_print("-set maxsteps ");
      debug_server_println(fmax_str);
      tp = fmax_str.toInt();
      // if maxstep is less than focuser position
      if (tp < (long)driverboard->getposition()) {
        tp = (long)driverboard->getposition() + 10;
      }
      tp = (tp < FOCUSERLOWERLIMIT) ? FOCUSERLOWERLIMIT : tp;
      tp = (tp > FOCUSERUPPERLIMIT) ? FOCUSERUPPERLIMIT : tp;
      ControllerData->set_maxstep(tp);
      goto Get_Handler;
    }

    // if update motorspeed
    String fms_str = _ascomserver->arg("ms");
    if (fms_str != "") {
      debug_server_print("-set motorspeed ");
      debug_server_println(fms_str);
      int temp1 = fms_str.toInt();
      if (temp1 < SLOW) {
        temp1 = SLOW;
      }
      if (temp1 > FAST) {
        temp1 = FAST;
      }
      ControllerData->set_motorspeed(temp1);
      goto Get_Handler;
    }

    // if update coilpower
    String fcp_str = _ascomserver->arg("cp");
    if (fcp_str != "") {
      debug_server_print("-set coilpower ");
      debug_server_println(fcp_str);
      if (fcp_str == "cp") {
        ControllerData->set_coilpower_enable(V_ENABLED);
      } else {
        ControllerData->set_coilpower_enable(V_NOTENABLED);
      }
      goto Get_Handler;
    }

    // if update reverse direction
    String frd_str = _ascomserver->arg("rd");
    if (frd_str != "") {
      debug_server_print("-set reverse ");
      debug_server_println(frd_str);
      if (frd_str == "rd") {
        ControllerData->set_reverse_enable(V_ENABLED);
      } else {
        ControllerData->set_reverse_enable(V_NOTENABLED);
      }
      goto Get_Handler;
    }

    // ----------------------------------------------------------------------
    // Basic rule for setting stepmode
    // Set driverboard->setstepmode(xx);
    // this sets the physical pins and saves new step mode
    // ----------------------------------------------------------------------
    // if update stepmode
    // (1=Full, 2=Half, 4=1/4, 8=1/8, 16=1/16, 32=1/32, 64=1/64, 128=1/128, 256=1/256)

    String fsm_str = _ascomserver->arg("sm");
    if (fsm_str != "") {
      int tmp = 0;
      debug_server_print("-set stepmode ");
      debug_server_println(fsm_str);
      tmp = fsm_str.toInt();
      if (tmp < STEP1) {
        tmp = STEP1;
      }
      if (tmp > ControllerData->get_brdmaxstepmode()) {
        tmp = ControllerData->get_brdmaxstepmode();
      }
      // call boards.cpp to apply physical pins and save new stepmode
      driverboard->setstepmode(tmp);
      goto Get_Handler;
    }

    // if update stepsize state
    String ss_str = _ascomserver->arg("ss");
    if (ss_str != "") {
      debug_server_print("-set stepsize state ");
      debug_server_println(ss_str);
      if (ss_str == "ss") {
        ControllerData->set_stepsize_enable(V_ENABLED);
      } else {
        ControllerData->set_stepsize_enable(V_NOTENABLED);
      }
      goto Get_Handler;
    }

    // if set step size value
    String ssv_str = _ascomserver->arg("setss");
    if (ssv_str != "") {
      String ss = _ascomserver->arg("ssv");
      if (ss != "") {
        debug_server_print("-set stepsize value ");
        debug_server_println(ssv_str);
        float tempstepsize = 0.0;
        tempstepsize = ss.toFloat();
        tempstepsize = (tempstepsize < MINIMUMSTEPSIZE) ? MINIMUMSTEPSIZE : tempstepsize;
        tempstepsize = (tempstepsize > MAXIMUMSTEPSIZE) ? MAXIMUMSTEPSIZE : tempstepsize;
        ControllerData->set_stepsize(tempstepsize);
      }
      get_focusersetup();
      return;
    }
    goto Get_Handler;
  }


Get_Handler:

  // Build /ascomsetup
  debug_server_println("Alpaca-request /setup/v1/focuser/0/setup");
  // convert current values of focuserposition and focusermaxsteps to string types
  String fpbuffer = String(driverboard->getposition());
  String mxbuffer = String(ControllerData->get_maxstep());
  String smbuffer = String(ControllerData->get_brdstepmode());
  switch (ControllerData->get_brdstepmode()) {
    case 1:
      smbuffer = AS_SM1CHECKED;
      smbuffer = smbuffer + AS_SM2UNCHECKED;
      smbuffer = smbuffer + AS_SM4UNCHECKED;
      smbuffer = smbuffer + AS_SM8UNCHECKED;
      smbuffer = smbuffer + AS_SM16UNCHECKED;
      smbuffer = smbuffer + AS_SM32UNCHECKED;
      break;
    case 2:
      smbuffer = AS_SM1UNCHECKED;
      smbuffer = smbuffer + AS_SM2CHECKED;
      smbuffer = smbuffer + AS_SM4UNCHECKED;
      smbuffer = smbuffer + AS_SM8UNCHECKED;
      smbuffer = smbuffer + AS_SM16UNCHECKED;
      smbuffer = smbuffer + AS_SM32UNCHECKED;
      break;
    case 4:
      smbuffer = AS_SM1UNCHECKED;
      smbuffer = smbuffer + AS_SM2UNCHECKED;
      smbuffer = smbuffer + AS_SM4CHECKED;
      smbuffer = smbuffer + AS_SM8UNCHECKED;
      smbuffer = smbuffer + AS_SM16UNCHECKED;
      smbuffer = smbuffer + AS_SM32UNCHECKED;
      break;
    case 8:
      smbuffer = AS_SM1UNCHECKED;
      smbuffer = smbuffer + AS_SM2UNCHECKED;
      smbuffer = smbuffer + AS_SM4UNCHECKED;
      smbuffer = smbuffer + AS_SM8CHECKED;
      smbuffer = smbuffer + AS_SM16UNCHECKED;
      smbuffer = smbuffer + AS_SM32UNCHECKED;
      break;
    case 16:
      smbuffer = AS_SM1UNCHECKED;
      smbuffer = smbuffer + AS_SM2UNCHECKED;
      smbuffer = smbuffer + AS_SM4UNCHECKED;
      smbuffer = smbuffer + AS_SM8UNCHECKED;
      smbuffer = smbuffer + AS_SM16CHECKED;
      smbuffer = smbuffer + AS_SM32UNCHECKED;
      break;
    case 32:
      smbuffer = AS_SM1UNCHECKED;
      smbuffer = smbuffer + AS_SM2UNCHECKED;
      smbuffer = smbuffer + AS_SM4UNCHECKED;
      smbuffer = smbuffer + AS_SM8UNCHECKED;
      smbuffer = smbuffer + AS_SM16UNCHECKED;
      smbuffer = smbuffer + AS_SM32CHECKED;
      break;
    default:
      smbuffer = AS_SM1CHECKED;
      smbuffer = smbuffer + AS_SM2UNCHECKED;
      smbuffer = smbuffer + AS_SM4UNCHECKED;
      smbuffer = smbuffer + AS_SM8UNCHECKED;
      smbuffer = smbuffer + AS_SM16UNCHECKED;
      smbuffer = smbuffer + AS_SM32UNCHECKED;
      break;
  }
  String msbuffer = String(ControllerData->get_motorspeed());
  switch (ControllerData->get_motorspeed()) {
    case 0:
      msbuffer = AS_MSSLOWCHECKED;
      msbuffer = msbuffer + AS_MSMEDUNCHECKED;
      msbuffer = msbuffer + AS_MSFASTUNCHECKED;
      break;
    case 1:
      msbuffer = AS_MSSLOWUNCHECKED;
      msbuffer = msbuffer + AS_MSMEDCHECKED;
      msbuffer = msbuffer + AS_MSFASTUNCHECKED;
      break;
    case 2:
      msbuffer = AS_MSSLOWUNCHECKED;
      msbuffer = msbuffer + AS_MSMEDUNCHECKED;
      msbuffer = msbuffer + AS_MSFASTCHECKED;
      break;
    default:
      msbuffer = AS_MSSLOWUNCHECKED;
      msbuffer = msbuffer + AS_MSMEDUNCHECKED;
      msbuffer = msbuffer + AS_MSFASTCHECKED;
      break;
  }
  String cpbuffer;
  if (!ControllerData->get_coilpower_enable()) {
    cpbuffer = "<input type=\"checkbox\" name=\"cp\" value=\"cp\" > ";
  } else {
    cpbuffer = "<input type=\"checkbox\" name=\"cp\" value=\"cp\" Checked> ";
  }
  String rdbuffer;
  if (!ControllerData->get_reverse_enable()) {
    rdbuffer = "<input type=\"checkbox\" name=\"rd\" value=\"rd\" > ";
  } else {
    rdbuffer = "<input type=\"checkbox\" name=\"rd\" value=\"rd\" Checked> ";
  }
  String stepsizebuffer;
  if (!ControllerData->get_stepsize_enable()) {
    stepsizebuffer = "<input type=\"checkbox\" name=\"ss\" value=\"ss\" > ";
  } else {
    stepsizebuffer = "<input type=\"checkbox\" name=\"ss\" value=\"ss\" Checked> ";
  }

  // various sections are built, now load the /ascomsetup page and replace sections
  // construct setup page of ascom server
  if (SPIFFS.exists("/ascomsetup.html")) {
    File file = SPIFFS.open("/ascomsetup.html", "r");
    _ASpg = file.readString();
    file.close();

    // process for dynamic data
    String bcol = ControllerData->get_wp_backcolor();
    _ASpg.replace("%BKC%", bcol);
    String txtcol = ControllerData->get_wp_textcolor();
    _ASpg.replace("%TXC%", txtcol);
    String ticol = ControllerData->get_wp_titlecolor();
    _ASpg.replace("%TIC%", ticol);
    String hcol = ControllerData->get_wp_headercolor();
    _ASpg.replace("%HEC%", hcol);
    // position
    _ASpg.replace("%FPB%", fpbuffer);
    // maxsteps
    _ASpg.replace("%MXB%", mxbuffer);
    // coil power
    _ASpg.replace("%CPB%", cpbuffer);
    // reverse
    _ASpg.replace("%RDB%", rdbuffer);
    // step mode
    _ASpg.replace("%SMB%", smbuffer);
    // motorspeed
    _ASpg.replace("%MSB%", msbuffer);
    // step size enable state
    _ASpg.replace("%SSS%", stepsizebuffer);
    // step size value
    String ssv = String(ControllerData->get_stepsize(), 2);
    _ASpg.replace("%SSV%", ssv);
    _ASpg.replace("%ssnum%", ssv);
    // footer
    _ASpg.replace("%NAM%", ControllerData->get_brdname());
    _ASpg.replace("%VER%", String(program_version));
    _ASpg.replace("%HEA%", String(ESP.getFreeHeap()));
    get_systemuptime();
    _ASpg.replace("%SUT%", systemuptime);
  } else {
    debug_server_println(T_NOTFOUND);
    _ASpg = ASCOMSERVERNOTFOUNDSTR;
  }
  // now send the ASCOM setup page
  _ASCOMServerTransactionID++;
  debug_server_print("/setup/v1/focuser/0/setup ");
  debug_server_println(_ASpg.length());
  sendmyheader();
  sendmycontent(_ASpg);
}


// ----------------------------------------------------------------------
// Management API functions
// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
// get_man_versions()
// handle request for version
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_man_versions() {
  // url /management/apiversions
  // Returns an integer array of supported Alpaca API version numbers.
  // { "Value": [1,2,3,4],"ClientTransactionID": 9876,"ServerTransactionID": 54321}

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  jsonretstr = "{\"Value\":[1]," + addclientinfo(jsonretstr);

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_man_description()
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_man_description() {
  // url /management/v1/description
  // Returns cross-cutting information that applies to all devices available at this URL:Port.
  // content-type: application/json
  // { "Value": { "ServerName": "Random Alpaca Device", "Manufacturer": "The Briliant Company",
  //   "ManufacturerVersion": "v1.0.0", "Location": "Horsham, UK" },
  //   "ClientTransactionID": 9876, "ServerTransactionID": 54321 }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  jsonretstr = "{\"Value\":" + String(ASCOMMANAGEMENTINFO) + "," + addclientinfo(jsonretstr);

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_man_configureddevices()
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_man_configureddevices() {
  // url /management/v1/configureddevices
  // Returns an array of device description objects, providing unique information for each served device, enabling them to be accessed through the Alpaca Device API.
  // content-type: application/json
  // { "Value": [{"DeviceName": "Super focuser 1","DeviceType": "Focuser","DeviceNumber": 0,"UniqueID": "277C652F-2AA9-4E86-A6A6-9230C42876FA"}],"ClientTransactionID": 9876,"ServerTransactionID": 54321}

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  jsonretstr = "{\"Value\":[{\"DeviceName\":" + String(ASCOMNAME) + ",\"DeviceType\":\"focuser\",\"DeviceNumber\":0,\"UniqueID\":\"" + String(ASCOMGUID) + "\"}]," + addclientinfo(jsonretstr);

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// ASCOM ALPACA API
// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
// get_interfaceversion()
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_interfaceversion() {
  // curl -X GET "http://192.168.2.128:4040/api/v1/focuser/0/interfaceversion?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // response {"value":2,"ClientID":1,"ClientTransactionID":1234,"ServerTransactionID":1,"ErrorNumber":"0","ErrorMessage":"ok"}

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  //jsonretstr = "{\"value\":2," + addclientinfo( jsonretstr );
  jsonretstr = "{ \"value\":2, \"errornumber\":0, \"errormessage\":\"\" }";

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// set_connected()
// ----------------------------------------------------------------------
void ASCOM_SERVER::set_connected() {
  // curl -X PUT 192.168.2.128:4040/api/v1/focuser/0/connected -H  "accept: application/json" -H  "Content-Type: application/x-www-form-urlencoded" -d "Connected=true&ClientID=1&ClientTransactionID=2"
  // response { "errornumber":0, "errormessage":"" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();  // checks connected param and sets _ASCOMConnectedState
  jsonretstr = "{ \"errornumber\":0, \"errormessage\":\"\" }";

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_connected()
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_connected() {
  // curl -X GET "192.168.2.128:4040/api/v1/focuser/0/connected?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // response { "value":false, "errornumber":0, "errormessage":"ok" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  // getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  jsonretstr = "{\"value\":";
  if (_ASCOMConnectedState == 0) {
    jsonretstr = jsonretstr + "false, ";
  } else {
    jsonretstr = jsonretstr + "true, ";
  }
  jsonretstr = jsonretstr + "\"errornumber\":0, \"errormessage\": \"\" }";

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_absolute()
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_absolute() {
  // curl -X GET "/api/v1/focuser/0/absolute?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // {  "Value":true,"ErrorNumber": 0,"ErrorMessage":"" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  jsonretstr = "{\"value\":true,\"errornumber\":0,\"errormessage\":\"\" }";

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_description()
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_description() {
  // GET "/api/v1/focuser/0/description?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // {  "Value": "string",  "ErrorNumber": 0,  "ErrorMessage": "string" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();

  jsonretstr = "{\"value\":" + String(ASCOMDESCRIPTION) + ",\"errornumber\":0,\"errormessage\":\"\" }";

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_name()
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_name() {
  // curl -X GET "192.168.2.128:4040/api/v1/focuser/0/name?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // {"Value":"myFP2ESPASCOMR","ClientID":1,"ClientTransactionID":1234,"ServerTransactionID":2,"ErrorNumber":"0","ErrorMessage":""myFP2ESPASCOMR""}

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  jsonretstr = "{\"value\":" + String(ASCOMNAME) + ",\"errornumber\":0,\"errormessage\":\"\" }";

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_driverinfo()
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_driverinfo() {
  // curl -X GET "/api/v1/focuser/0/driverinfo?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // {  "Value": "string",  "ErrorNumber": 0,  "ErrorMessage": "string" }
  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  //jsonretstr = "{\"value\":\"" + String(ASCOMDRIVERINFO) + "\",\"errornumber\":0,\"errormessage\":\"\" }";
  jsonretstr = "{\"value\":" + String(ASCOMDRIVERINFO) + ",\"errornumber\":0,\"errormessage\":\"\" }";

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_driverversion()
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_driverversion() {
  // curl -X GET "/api/v1/focuser/0/driverversion?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // {  "Value": "string",  "ErrorNumber": 0,  "ErrorMessage": "string" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  jsonretstr = "{\"value\":\"" + String(program_version) + "\",\"errornumber\":0,\"errormessage\":\"\" }";

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_maxstep()
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_maxstep() {
  // curl -X GET "/api/v1/focuser/0/maxstep?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // {  "Value": 0,  "ErrorNumber": 0,  "ErrorMessage": "string" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  jsonretstr = "{\"value\":" + String(ControllerData->get_maxstep()) + ",\"errornumber\":0,\"errormessage\":\"\" }";

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_maxincrement()
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_maxincrement() {
  // curl -X GET "/api/v1/focuser/0/maxincrement?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // {  "Value": 0,  "ErrorNumber": 0,  "ErrorMessage": "string" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  jsonretstr = "{\"value\":" + String(ControllerData->get_maxstep()) + ",\"errornumber\":0,\"errormessage\":\"\" }";

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_temperature()
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_temperature() {
  // curl -X GET "/api/v1/focuser/0/temperature?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // {  "Value": 1.100000023841858,  "ErrorNumber": 0,  "ErrorMessage": "string" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  jsonretstr = "{\"value\":" + String(temp, 2) + ",\"errornumber\":0,\"errormessage\":\"\" }";

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_position()
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_position() {
  // curl -X GET "/api/v1/focuser/0/position?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // {  "Value": 0,  "ErrorNumber": 0,  "ErrorMessage": "string" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  jsonretstr = "{\"value\":" + String(driverboard->getposition()) + ",\"errornumber\":0,\"errormessage\":\"\" }";

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// set_halt()
//
// ----------------------------------------------------------------------
void ASCOM_SERVER::set_halt() {
  // curl -X PUT "/api/v1/focuser/0/halt" -H  "accept: application/json" -H  "Content-Type: application/x-www-form-urlencoded" -d "ClientID=22&ClientTransactionID=33"
  // { "ErrorNumber": 0, "ErrorMessage": "string" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  portENTER_CRITICAL(&halt_alertMux);
  halt_alert = true;
  portEXIT_CRITICAL(&halt_alertMux);

  //ftargetPosition = fcurrentPosition;
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  jsonretstr = "{ \"errornumber\":0,\"errormessage\":\"\" }";

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_ismoving()
//
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_ismoving() {
  // curl -X GET "/api/v1/focuser/0/ismoving?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // {  "Value": true,  "ErrorNumber": 0,  "ErrorMessage": "string" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  if (isMoving == 1) {
    jsonretstr = "{\"value\":1,\"errornumber\":0,\"errormessage\":\"\" }";
  } else {
    jsonretstr = "{\"value\":0,\"errornumber\":0,\"errormessage\":\"\" }";
  }

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_stepsize()
//
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_stepsize() {
  // curl -X GET "/api/v1/focuser/0/stepsize?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // {  "Value": 1.100000023841858,  "ErrorNumber": 0,  "ErrorMessage": "string" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  jsonretstr = "{\"value\":" + String(ControllerData->get_stepsize()) + ",\"errornumber\":0,\"errormessage\":\"\" }";

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_tempcomp()
//
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_tempcomp() {
  // curl -X GET "/api/v1/focuser/0/tempcomp?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // {  "Value": true,  "ErrorNumber": 0,  "ErrorMessage": "string" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // The state of temperature compensation mode (if available), else always False.
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  if (ControllerData->get_tempcomp_enable() == V_NOTENABLED) {
    jsonretstr = "{\"value\":false,\"errornumber\":0,\"errormessage\":\"\" }";
  } else {
    jsonretstr = "{\"value\":true,\"errornumber\":0,\"errormessage\":\"\" }";
  }
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// set_tempcomp()
//
// ----------------------------------------------------------------------
void ASCOM_SERVER::set_tempcomp() {
  // curl -X PUT "/api/v1/focuser/0/tempcomp" -H  "accept: application/json" -H  "Content-Type: application/x-www-form-urlencoded" -d "TempComp=true&Client=1&ClientTransactionIDForm=12"
  // {  "ErrorNumber": 0,  "ErrorMessage": "string" }

  // look for parameter tempcomp=true or tempcomp=false
  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  if (tempprobe->get_state() == true) {
    if (_ASCOMTempCompState == 1) {
      // turn on temperature compensation
      ControllerData->set_tempcomp_enable(V_ENABLED);
    } else {
      // turn off temperature compensation
      ControllerData->set_tempcomp_enable(V_NOTENABLED);
    }
    jsonretstr = "{ \"errornumber\":0,\"errormessage\":\"\" }";

    // sendreply builds http header, sets content type, and then sends jsonretstr
    sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
  } else {
    _ASCOMErrorNumber = ASCOMNOTIMPLEMENTED;
    _ASCOMErrorMessage = T_NOTIMPLEMENTED;
    jsonretstr = "{ \"errornumber\":" + String(_ASCOMErrorNumber) + ",\"errormessage\":\"" + String(_ASCOMErrorMessage) + "\" }";

    // sendreply builds http header, sets content type, and then sends jsonretstr
    sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
  }
}

// ----------------------------------------------------------------------
// get_tempcompavailable()
//
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_tempcompavailable() {
  // curl -X GET "/api/v1/focuser/0/tempcompavailable?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // {  "Value": true,  "ErrorNumber": 0,  "ErrorMessage": "string" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  getURLParameters();
  // addclientinfo adds clientid, clienttransactionid, servertransactionid, errornumber, errormessage and terminating }
  if (ControllerData->get_tempcomp_enable() == V_ENABLED) {
    jsonretstr = "{\"value\":true,\"errornumber\":0,\"errormessage\":\"\" }";
  } else {
    jsonretstr = "{\"value\":false,\"errornumber\":0,\"errormessage\":\"\" }";
  }

  // sendreply builds http header, sets content type, and then sends jsonretstr
  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// set_move()
//
// ----------------------------------------------------------------------
void ASCOM_SERVER::set_move() {
  // curl -X PUT "/api/v1/focuser/0/move" -H  "accept: application/json" -H  "Content-Type: application/x-www-form-urlencoded" -d "Position=1000&ClientID=22&ClientTransactionID=33"
  // {  "ErrorNumber": 0,  "ErrorMessage": "string" }

  // extract new value
  String jsonretstr = "";
  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  // get clientID and clienttransactionID
  getURLParameters();

  // destination is in _ASCOMpos
  // this is interfaceversion = 3, so moves are allowed when temperature compensation is on
  long newpos;
  if (_ASCOMpos <= 0) {
    newpos = 0L;
    ftargetPosition = newpos;
    jsonretstr = "{ \"errornumber\":0,\"errormessage\":\"\" }";

    sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
  } else {
    newpos = _ASCOMpos;
    if (newpos > ControllerData->get_maxstep()) {
      newpos = ControllerData->get_maxstep();
      ftargetPosition = newpos;
      jsonretstr = "{ \"errornumber\":0,\"errormessage\":\"\" }";

      sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
    } else {
      ftargetPosition = newpos;
      jsonretstr = "{ \"errornumber\":0,\"errormessage\":\"\" }";

      sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
    }
  }
}

// ----------------------------------------------------------------------
// get_supportedactions()
//
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_supportedactions() {
  // curl -X GET "/api/v1/focuser/0/supportedactions?ClientID=1&ClientTransactionID=1234" -H  "accept: application/json"
  // {  "Value": [    "string"  ],  "ErrorNumber": 0,  "ErrorMessage": "string" }

  String jsonretstr = "";

  _ASCOMServerTransactionID++;
  _ASCOMErrorNumber = 0;
  _ASCOMErrorMessage = "";
  // get clientID and clienttransactionID
  getURLParameters();
  jsonretstr = "{\"Value\": [\"isMoving\",\"MaxStep\",\"Temperature\",\"Position\",\"Absolute\",\"MaxIncrement\",\"StepSize\",\"TempComp\",\"TempCompAvailable\", ]," + addclientinfo(jsonretstr);

  sendreply(NORMALWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ----------------------------------------------------------------------
// get_notfound()
//
// ----------------------------------------------------------------------
void ASCOM_SERVER::get_notfound() {
  String message = T_NOTFOUND;
  String jsonretstr = "";

  message += "URI: ";
  message += _ascomserver->uri();
  message += "\nMethod: ";
  if (_ascomserver->method() == HTTP_GET) {
    message += "GET";
  } else if (_ascomserver->method() == HTTP_POST) {
    message += "POST";
  } else if (_ascomserver->method() == HTTP_PUT) {
    message += "PUT";
  } else if (_ascomserver->method() == HTTP_DELETE) {
    message += "DELETE";
  } else {
    message += "UNKNOWN_METHOD: " + _ascomserver->method();
  }
  message += "\nArguments: ";
  message += _ascomserver->args();
  message += "\n";
  for (uint8_t i = 0; i < _ascomserver->args(); i++) {
    message += " " + _ascomserver->argName(i) + ": " + _ascomserver->arg(i) + "\n";
  }

  _ASCOMErrorNumber = ASCOMNOTIMPLEMENTED;
  _ASCOMErrorMessage = T_NOTIMPLEMENTED;
  _ASCOMServerTransactionID++;
  jsonretstr = "{" + addclientinfo(jsonretstr);

  sendreply(BADREQUESTWEBPAGE, JSONPAGETYPE, jsonretstr);
}

// ASCOM ALPACA SERVER END ----------------------------------------------------------
