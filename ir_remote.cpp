// ----------------------------------------------------------------------
// myFP2ESP32 IR-REMOTE CLASS
// Copyright Robert Brown 2014-2023. All Rights Reserved.
// ir_remote.cpp
// Library  myfp2eIRremoteESP8266.zip
// Optional
// ----------------------------------------------------------------------

// ----------------------------------------------------------------------
// EXTERNALS
// ----------------------------------------------------------------------

#include <Arduino.h>
#include "controller_config.h"

#if defined(ENABLE_INFRAREDREMOTE)
#include "ir_remote.h"
//extern IR_REMOTE *irremote;


// ----------------------------------------------------------------------
// CLASSES
// ----------------------------------------------------------------------
#include "controller_data.h"
extern CONTROLLER_DATA *ControllerData;

#include "driver_board.h"
extern DRIVER_BOARD *driverboard;


// ----------------------------------------------------------------------
// EXTERNS
// ----------------------------------------------------------------------
extern volatile bool halt_alert;
extern portMUX_TYPE halt_alertMux;
extern long ftargetPosition;
extern bool isMoving;
extern bool irremote_status;

// ----------------------------------------------------------------------
// INFRARED REMOTE CONTROLLER - CHANGE AT YOUR OWN PERIL
// ----------------------------------------------------------------------
// keep interrupt out of class definition
// use cut down version to save space
#include <myfp2eIRremoteESP8266.h>
#include <myfp2eIRrecv.h>
#include <myfp2eIRutils.h>
#include "ir_remotemappings.h"
int RECV_PIN = 15;
IRrecv *irrecv;
decode_results results;


// ----------------------------------------------------------------------
// NOTES
// By default, the IRRemote uses timer3
// ----------------------------------------------------------------------
// Can be changed by specifing the timer to use in the call IRrecv()
// IRrecv(const uint16_t recvpin, const uint16_t bufsize, const uint8_t timeout,
// const bool save_buffer, const uint8_t timer_num)
// ----------------------------------------------------------------------


// ----------------------------------------------------------------------
// IRREMOTE Class
// ----------------------------------------------------------------------
IR_REMOTE::IR_REMOTE() {
  _loaded = false;
  _pin = -1;
}

bool IR_REMOTE::start(void) {
  if (_loaded == V_RUNNING) {
    return true;
  }

  _pin = ControllerData->get_brdirpin();
  if (_pin != -1) {
    irrecv = new IRrecv(_pin);
    // Start IR
    irrecv->enableIRIn();
    _loaded = true;
    irremote_status = true;
    return true;
  } else {
    _loaded = false;
    return false;
  }
}

void IR_REMOTE::update() {
  if (_loaded == V_RUNNING) {
    // check IR
    if (irrecv->decode(&results)) {
      int adjpos = 0;
      static long lastcode;
      if (results.value == KEY_REPEAT) {
        results.value = lastcode;  // repeat last code
      } else {
        lastcode = results.value;
      }
      if ((isMoving == 1) && (lastcode == IR_HALT)) {
        portENTER_CRITICAL(&halt_alertMux);
        halt_alert = true;
        portEXIT_CRITICAL(&halt_alertMux);
      } else {
        switch (lastcode) {
          case IR_SLOW:
            ControllerData->set_motorspeed(SLOW);
            break;
          case IR_MEDIUM:
            ControllerData->set_motorspeed(MED);
            break;
          case IR_FAST:
            ControllerData->set_motorspeed(FAST);
            break;
          case IR_IN1:
            adjpos = -1;
            break;
          case IR_OUT1:
            adjpos = 1;
            break;
          case IR_IN10:
            adjpos = -10;
            break;
          case IR_OUT10:
            adjpos = 10;
            break;
          case IR_IN50:
            adjpos = -50;
            break;
          case IR_OUT50:
            adjpos = 50;
            break;
          case IR_IN100:
            adjpos = -100;
            break;
          case IR_OUT100:
            adjpos = 100;
            break;
          case IR_SETPOSZERO:  // 0 RESET POSITION TO 0
            adjpos = 0;
            ftargetPosition = 0;
            driverboard->setposition(0);
            ControllerData->set_fposition(0);
            break;
          case IR_PRESET0:
            ftargetPosition = ControllerData->get_focuserpreset(0);
            break;
          case IR_PRESET1:
            ftargetPosition = ControllerData->get_focuserpreset(1);
            break;
          case IR_PRESET2:
            ftargetPosition = ControllerData->get_focuserpreset(2);
            break;
          case IR_PRESET3:
            ftargetPosition = ControllerData->get_focuserpreset(3);
            break;
          case IR_PRESET4:
            ftargetPosition = ControllerData->get_focuserpreset(4);
            break;
        }  // switch(lastcode)
      }    // if ( (isMoving == 1) && (lastcode == IR_HALT))

      // Receive the next value
      irrecv->resume();
      long newpos;
      if (adjpos < 0) {
        newpos = ControllerData->get_fposition() + adjpos;
        newpos = (newpos < 0) ? 0 : newpos;
        ftargetPosition = newpos;
      } else if (adjpos > 0) {
        newpos = ControllerData->get_fposition() + adjpos;
        newpos = (newpos > ControllerData->get_maxstep()) ? ControllerData->get_maxstep() : newpos;
        ftargetPosition = newpos;
      }
    }
  }
}

#endif  // #ifdef ENABLE_INFRAREDREMOTE
