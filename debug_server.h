// --------------------------------------------------------------------
// myFP2ESP32 DEBUG SERVER CLASS
// Copyright Robert Brown 2020-2023. All Rights Reserved.
// debug_server.h
// --------------------------------------------------------------------

#if !defined(_debug_server_h_)
#define _debug_server_h_

#include <WiFiServer.h>

// --------------------------------------------------------------------
// DEFINES
// --------------------------------------------------------------------

// --------------------------------------------------------------------
// CLASS
// --------------------------------------------------------------------
class DEBUG_SERVER {
public:
  DEBUG_SERVER();
  void begin();

  bool start_server();
  void stop_server(void);

  void check_client(void);
  bool get_serverstate(void);

  void print(const char *);
  void print(char);
  void print(int);
  void print(unsigned int);
  void print(double);
  void print(String);
  void print(long);
  void print(unsigned long);
  
  void println(char *);
  void println(char);
  void println(int);
  void println(unsigned int);
  void println(double);
  void println(String);
  void println(long);
  void println(unsigned long);
  
private:
  void processCommand(void);
  void clearclient(void);
  bool check_status(void);

  WiFiServer *_mydebugserver = NULL;
  WiFiClient *_myclient = NULL;
  IPAddress _myclientIPAddress;  // IP Address for connection
  bool _clientstatus = false;
  bool _serverstate = V_STOPPED;
  bool _loaded = false;
  char _clientaddress[IPADDRESSLEN] = { 0 };
};


#endif
