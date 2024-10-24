// ----------------------------------------------------------------------
// myFP2ESP32 DUCKDNS CLASS
// Copyright Robert Brown 2014-2023. All Rights Reserved.
// duckdns.cpp
// Optional
// ----------------------------------------------------------------------


// ----------------------------------------------------------------------
// EXTERNALS
// ----------------------------------------------------------------------

#include <Arduino.h>
#include "controller_config.h"

#if defined(ENABLE_DUCKDNS)


// ----------------------------------------------------------------------
// CLASSES
// ----------------------------------------------------------------------
#include "controller_data.h"
extern CONTROLLER_DATA *ControllerData;


// ----------------------------------------------------------------------
// EXTERNS
// ----------------------------------------------------------------------
extern char duckdnsdomain[];
extern char duckdnstoken[];


// ----------------------------------------------------------------------
// INCLUDES
// ----------------------------------------------------------------------
// https://github.com/ayushsharma82/EasyDDNS
#include <EasyDDNS.h>
#include "duckdns.h"


// ----------------------------------------------------------------------
// DUCKDNS Class
// THERE IS ONLY STATUS SHOWN ON SERVER PAGE /admin1
// DUCKDNS CANNOT BE STARTED/STOPPED
// REFRESH IS HANDLED BY THE CODE - JUST SET THE REFRESH VALUE
// ----------------------------------------------------------------------
DUCK_DNS::DUCK_DNS() {
  this->_loaded = false;
}


// ----------------------------------------------------------------------
// start duckdns
// ----------------------------------------------------------------------
bool DUCK_DNS::start(void) {
  if (this->_loaded == V_RUNNING) {
    return true;
  }

  if (ControllerData->get_duckdns_enable() != V_ENABLED) {
    return false;
  } else {
    // Set DDNS Service Nameto "duckdns"
    EasyDDNS.service("duckdns");

    String domain = ControllerData->get_duckdns_domain();
    int domainlen = domain.length() + 1;
    String token = ControllerData->get_duckdns_token();
    int tokenlen = token.length() + 1;
    char ddd[domainlen];
    char ddt[tokenlen];
    domain.toCharArray(ddd, domainlen);
    token.toCharArray(ddt, tokenlen);
    // Enter ddns Domain & Token | Example - "esp.duckdns.org","1234567"
    EasyDDNS.client(ddd, ddt);
    EasyDDNS.update(ControllerData->get_duckdns_refreshtime());

    this->_loaded = true;
    return true;
  }
}

// ----------------------------------------------------------------------
// set duckdns refresh time
// ----------------------------------------------------------------------
void DUCK_DNS::set_refresh(unsigned int ti) {
  // save new refresh value
  if (ti != ControllerData->get_duckdns_refreshtime()) {
    ControllerData->set_duckdns_refreshtime(ti);
  }

  // apply new refresh time if duckdns is running
  if (this->_loaded == V_RUNNING) {
    EasyDDNS.update(ti);
  }
}

#endif  // #ifdef ENABLE_DUCKDNS
