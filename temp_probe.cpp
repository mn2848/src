// ----------------------------------------------------------------------
// myFP2ESP32 TEMPERATURE PROBE CLASS
// Copyright Robert Brown 2014-2023. All Rights Reserved.
// Copyright Holger Manz, 2020-2021. All Rights Reserved.
// temp_probe.cpp
// Default Configuration
// Temperature Probe, Resolution, Handling of temperature compensation
// ----------------------------------------------------------------------


// ----------------------------------------------------------------------
// INCLUDES
// ----------------------------------------------------------------------
#include <Arduino.h>
#include "controller_defines.h"


// ----------------------------------------------------------------------
// INCLUDES
// ----------------------------------------------------------------------
#include "controller_data.h"
extern CONTROLLER_DATA *ControllerData;

// https://github.com/PaulStoffregen/OneWire
#include <OneWire.h>
#include <myDallasTemperature.h>

#include "temp_probe.h"


// ----------------------------------------------------------------------
// EXTERNALS
// ----------------------------------------------------------------------
extern unsigned long ftargetPosition;


// ----------------------------------------------------------------------
// DEFINES
// ----------------------------------------------------------------------
// default temperature value in C
#define V_DEFAULTTEMP 20.0
// temperature compensation DO NOT CHANGE
#define TEMP_FALLING 1
#define TEMP_RISING 0


// ----------------------------------------------------------------------
// CLASS
// Pass ControllerData->get_brdtemppin() as the pin number
// ----------------------------------------------------------------------
TEMP_PROBE::TEMP_PROBE(int pin)
  : _pin(pin) {
  //_oneWire = new OneWire(pin);
  //_tpsensor = new DallasTemperature(_oneWire);
  //_oneWire = new OneWire(pin);
  //_tpsensor(&_oneWire);
  _loaded = false;
  _found = false;
  _lasttemp = V_DEFAULTTEMP;
  _state = false;
}


// ----------------------------------------------------------------------
// bool init(void);
// Init the temp probe, return false=error, true=ok
// ----------------------------------------------------------------------
bool TEMP_PROBE::init() {
  debug_server_print(T_TEMPPROBE);
  debug_server_println(T_INIT);

  // check if valid pin is defined for board
  if (ControllerData->get_brdtemppin() == -1) {
    debug_server_println(T_NOTSUPPORTED);
    ControllerData->set_tcavailable(V_NOTENABLED);
    return false;
  }

  _tpOneWire = new OneWire();
  _tpOneWire->begin(_pin);
  _tpsensor = new DallasTemperature(_tpOneWire);
  _tpsensor->begin();

  if (_tpsensor->getDeviceCount() != 0) {
    // get the probe address ID so we can set the probe resolution
    if (_tpsensor->getAddress(_tpAddress, 0) == true) {
      debug_server_println(T_FOUND);
      _found = true;
      return true;
    }
    debug_server_println(T_NOTFOUND);
    // no probe was found
    return false;
 }
}


// ----------------------------------------------------------------------
// start the temp probe
// bool start(void);
// ----------------------------------------------------------------------
bool TEMP_PROBE::start() {
  debug_server_print(T_TEMPPROBE);
  debug_server_println(T_START);

  // enabled?
  if (ControllerData->get_tempprobe_enable() == V_NOTENABLED) {
    debug_server_println(T_NOTENABLED);
    _loaded = false;
    _lasttemp = V_DEFAULTTEMP;
    _state = false;
    ControllerData->set_tcavailable(V_NOTENABLED);
    return false;
  }

  // search for a sensor
  if (_tpsensor->getDeviceCount() != 0) {
    // get the address so we can set the probe resolution
    if (_tpsensor->getAddress(_tpAddress, 0) == true) {
      debug_server_println(T_FOUND);
      _found = true;
      // set probe resolution
      _tpsensor->setResolution(_tpAddress, ControllerData->get_tempresolution());
      // request the sensor to begin a temperature reading
      _tpsensor->requestTemperatures();
      _found = true;
      _loaded = true;
      _state = true;
      ControllerData->set_tcavailable(V_ENABLED);
      debug_server_println(T_RUNNING);
      return true;
    } else {
      // could not get sensor address info
      debug_server_println("TP-err getAddress()");
      _loaded = false;
      _state = false;
      ControllerData->set_tcavailable(V_ENABLED);
    }
  } else {
    // sensor not found
    debug_server_println(T_NOTFOUND);
    _loaded = false;
    _state = false;
    ControllerData->set_tcavailable(V_NOTENABLED);
  }
  return false;
}

