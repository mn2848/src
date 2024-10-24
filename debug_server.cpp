// --------------------------------------------------------------------
// myFP2ESP32 DEBUG SERVER: HANDLES DEBUGGING
// Copyright Robert Brown 2020-2023. All Rights Reserved.
// debug_server.cpp
// Dependancies
//    ControllerData
//    Controller must be up and running as ACCESSPOINT or STATION
// --------------------------------------------------------------------


// --------------------------------------------------------------------
// INCLUDES
// --------------------------------------------------------------------
#include <Arduino.h>
#include <WiFiClient.h>
#include <WiFiServer.h>
#include <time.h>

#include "controller_defines.h"
#include "controller_config.h"

#include "controller_data.h"
extern CONTROLLER_DATA *ControllerData;

// debug server
#include "debug_server.h"
extern DEBUG_SERVER *debugserver;


// --------------------------------------------------------------------
// EXTERNS
// --------------------------------------------------------------------
extern bool debugsrvr_status;


// --------------------------------------------------------------------
// DEFINES
// --------------------------------------------------------------------
#define _connected true
#define _disconnected false


// --------------------------------------------------------------------
// CLASS
// --------------------------------------------------------------------
DEBUG_SERVER::DEBUG_SERVER() {
  _serverstate = V_STOPPED;
  _loaded = false;
  _clientstatus = _disconnected;
}

// Clear debug client
// --------------------------------------------------------------------
void DEBUG_SERVER::clearclient(void) {
  _myclientIPAddress = IPAddress(0, 0, 0, 0);
  sprintf(_clientaddress, "%s%c", _myclientIPAddress.toString().c_str(), 0x00);
  _clientstatus = _disconnected;
}


// --------------------------------------------------------------------
// start debug server
// --------------------------------------------------------------------
bool DEBUG_SERVER::start_server() {
  unsigned long port;

  clearclient();
  //Serial.print(T_DEBUGSERVER)=;
  //Serial.println(T_START);
  // check if server already created, if not, create one
  if (_loaded == false) {
    // create the server
    port = ControllerData->get_debugsrvr_port();
    if (port < 1000) {
      port = 9090;
      ControllerData->set_debugsrvr_port(port);
    }
    _mydebugserver = new WiFiServer(port);
  }
  _mydebugserver->begin(port);
  _serverstate = V_RUNNING;
  _loaded = true;
  debugsrvr_status = true;
  //Serial.println(T_RUNNING);
  return _serverstate;
}

// --------------------------------------------------------------------
// stop server
// --------------------------------------------------------------------
void DEBUG_SERVER::stop_server(void) {
  //Serial.print(T_DEBUGSERVER);
  //Serial.println(T_STOP);
  debugsrvr_status = false;
  _serverstate = V_STOPPED;
  _clientstatus = _disconnected;
  clearclient();
  delete _mydebugserver;
  _loaded = false;
}


// --------------------------------------------------------------------
// check if a client has requested connection,
// terminated connection or has sent a request
// MAJOR WARNING: DO NOT ENABLE AND DEBUG MESSAGES IN THIS FUNCTION
// --------------------------------------------------------------------
void DEBUG_SERVER::check_client(void) {
  // avoid a crash
  if (_loaded == false) {
    return;
  }

  // check if any new connections
  WiFiClient newclient = _mydebugserver->available();
  // there is a new tcp/ip client
  if (newclient) {
    // is a client already connected?
    if (_clientstatus != _connected) {
      // no client connected, so use this newclient
      _myclient = new WiFiClient(newclient);
      // get IP of new client
      _myclientIPAddress = newclient.remoteIP();
      sprintf(_clientaddress, "%s%c", _myclientIPAddress.toString().c_str(), 0x00);
      _clientstatus = _connected;
      // newClient will dispose at end of loop()
      newclient.stop();
    } else {
      // already a client, do drop this new client
      newclient.stop();
    }
  }

  // check if debug client is connected
  // check if any data received from debug client
  if (_clientstatus == _connected) {
    // if client is connected
    if (_myclient->connected()) {
      // if client has send request
      while (_myclient->available()) {
        // process request and send client number
        this->processCommand();
      }
    } else {
      // not connected, stop client
      _myclient->stop();
      // release client
      delete _myclient;
      clearclient();
    }
  }
}


// --------------------------------------------------------------------
// HANDLE SEND AND RECEIVE TCP/IP REQUESTS FROM/TO CLIENTS
// --------------------------------------------------------------------

