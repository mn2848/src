// ----------------------------------------------------------------------
// myFP2ESP32 MANAGEMENT SERVER ROUTINES
// Copyright Brown 2014-2023. All Rights Reserved.
// Copyright Holger M, 2019-2021. All Rights Reserved.
// management_server.h
// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
// Includes
// ----------------------------------------------------------------------
#include <Arduino.h>
#include <ArduinoJson.h>  // Benoit Blanchon https://github.com/bblanchon/ArduinoJson
#include "SPIFFS.h"
#include <WebServer.h>


// ----------------------------------------------------------------------
// MANAGEMENT SERVER CLASS
// ----------------------------------------------------------------------
class MANAGEMENT_SERVER {
public:
  MANAGEMENT_SERVER();
  bool start(unsigned long);
  void stop(void);
  void loop(bool);
  void get_notfound(void);
  void handlenotfound(void);
  void success(void);
  void fail(void);
  void send_redirect(String);
  void check_reboot(void);

  // admin
  void getadmin1(void);
  void getadmin2(void);
  void getadmin3(void);
  void getadmin4(void);
  void getadmin5(void);
  void getadmin6(void);
  void getadmin7(void);
  void getadmin8(void);
  void getadmin9(void);

  void getmove(void);
  void getlinks(void);

  // xhtml
  void getposition(void);
  void getismoving(void);
  void gettargetposition(void);

  // file management
  void get_filelist_long(void);
  void get_filelist_short(void);
  void post_deletefile();
  void get_deletefile(void);
  void upload_file(void);
  void post_uploadstart(void);
  void post_uploadfile(void);
  bool handlefileread(String);
  String get_uri(void);

  // get set command interface
  void handlecmds(void);
  void handleget(void);
  void handleset(void);

  // board management
  void brdedit(void);

  void rssi(void);
  void saveconfig(void);

private:
  bool check_access(void);
  void checkreboot(void);
  void focuser_moving(void);
  void file_sys_error(void);
  void send_myheader(void);
  void send_mycontent(String);
  void send_json(String);
  String get_contenttype(String);
  bool is_hexdigit(char);

  File _fsUploadFile;
  WebServer *mserver;
  unsigned int _port = MNGSERVERPORT;
  bool _loaded = false;
  bool _parkstate = true;
  byte _state = V_STOPPED;
  const char *T_MS = "MS-";
  const char *T_READFILE = "Read ";
  const char *T_POST = "Post ";
  const char *T_ERRAUTH = "Err Authentication";
  const char *T_STALLGUARD = "Stallguard ";
  const char *T_PHYSICALSWITCH = "PhysicalSwitch ";
  const char *T_NONE = "None ";
  const char *T_PB = "PushButtons ";
  const char *T_J1 = "Joystick1 ";
  const char *T_J2 = "Joystick2 "; 
  const char *T_SET = "Set ";
};