// ----------------------------------------------------------------------
// Stop probe
// ----------------------------------------------------------------------
void TEMP_PROBE::stop() {
  debug_server_print(T_TEMPPROBE);
  debug_server_println(T_STOP);
  _state = false;
  _loaded = false;
}

// ----------------------------------------------------------------------
// Get state
// ----------------------------------------------------------------------
bool TEMP_PROBE::get_state(void) {
  return _state;
}

// ----------------------------------------------------------------------
// Get loaded
// ----------------------------------------------------------------------
bool TEMP_PROBE::get_loaded(void) {
  return _loaded;
}

// ----------------------------------------------------------------------
// Return temp probe found
// ----------------------------------------------------------------------
bool TEMP_PROBE::get_found() {
  return _found;
}

// ----------------------------------------------------------------------
// Start a temperature reading, can take up to 700mS, but we do an async read
// ----------------------------------------------------------------------
void TEMP_PROBE::request() {
  if (_loaded == false) {
    return;
  }
  _tpsensor->requestTemperatures();
}

// ----------------------------------------------------------------------
// Set probe resolution
// ----------------------------------------------------------------------
void TEMP_PROBE::set_resolution(byte tpr) {
  if (_loaded == true) {
    _tpsensor->setResolution(_tpAddress, tpr);
  }
}

// ----------------------------------------------------------------------
// read temp probe value
// ----------------------------------------------------------------------
float TEMP_PROBE::read(void) {
  if (_loaded == false) {
    return _lasttemp;
  }

  // get temperature, always in celsius
  float result = _tpsensor->getTempCByIndex(0);
  // avoid erronous readings
  if (result > -40.0 && result < 80.0) {
    // valid, save in _lasttemp
    _lasttemp = result;
  } else {
    // invalid, use the last valid temp reading
    result = _lasttemp;
  }
  return result;
}

// ----------------------------------------------------------------------
// update_temp probe
// read temperature
// check for temperature compensation and if so, apply tc rules
// ----------------------------------------------------------------------
float TEMP_PROBE::update(void) {
  debug_server_print(T_TEMPPROBE);
  debug_server_println("update");
  if (_loaded == false) {
    debug_server_println(T_NOTENABLED);
    return _lasttemp;
  }
  // track tempcompenabled changes
  static byte tcchanged = ControllerData->get_tempcomp_enable();
  // start with a temp request
  static byte requesttempflag = 0;
  static float tempval;
  // start temperature to use when temperature compensation is enabled
  static float starttemp;

  if (tcchanged != ControllerData->get_tempcomp_enable()) {
    tcchanged = ControllerData->get_tempcomp_enable();
    if (tcchanged == 1) {
      starttemp = read();
    }
  }

  if (requesttempflag) {
    tempval = read();
  } else {
    _tpsensor->requestTemperatures();
  }

  // toggle flag
  requesttempflag ^= 1;

  // check for temperature compensation
  if (ControllerData->get_tempcomp_enable() == V_ENABLED) {
    // calculate if temp has moved by more than 1 degree
    if ((abs)(starttemp - tempval) >= 1) {
      unsigned long newPos;
      // temperature falling (1) or rising (0)?
      byte temperaturedirection;
      temperaturedirection = (tempval < starttemp) ? TEMP_FALLING : TEMP_RISING;

      // check if tc direction for compensation is inwards
      if (ControllerData->get_tcdirection() == TC_DIRECTION_IN) {
        // temperature compensation direction is inwards, if temperature falling then move in else move out

        if (temperaturedirection == TEMP_FALLING) {
          // then move inwards
          newPos = ftargetPosition - ControllerData->get_tempcoefficient();
        } else {
          // else move outwards
          newPos = ftargetPosition + ControllerData->get_tempcoefficient();
        }
      } else {
        // temperature compensation direction is out, if a fall then move out else move in
        if (temperaturedirection == TEMP_FALLING) {
          newPos = ftargetPosition + ControllerData->get_tempcoefficient();
        } else {
          newPos = ftargetPosition - ControllerData->get_tempcoefficient();
        }  // if ( temperaturedirection == 1 )
      }    // if (ControllerData->get_tcdirection() == 0)
      newPos = (newPos > ControllerData->get_maxstep()) ? ControllerData->get_maxstep() : newPos;
      // newPos should be checked for < 0 but cannot due to unsigned
      // newPos = (newPos < 0 ) ? 0 : newPos;
      ftargetPosition = newPos;
      // save this current temp point for future reference
      starttemp = tempval;
    }  // end of check for tempchange >=1
  }    // end of check for tempcomp enabled
  return tempval;
}