bool DEBUG_SERVER::check_status(void) {
  if (debugsrvr_status) {
    return true;
  }
  if (_loaded) {
    return true;
  }
  if (_serverstate) {
    return true;
  }
  return false;
}


// --------------------------------------------------------------------
// print message to client
// char array
// --------------------------------------------------------------------
void DEBUG_SERVER::print(const char *s) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    Serial.print(s);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->print(s);
      }
    }
  }
}

// --------------------------------------------------------------------
// print message to client
// char
// --------------------------------------------------------------------
void DEBUG_SERVER::print(char c) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    Serial.print(c);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->print(c);
      }
    }
  }
}

// --------------------------------------------------------------------
// print message to client
// int
// --------------------------------------------------------------------
void DEBUG_SERVER::print(int d) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    Serial.print(d);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->print(d);
      }
    }
  }
}

// --------------------------------------------------------------------
// print message to client
// unsigned int
// --------------------------------------------------------------------
void DEBUG_SERVER::print(unsigned int d) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    Serial.print(d);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->print(d);
      }
    }
  }
}

// --------------------------------------------------------------------
// print message to client
// double
// --------------------------------------------------------------------
void DEBUG_SERVER::print(double d) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    Serial.print(d);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->print(d);
      }
    }
  }
}

// --------------------------------------------------------------------
// print message to client
// String
// --------------------------------------------------------------------
void DEBUG_SERVER::print(String s) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    //Serial.println("debug server output = serial");
    Serial.print(s);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->print(s);
      }
    }
  }
}

// --------------------------------------------------------------------
// print message to client
// long
// --------------------------------------------------------------------
void DEBUG_SERVER::print(long d) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    Serial.print(d);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->print(d);
      }
    }
  }
}

// --------------------------------------------------------------------
// print message to client
// unsigned long
// --------------------------------------------------------------------
void DEBUG_SERVER::print(unsigned long d) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    Serial.print(d);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->print(d);
      }
    }
  }
}


// --------------------------------------------------------------------
// print message to client + newline
// char array
// --------------------------------------------------------------------
void DEBUG_SERVER::println(char *s) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    //Serial.println("debug server output = serial");
    Serial.println(s);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->println(s);
      }
    }
  }
}

// --------------------------------------------------------------------
// print message to client + newline
// char
// --------------------------------------------------------------------
void DEBUG_SERVER::println(char c) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    Serial.println(c);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->println(c);
      }
    }
  }
}

// --------------------------------------------------------------------
// print message to client + newline
// int
// --------------------------------------------------------------------
void DEBUG_SERVER::println(int d) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    Serial.println(d);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->println(d);
      }
    }
  }
}

// --------------------------------------------------------------------
// print message to client
// int + newline
// --------------------------------------------------------------------
void DEBUG_SERVER::println(unsigned int d) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    Serial.println(d);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->println(d);
      }
    }
  }
}

// --------------------------------------------------------------------
// print message to client + newline
// double
// --------------------------------------------------------------------
void DEBUG_SERVER::println(double d) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    Serial.println(d);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->println(d);
      }
    }
  }
}

// --------------------------------------------------------------------
// print message to client + newline
// String
// --------------------------------------------------------------------
void DEBUG_SERVER::println(String s) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    Serial.println(s);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->println(s);
      }
    }
  }
}

// --------------------------------------------------------------------
// print message to client + newline
// long
// --------------------------------------------------------------------
void DEBUG_SERVER::println(long d) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    Serial.println(d);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->println(d);
      }
    }
  }
}

// --------------------------------------------------------------------
// print message to client + newline
// unsigned long
// --------------------------------------------------------------------
void DEBUG_SERVER::println(unsigned long d) {
  if (check_status() == false) {
    return;
  }

  if (ControllerData->get_debugsrvr_out() == DEBUGSERVEROUTPUTSERIAL) {
    Serial.println(d);
  } else {
    // if client is still connected
    if (_clientstatus == true) {
      if (_myclient->connected() == true) {
        // send reply
        _myclient->println(d);
      }
    }
  }
}

// --------------------------------------------------------------------
// processCommand
// reply to any client request with "connected"
// --------------------------------------------------------------------
void DEBUG_SERVER::processCommand() {
  // read
  static char msg[] = { "Connected " };

  while (_myclient->available()) {
    _myclient->read();
  }

  println(msg);
}
