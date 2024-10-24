
// ----------------------------------------------------------------------
// setstepmode()
// ----------------------------------------------------------------------
// Basic rule for setting stepmode
// Set DRIVER_BOARD->setstepmode(xx);                         // this sets the physical pins
// and this also saves ControllerData->set_brdstepmode(xx);   // this saves config setting

// setstallguardvalue()
// Write
// to change a stall-guard value, code must call DRIVER_BOARD->setstallguard(sgval)
// which also updates ControllerData->set_stallguard_value(smgval);
// Read
// code calls ControllerData->get_stallguard_value();

// settmc2209current()
// Write
// to change tmc2209 current mA value, code must call DRIVER_BOARD->settmc2209current(val)
// which also updates ControllerData->set_tmc2209current(val);
// Read
// code calls ControllerData->get_tmc2209current();

// settmc2225current()
// Write
// to change tmc2225 current mA value, code must call DRIVER_BOARD->settmc2225current(val)
// which also updates ControllerData->set_tmc2225current(val);
// Read
// code calls ControllerData->get_tmc2225current();


// ----------------------------------------------------------------------
// NOTE
// ----------------------------------------------------------------------
// Consider this
//     BOARD_println("DrvBrd:start: INOUT LEDs enabled");
// The text is only ever included IF the BOARD_println is defined (default is NOT defined)
// This reduces program size
// Case 1: BOARD_print is defined
// The text is placed into program memory space by the compiler, it does not require (F(text))
// Case 2: Convert text to char *
// If the text is made a char *, then it is included every single time (program size increases)
// Conclusion: As the BOARD_println() is normally undefined then it is best to leave it as is


// ----------------------------------------------------------------------
// Includes
// ----------------------------------------------------------------------
#include <Arduino.h>
#include "controller_config.h"


// DEFAULT CONFIGURATION
// Driverboard
// In-Out LEDs
// Pushbuttons
// Joystick1 and Joystick2
// Home Position Switch
// Step Mode


// ----------------------------------------------------------------------
// CLASSES
// ----------------------------------------------------------------------
#include "controller_data.h"
extern CONTROLLER_DATA *ControllerData;

// import defines for stall guard and current settings for TMC driver boards
#include "defines/tmcstepper_defines.h"

// DRIVER BOARD
#include "driver_board.h"
extern DRIVER_BOARD *driverboard;


// ----------------------------------------------------------------------
// Externs
// ----------------------------------------------------------------------
extern volatile bool timerSemaphore;
// number of steps to move in timer interrupt service routine
extern volatile uint32_t stepcount;
extern portMUX_TYPE timerSemaphoreMux;
extern portMUX_TYPE stepcountMux;
// flag indicator for file access, rather than use SPIFFS.begin() test
extern bool filesystemloaded;
extern long ftargetPosition;


// ----------------------------------------------------------------------
// JOYSTICK DEFINITIONS
// ----------------------------------------------------------------------
// joystick pin definitions come from the Board config

// other joystick settings
#define JZEROPOINT 1837  // value read when joystick is centered
#define JTHRESHOLD 300   // margin of error around center position
#define JMAXVALUE 4095   // maximum value reading of joystick
#define JMINVALUE 0      // minimum value reading of joystick


// ----------------------------------------------------------------------
// DEFINES
// ----------------------------------------------------------------------


// ----------------------------------------------------------------------
// DATA
// ----------------------------------------------------------------------
// shared between interrupt handler and driverboard class
// direction of steps to move
bool stepdir;

// ----------------------------------------------------------------------
// Timer Interrupt
// ----------------------------------------------------------------------
// so we can get CPU frequency
#include "esp32-hal-cpu.h"
// use a unique name for the timer
hw_timer_t *movetimer = NULL;

/*
  if (stepcount  && !(driverboard->hpsw_alert() && stepdir == moving_in))

  stepcount   stepdir        hpsw_alert     action
  ----------------------------------------------------
    0           x             x             stop
    >0          moving_out    x             step
    >0          moving_in     False         step
    >0          moving_in     True          stop
*/

inline void asm2uS() __attribute__((always_inline));

// On esp32 with 240mHz clock a nop takes ? 1/240000000 second or 0.000000004166 of a second
// To get to 2us for ESP32 we will need 240 nop instructions
// 1/3uS on ESP32
inline void asm1uS() {
  asm volatile(
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t"
    "nop \n\t" ::);
}

// ----------------------------------------------------------------------
// timer ISR  Interrupt Service Routine
// STEP MOTOR
// ----------------------------------------------------------------------
void IRAM_ATTR onTimer() {
  static bool mjob = false;  // motor job is running or not

  // if steps > 0, AND if no hpsw alert, AND if direction = move in
  // then step motor
  if (stepcount && !(driverboard->hpsw_alert() && stepdir == moving_in)) {
    driverboard->movemotor(stepdir, true);
    portENTER_CRITICAL(&stepcountMux);
    stepcount--;
    portEXIT_CRITICAL(&stepcountMux);
    // job is running
    mjob = true;
  } else {
    // stepcount = 0, OR hpsw alert, OR
    if (mjob == true) {
      // just in case hps_alert was fired up
      portENTER_CRITICAL(&stepcountMux);
      stepcount = 0;
      portEXIT_CRITICAL(&stepcountMux);
      // wait, and do nothing
      mjob = false;
      portENTER_CRITICAL(&timerSemaphoreMux);
      timerSemaphore = true;
      portEXIT_CRITICAL(&timerSemaphoreMux);
    }
  }
}

// ----------------------------------------------------------------------
// JOYSTICK2
// Keyes KY-023 PS2 style 2-Axis Joystick
// ----------------------------------------------------------------------
// This must be outside of class
void IRAM_ATTR joystick2sw_isr() {
  // The pin is an input, with the internal pullup holding the pin state high.
  // When the switch is pressed, the pin is taken to ground (hence falling).
  // The pin is monitored by an interrupt which looks for a falling pulse.
  // When a fall is detected, the ISR sets the switch state by using the
  // pointer to the driverboard class
  // NOTE: true means switch has been pressed (and not the logic state of the pin)
  driverboard->set_joystick2_swstate(true);
}

// ----------------------------------------------------------------------
// DRIVER_BOARD CLASS
// ----------------------------------------------------------------------
DRIVER_BOARD::DRIVER_BOARD() {
}

// ----------------------------------------------------------------------
// start
// ----------------------------------------------------------------------
void DRIVER_BOARD::start(long startposition) {
  debug_server_print(T_DRIVERBOARD);
  debug_server_println(T_START);
  do {
    // returns the CPU frequency in MHz as an unsigned 8-bit integer
    _clock_frequency = ESP.getCpuFreqMHz();

    // make sure timersemaphore is false when DRIVER_BOARD created
    portENTER_CRITICAL(&timerSemaphoreMux);
    timerSemaphore = false;
    portEXIT_CRITICAL(&timerSemaphoreMux);
    // make sure stepcount is 0 when DRIVER_BOARD created
    portENTER_CRITICAL(&stepcountMux);
    stepcount = 0;
    portEXIT_CRITICAL(&stepcountMux);

    // get board number and cache it locally here
    this->_boardnum = ControllerData->get_brdnumber();

#if (DRVBRD == PRO2ESP32R3WEMOS) || (DRVBRD == PRO2ESP32DRV8825)
    // setup board
    if (this->_boardnum == PRO2ESP32R3WEMOS) {
      pinMode(ControllerData->get_brdenablepin(), OUTPUT);
      pinMode(ControllerData->get_brddirpin(), OUTPUT);
      pinMode(ControllerData->get_brdsteppin(), OUTPUT);
      digitalWrite(ControllerData->get_brdenablepin(), 1);
      // fixed step mode
    } else if (this->_boardnum == PRO2ESP32DRV8825) {
      pinMode(ControllerData->get_brdenablepin(), OUTPUT);
      pinMode(ControllerData->get_brddirpin(), OUTPUT);
      pinMode(ControllerData->get_brdsteppin(), OUTPUT);
      digitalWrite(ControllerData->get_brdenablepin(), 1);
      digitalWrite(ControllerData->get_brdsteppin(), 0);
      pinMode(ControllerData->get_brdboardpins(0), OUTPUT);
      pinMode(ControllerData->get_brdboardpins(1), OUTPUT);
      pinMode(ControllerData->get_brdboardpins(2), OUTPUT);
      // restore step mode
      setstepmode(ControllerData->get_brdstepmode());
    }

#elif (DRVBRD == PRO2ESP32ULN2003) || (DRVBRD == PRO2ESP32L298N) || (DRVBRD == PRO2ESP32L293DMINI) \
  || (DRVBRD == PRO2ESP32L9110S)
    if (this->_boardnum == PRO2ESP32ULN2003) {
      // IN1, IN2, IN3, IN4
      this->_inputpins[0] = ControllerData->get_brdboardpins(0);
      this->_inputpins[1] = ControllerData->get_brdboardpins(1);
      this->_inputpins[2] = ControllerData->get_brdboardpins(2);
      this->_inputpins[3] = ControllerData->get_brdboardpins(3);
      for (int i = 0; i < 4; i++) {
        pinMode(this->_inputpins[i], OUTPUT);
      }
      myhstepper = new HalfStepper(ControllerData->get_brdstepsperrev(), this->_inputpins[0], this->_inputpins[1], this->_inputpins[2], this->_inputpins[3]);  // ok
      // restore step mode
      setstepmode(ControllerData->get_brdstepmode());
    } else if (this->_boardnum == PRO2ESP32L298N) {
      // IN1, IN2, IN3, IN4
      this->_inputpins[0] = ControllerData->get_brdboardpins(0);
      this->_inputpins[1] = ControllerData->get_brdboardpins(1);
      this->_inputpins[2] = ControllerData->get_brdboardpins(2);
      this->_inputpins[3] = ControllerData->get_brdboardpins(3);
      for (int i = 0; i < 4; i++) {
        pinMode(this->_inputpins[i], OUTPUT);
      }
      myhstepper = new HalfStepper(ControllerData->get_brdstepsperrev(), this->_inputpins[0], this->_inputpins[1], this->_inputpins[2], this->_inputpins[3]);  // ok
      // restore step mode
      setstepmode(ControllerData->get_brdstepmode());
    } else if (this->_boardnum == PRO2ESP32L293DMINI) {
      // IN1, IN2, IN3, IN4
      this->_inputpins[0] = ControllerData->get_brdboardpins(0);
      this->_inputpins[1] = ControllerData->get_brdboardpins(1);
      this->_inputpins[2] = ControllerData->get_brdboardpins(2);
      this->_inputpins[3] = ControllerData->get_brdboardpins(3);
      for (int i = 0; i < 4; i++) {
        pinMode(this->_inputpins[i], OUTPUT);
      }
      pinMode(ControllerData->get_brdenablepin(), OUTPUT); //MN use of enablepin with L293D
      myhstepper = new HalfStepper(ControllerData->get_brdstepsperrev(), this->_inputpins[0], this->_inputpins[1], this->_inputpins[2], this->_inputpins[3]);  // ok
      // restore step mode
      setstepmode(ControllerData->get_brdstepmode());
    } else if (this->_boardnum == PRO2ESP32L9110S) {
      // IN1, IN2, IN3, IN4
      this->_inputpins[0] = ControllerData->get_brdboardpins(0);
      this->_inputpins[1] = ControllerData->get_brdboardpins(1);
      this->_inputpins[2] = ControllerData->get_brdboardpins(2);
      this->_inputpins[3] = ControllerData->get_brdboardpins(3);
      for (int i = 0; i < 4; i++) {
        pinMode(this->_inputpins[i], OUTPUT);
      }
      myhstepper = new HalfStepper(ControllerData->get_brdstepsperrev(), this->_inputpins[0], this->_inputpins[1], this->_inputpins[2], this->_inputpins[3]);  // ok
      // restore step mode
      setstepmode(ControllerData->get_brdstepmode());
    }

#elif (DRVBRD == PRO2ESP32TMC2225) || (DRVBRD == PRO2ESP32TMC2209) || (DRVBRD == PRO2ESP32TMC2209P) \
  || (DRVBRD == PRO2ESP32LOLINS2MINI)
    if (this->_boardnum == PRO2ESP32TMC2225) {
      // init tmc2225
      pinMode(ControllerData->get_brdenablepin(), OUTPUT);
      pinMode(ControllerData->get_brddirpin(), OUTPUT);
      pinMode(ControllerData->get_brdsteppin(), OUTPUT);
      // high disables the driver chip
      digitalWrite(ControllerData->get_brdenablepin(), 1);
      digitalWrite(ControllerData->get_brdsteppin(), 0);
      // ms1
      pinMode(ControllerData->get_brdboardpins(0), OUTPUT);
      // ms2
      pinMode(ControllerData->get_brdboardpins(1), OUTPUT);
      // set step mode handled by init_tmc2225
      init_tmc2225();
    } else if (this->_boardnum == PRO2ESP32TMC2209 || this->_boardnum == PRO2ESP32TMC2209P) {
      // init tmc2209
      pinMode(ControllerData->get_brdenablepin(), OUTPUT);
      pinMode(ControllerData->get_brddirpin(), OUTPUT);
      pinMode(ControllerData->get_brdsteppin(), OUTPUT);
      // high disables the driver chip
      digitalWrite(ControllerData->get_brdenablepin(), 1);
      digitalWrite(ControllerData->get_brdsteppin(), 0);
      // ms1
      pinMode(ControllerData->get_brdboardpins(0), OUTPUT);
      // ms2
      pinMode(ControllerData->get_brdboardpins(1), OUTPUT);
      // set step mode handled by init_tmc2209()
      init_tmc2209();
    } else if (this->_boardnum == PRO2ESP32LOLINS2MINI) {
      pinMode(ControllerData->get_brdenablepin(), OUTPUT);
      pinMode(ControllerData->get_brddirpin(), OUTPUT);
      pinMode(ControllerData->get_brdsteppin(), OUTPUT);
      digitalWrite(ControllerData->get_brdenablepin(), 1);
      digitalWrite(ControllerData->get_brdsteppin(), 0);
      // restore step mode
      setstepmode(ControllerData->get_brdstepmode());
    }
#endif

  } while (0);

  // For all boards do the following
  // set default focuser position to same as mySetupData
  this->_focuserposition = startposition;
  // initialize home position switch if present
  if (init_hpsw() == true) {
    debug_server_println(db2);
  }

  // In Out LEDS
  if (set_leds(ControllerData->get_inoutled_enable()) == true) {
    debug_server_println(db3);
  }

  // only 1 can be in an enabled state,
  if (set_pushbuttons(ControllerData->get_pushbutton_enable()) == true) {
    debug_server_println(db4);
  } else if (set_joystick1(ControllerData->get_joystick1_enable()) == true) {
    debug_server_println(db5);
  } else if (set_joystick2(ControllerData->get_joystick2_enable()) == true) {
    debug_server_println(db6);
  }
}

// ----------------------------------------------------------------------
// destructor
// ----------------------------------------------------------------------
DRIVER_BOARD::~DRIVER_BOARD() {
#if (DRVBRD == PRO2ESP32ULN2003) || (DRVBRD == PRO2ESP32L298N) || (DRVBRD == PRO2ESP32L293DMINI) \
  || (DRVBRD == PRO2ESP32L9110S)
  delete myhstepper;
#endif
}


// ----------------------------------------------------------------------
// INITIALISE HOME POSITION SWITCH
// if( driverboard->init_hpsw() == true)
// If the BRD HPSW Pin = -1 then HPSW not supported - return false
// If the BRD HPSW Pin != -1, HPSW En = 0; init the HPSWPin
// If the BRD HPSW Pin != -1, HPSW En = 1; init the HPSWPin
// ----------------------------------------------------------------------
bool DRIVER_BOARD::init_hpsw(void) {
  // If the designated pin for an option is -1 then the option is not supported
  // Check if this board supports HPSW option
  debug_server_println(db7);

  if (ControllerData->get_brdhpswpin() == -1) {
    debug_server_println(db8);
    return false;
  }

  // BRD supports a HPSW, init the HPSW Pin

  // tmc2209 stall guard etc is a special case
#if (DRVBRD == PRO2ESP32TMC2209 || DRVBRD == PRO2ESP32TMC2209P)
  bool state = false;
  // determe what the hpsw setting is, stall guard, physical switch or none
  tmc2209stallguard sgstate = ControllerData->get_stallguard_state();
  switch (sgstate) {
    case Use_Stallguard:
      // StallGuard4 threshold [0... 255] level for stall detection. It compensates for
      // motor specific characteristics and controls sensitivity. A higher value gives a higher
      // sensitivity. A higher value makes StallGuard4 more sensitive and requires less torque to
      // indicate a stall. The double of this value is compared to SG_RESULT.
      // The stall output becomes active if SG_RESULT falls below this value.

      // initialize the pin
      pinMode(ControllerData->get_brdhpswpin(), INPUT_PULLUP);
      mytmcstepper->SGTHRS(ControllerData->get_stallguard_value());
      debug_server_println(db9);
      state = true;
      break;

    case Use_Physical_Switch:
      // if using a physical switch then hpsw in controllerdata must also be enabled

      // initialize the pin
      pinMode(ControllerData->get_brdhpswpin(), INPUT_PULLUP);
      mytmcstepper->SGTHRS(0);
      debug_server_println(db10);
      state = true;
      break;

    case Use_None:
      mytmcstepper->SGTHRS(0);
      debug_server_println(db11);
      state = true;
      break;
  }
  return state;
#endif

  // for all other boards
  pinMode(ControllerData->get_brdhpswpin(), INPUT_PULLUP);
  return true;
}

// ----------------------------------------------------------------------
// INITIALISE TMC2209 STEPPER DRIVER
// driverboard->init_tmc2209()
// ----------------------------------------------------------------------
void DRIVER_BOARD::init_tmc2209(void) {
  debug_server_print(db12);
  // provide protection around mytmcstepper code
#if (DRVBRD == PRO2ESP32TMC2209 || DRVBRD == PRO2ESP32TMC2209P)
  // Specify the serial2 interface to the tmc
  mytmcstepper = new TMC2209Stepper(&SERIAL_PORT2, R_SENSE, DRIVER_ADDRESS);
  Serial2.begin(TMC2209SPEED);
  mytmcstepper->begin();
  mytmcstepper->pdn_disable(1);       // Use PDN/UART pin for communication
  mytmcstepper->mstep_reg_select(1);  // Adjust stepMode from the registers
  mytmcstepper->I_scale_analog(0);    // Adjust current from the registers
  mytmcstepper->toff(TOFF_VALUE);     // use TMC22xx Calculations sheet to get these
  mytmcstepper->tbl(2);
  // set driver current mA
  mytmcstepper->rms_current(ControllerData->get_tmc2209current());
  // step mode set according to ControllerData->get_brdstepmode()
  int sm = ControllerData->get_brdstepmode();
  // handle full steps
  sm = (sm == STEP1) ? 0 : sm;
  debug_server_print(db13);
  debug_server_println(sm);
  mytmcstepper->microsteps(sm);

  // stall guard settings
  mytmcstepper->semin(0);
  // lower threshold velocity for switching on smart energy CoolStep and StallGuard to DIAG output
  // 20bit max
  mytmcstepper->TCOOLTHRS(0xFFFFF);
  // use TMC22xx Calculations sheet to get these
  mytmcstepper->hysteresis_end(0);
  // use TMC22xx Calculations sheet to get these
  mytmcstepper->hysteresis_start(0);
  // setup of stall guard has moved to init_hpsw()
  debug_server_print(db14);
  mytmcstepper->test_connection() == 0 ? debug_server_println(T_OK) : debug_server_println(T_NOTOK);
  debug_server_print(db15);
  digitalRead(ControllerData->get_brdenablepin()) ? debug_server_println(T_DISABLED) : debug_server_println(T_ENABLED);
  debug_server_print(db13);
  debug_server_println(mytmcstepper->microsteps());
#endif  // #if (DRVBRD == PRO2ESP32TMC2209 || DRVBRD == PRO2ESP32TMC2209P)
}

// ----------------------------------------------------------------------
// INITIALISE TMC2225 STEPPER DRIVER
// driverboard->init_tmc2225()
// ----------------------------------------------------------------------
void DRIVER_BOARD::init_tmc2225(void) {
  debug_server_print(db16);
  // provide protection around mytmcstepper code
#if (DRVBRD == PRO2ESP32TMC2225)
  // specify the serial2 interface to the tmc2225
  mytmcstepper = new TMC2208Stepper(&SERIAL_PORT2, R_SENSE);
  Serial2.begin(TMC2225SPEED);
  mytmcstepper->begin();
  // use PDN/UART pin for communication
  mytmcstepper->pdn_disable(1);
  mytmcstepper->mstep_reg_select(true);
  // adjust current from the registers
  mytmcstepper->I_scale_analog(0);
  // set driver current [recommended NEMA = 400mA, set to 300mA]
  mytmcstepper->rms_current(ControllerData->get_tmc2225current());
  // enable driver
  mytmcstepper->toff(2);
  // stepmode set according to ControllerData->get_brdstepmode();
  unsigned short sm = (unsigned short)ControllerData->get_brdstepmode();
  // handle full steps
  sm = (sm == STEP1) ? 0 : sm;
  mytmcstepper->microsteps(sm);
  // step mode = 1/4 - default specified in boardfile.jsn
  mytmcstepper->hysteresis_end(0);
  mytmcstepper->hysteresis_start(0);
  debug_server_print(db14);
  mytmcstepper->test_connection() == 0 ? debug_server_println(T_OK) : debug_server_println(T_NOTOK);
  debug_server_print(db15);
  digitalRead(ControllerData->get_brdenablepin()) ? debug_server_println(T_DISABLED) : debug_server_println(T_ENABLED);
  debug_server_print(db13);
  debug_server_println(mytmcstepper->microsteps());
#endif  // #if (DRVBRD == PRO2ESP32TMC2225)
}

// ----------------------------------------------------------------------
// CHECK HOME POSITION SWITCH
// if( driverboard->hpsw_alert() == true )
// ----------------------------------------------------------------------
// check hpsw state - switch or stall guard for tmc2209 if enabled
// 1: physical switch uses internal pullup, if hpsw is closed = low,
//    so we need to invert state, to return high when switch is closed
//
// 2: tmc2209 stall guard
//    if stall guard high then stall guard detected on DIAG pin, HIGH = activated
//    hps_alert must return high if hpsw or stall guard is activated
//    so return state (no invert needed)
// ----------------------------------------------------------------------
bool DRIVER_BOARD::hpsw_alert(void) {
  // if moving out, return
  if (stepdir == moving_out) {
    return false;
  }

  // if hpsw is not enabled then return false
  if (ControllerData->get_hpswitch_enable() == V_NOTENABLED) {
    return false;
  } else {

    // hpsw is enabled so check it
    debug_server_print(db17);

#if (DRVBRD == PRO2ESP32TMC2209) || (DRVBRD == PRO2ESP32TMC2209P)
    // check tmc2209 boards
    if (ControllerData->get_stallguard_state() == Use_Stallguard) {
      // diag pin = HIGH if stall guard found, we return high if DIAG, low otherwise
      debug_server_println(db18);
      return (bool)digitalRead(ControllerData->get_brdhpswpin());
    } else if (ControllerData->get_stallguard_state() == Use_Physical_Switch) {
      debug_server_println(db19);
      // Physical swith - When closed HPSW returns low, so invert state on return
      return !((bool)digitalRead(ControllerData->get_brdhpswpin()));
    } else {
      // Use None
      debug_server_println(db20);
      return false;
    }
#else
    // for all other boards check the physical HOMEPOSITIONSWITCH
    // switch uses internal pullup, if hpsw is closed = low, so we need to invert state to return high when switch is closed
    return !((bool)digitalRead(ControllerData->get_brdhpswpin()));
#endif
  }
}


// ----------------------------------------------------------------------
// SET LED STATE
// if( driverboard->set_leds(true) == true)
// return true if the led state was set correctly
// ----------------------------------------------------------------------
bool DRIVER_BOARD::set_leds(bool state) {
  debug_server_println(db21);
  if (state == true) {
    // check if already enabled
    if (this->_leds_loaded == V_ENABLED) {
      return true;
    }
    // leds are not enabled, so enable them
    // check if they are permitted for this board
    if ((ControllerData->get_brdinledpin() == -1) || (ControllerData->get_brdoutledpin() == -1)) {
      debug_server_println(db22);
      this->_leds_loaded = V_NOTENABLED;
      ControllerData->set_inoutled_enable(V_NOTENABLED);
      return false;
    }
    ControllerData->set_inoutled_enable(V_ENABLED);
    pinMode(ControllerData->get_brdinledpin(), OUTPUT);
    pinMode(ControllerData->get_brdoutledpin(), OUTPUT);
    this->_leds_loaded = V_ENABLED;
    return true;
  } else {
    // state is false; disable leds
    // no need to check if it is already stopped/notenabled
    this->_leds_loaded = V_NOTENABLED;
    ControllerData->set_inoutled_enable(V_NOTENABLED);
    return true;
  }
  return this->_leds_loaded;
}

// ----------------------------------------------------------------------
// IS LEDS LOADED
// if( driverboard->get_leds_loaded() == true)
// ----------------------------------------------------------------------
bool DRIVER_BOARD::get_leds_loaded(void) {
  return this->_leds_loaded;
}

// ----------------------------------------------------------------------
// SET PUSHBUTTONS STATE
// if( driverboard->set_pushbuttons(bool) == true)
// return true if the pushbuttons state was set correctly
// ----------------------------------------------------------------------
bool DRIVER_BOARD::set_pushbuttons(bool state) {
  debug_server_println(db23);

  // check if they are permitted for this board
  if ((ControllerData->get_brdpb1pin() == -1) || (ControllerData->get_brdpb2pin() == -1)) {
    debug_server_println(db22);
    ControllerData->set_pushbutton_enable(V_NOTENABLED);
    this->_pushbuttons_loaded = V_NOTENABLED;
    return V_NOTENABLED;
  }

  if (state == true) {
    // check if already enabled
    if (this->_pushbuttons_loaded == V_ENABLED) {
      return V_ENABLED;
    }

    // pushbuttons are not enabled
    // check if joystick1 is enabled
    if (this->_joystick1_loaded == V_ENABLED) {
      debug_server_println(db24);
      this->_pushbuttons_loaded = V_NOTENABLED;
      ControllerData->set_pushbutton_enable(V_NOTENABLED);
      return V_NOTENABLED;
    }

    // check if joystick2 is enabled
    if (this->_joystick2_loaded == V_ENABLED) {
      debug_server_println(db25);
      this->_pushbuttons_loaded = V_NOTENABLED;
      ControllerData->set_pushbutton_enable(V_NOTENABLED);
      return V_NOTENABLED;
    }
    // pushbuttons is permitted, enable pushbuttons
    debug_server_println(db26);
    ControllerData->set_pushbutton_enable(V_ENABLED);
    pinMode(ControllerData->get_brdpb2pin(), INPUT);
    pinMode(ControllerData->get_brdpb1pin(), INPUT);
    this->_pushbuttons_loaded = V_ENABLED;
  } else {
    // state = false;
    if (this->_pushbuttons_loaded == V_ENABLED) {
      // interrupts not used
    }

    // no need to check if it is already stopped/notenabled
    ControllerData->set_pushbutton_enable(V_NOTENABLED);
    this->_pushbuttons_loaded = V_NOTENABLED;
    debug_server_println(db27);
  }
  return this->_pushbuttons_loaded;
}

// ----------------------------------------------------------------------
// IS PUSHBUTTONS LOADED
// if( driverboard->get_pushbuttons_loaded() == true)
// ----------------------------------------------------------------------
bool DRIVER_BOARD::get_pushbuttons_loaded(void) {
  return this->_pushbuttons_loaded;
}

// ----------------------------------------------------------------------
// UPDATE PUSHBUTTONS
// driverboard->update_pushbuttons()
// ----------------------------------------------------------------------
void DRIVER_BOARD::update_pushbuttons(void) {
  if (this->_pushbuttons_loaded == true) {
    static long newpos;

    // PB are active high - pins are low by virtue of pull down resistors through J16 and J17 jumpers
    // read from the board pin number, and compare the return pin value - if 1 then button is pressed
    if (digitalRead(ControllerData->get_brdpb1pin()) == 1) {
      newpos = ftargetPosition - ControllerData->get_pushbutton_steps();
      newpos = (newpos < 0) ? 0 : newpos;
      ftargetPosition = newpos;
    }
    if (digitalRead(ControllerData->get_brdpb2pin()) == 1) {
      newpos = ftargetPosition + ControllerData->get_pushbutton_steps();
      // an unsigned long range is 0 to 4,294,967,295
      // when an unsigned long decrements from 0-1 it goes to largest +ve value, ie 4,294,967,295
      // which would in likely be much much greater than maxstep
      newpos = (newpos > ControllerData->get_maxstep()) ? ControllerData->get_maxstep() : newpos;
      ftargetPosition = newpos;
    }
  }
}

// ----------------------------------------------------------------------
// SET JOYSTICK1 STATE
// if( driverboard->set_joystick1() == true)
// ----------------------------------------------------------------------
bool DRIVER_BOARD::set_joystick1(bool state) {
  debug_server_println(db28);

  // check if joystick1 is permitted for this board
  if ((ControllerData->get_brdpb1pin() == -1) || (ControllerData->get_brdpb2pin() == -1)) {
    debug_server_println(db22);
    this->_joystick1_loaded = V_NOTENABLED;
    ControllerData->set_joystick1_enable(V_NOTENABLED);
    return V_NOTENABLED;
  }

  // permitted - check for clash with PB and J2
  // if this is SET TRUE
  if (state == true) {
    // check if already enabled
    if (this->_joystick1_loaded == V_ENABLED) {
      return V_ENABLED;
    }

    // joystick1 is not enabled, check if push buttons is enabled
    if (this->_pushbuttons_loaded == V_ENABLED) {
      debug_server_println(db29);
      this->_joystick1_loaded = V_NOTENABLED;
      ControllerData->set_joystick1_enable(V_NOTENABLED);
      return V_NOTENABLED;
    }

    // check if joystick2 is enabled
    if (this->_joystick2_loaded == V_ENABLED) {
      debug_server_println(db25);
      this->_joystick1_loaded = V_NOTENABLED;
      ControllerData->set_joystick1_enable(V_NOTENABLED);
      return V_NOTENABLED;
    }

    // pb and joy2 are not enabled, so try enable joy1
    // joystick1 is permitted
    // enable joystick1
    debug_server_println(db26);
    // enable joystick1, joystick 1 does not use brdpb2pin
    pinMode(ControllerData->get_brdpb2pin(), INPUT);
    pinMode(ControllerData->get_brdpb1pin(), INPUT);
    ControllerData->set_joystick1_enable(V_ENABLED);
    this->_joystick1_loaded = V_ENABLED;
  } else {
    // state is false
    debug_server_println(db27);
    // disable joystick1
    ControllerData->set_joystick1_enable(V_NOTENABLED);
    this->_joystick1_loaded = V_NOTENABLED;
  }
  return this->_joystick1_loaded;
}

// ----------------------------------------------------------------------
// IS JOYSTICK1 LOADED
// if( driverboard->get_joystick1_loaded() == true)
// ----------------------------------------------------------------------
bool DRIVER_BOARD::get_joystick1_loaded(void) {
  return this->_joystick1_loaded;
}

// ----------------------------------------------------------------------
// UPDATE JOYSTICK1
// driverboard->update_joystick1()
// ----------------------------------------------------------------------
void DRIVER_BOARD::update_joystick1(void) {
  if (this->_joystick1_loaded == true) {
    static int joyval;
    static long newpos;

    debug_server_println(db30);

    joyval = analogRead(ControllerData->get_brdpb1pin());
    debug_server_print(db31);
    debug_server_println(joyval);
    if (joyval < (JZEROPOINT - JTHRESHOLD)) {
      newpos = ftargetPosition - 1;
      newpos = (newpos < 0) ? 0 : newpos;
      ftargetPosition = newpos;
      debug_server_print(db32);
      debug_server_println(joyval);
    } else if (joyval > (JZEROPOINT + JTHRESHOLD)) {
      newpos = ftargetPosition + 1;
      // an unsigned long range is 0 to 4,294,967,295
      // when an unsigned long decrements from 0-1 it goes to largest +ve value, ie 4,294,967,295
      // which would in likely be much much greater than maxstep
      newpos = (newpos > ControllerData->get_maxstep()) ? ControllerData->get_maxstep() : newpos;
      ftargetPosition = newpos;
      debug_server_print(db33);
      debug_server_println(joyval);
    }
  } else {
    debug_server_println(db34);
  }
}

// ----------------------------------------------------------------------
// SET JOYSTICK2 ENABLED STATE
// if( driverboard->set_joystick2() == true)
// ----------------------------------------------------------------------
bool DRIVER_BOARD::set_joystick2(bool state) {
  debug_server_print(db35);
  // check if joystick is permitted for this board
  if ((ControllerData->get_brdpb1pin() == -1) || (ControllerData->get_brdpb2pin() == -1)) {
    debug_server_println(db22);
    this->_joystick2_loaded = V_NOTENABLED;
    ControllerData->set_joystick2_enable(V_NOTENABLED);
  }

  // permitted - check for clash with PB and J1
  // if this is SET TRUE
  if (state == true) {
    // check if already enabled
    if (this->_joystick2_loaded == V_ENABLED) {
      return V_ENABLED;
    }

    // joystick2 is not enabled
    // check if push buttons is enabled
    if (this->_pushbuttons_loaded == V_ENABLED) {
      debug_server_println(db29);
      this->_joystick2_loaded = V_NOTENABLED;
      ControllerData->set_joystick2_enable(V_NOTENABLED);
      return V_NOTENABLED;
    }

    // check if joystick1 is enabled
    if (this->_joystick1_loaded == V_ENABLED) {
      debug_server_println(db24);
      this->_joystick2_loaded = V_NOTENABLED;
      ControllerData->set_joystick2_enable(V_NOTENABLED);
      return V_NOTENABLED;
    }

    // joystick2 is permitted
    // enable joystick2
    debug_server_println(db26);
    ControllerData->set_joystick2_enable(V_ENABLED);
    this->_joystick2_swstate = false;
    pinMode(ControllerData->get_brdpb2pin(), INPUT);
    pinMode(ControllerData->get_brdpb1pin(), INPUT);
    // setup interrupt, falling edge, pin state = HIGH and falls to GND (0) when pressed
    attachInterrupt(ControllerData->get_brdpb2pin(), joystick2sw_isr, FALLING);
    this->_joystick2_loaded = V_ENABLED;
  } else {
    // state is false
    debug_server_println(db27);
    // disable joystick2
    if (this->_joystick2_loaded == V_ENABLED) {
      // detach interrupt for switch
      detachInterrupt(ControllerData->get_brdpb2pin());
    }
    this->_joystick2_swstate = false;
    this->_joystick2_loaded = false;
    ControllerData->set_joystick2_enable(V_NOTENABLED);
  }
  return this->_joystick2_loaded;
}

// ----------------------------------------------------------------------
// IS JOYSTICK2 LOADED
// if( driverboard->get_joystick2_loaded() == true)
// ----------------------------------------------------------------------
bool DRIVER_BOARD::get_joystick2_loaded(void) {
  return this->_joystick2_loaded;
}

// ----------------------------------------------------------------------
// UPDATE JOYSTICK2
// driverboard->update_joystick2()
// ----------------------------------------------------------------------
void DRIVER_BOARD::update_joystick2(void) {
  if (_joystick2_loaded == true) {
    static int joyval;
    static long newpos;

    debug_server_println(db36);

    analogRead(ControllerData->get_brdpb1pin());
    debug_server_print(db31);
    debug_server_println(joyval);
    if (joyval < (JZEROPOINT - JTHRESHOLD)) {
      newpos = ftargetPosition - 1;
      newpos = (newpos < 0) ? 0 : newpos;
      ftargetPosition = newpos;
      debug_server_print(db32);
      debug_server_println(joyval);
    } else if (joyval > (JZEROPOINT + JTHRESHOLD)) {
      newpos = ftargetPosition + 1;
      // an unsigned long range is 0 to 4,294,967,295
      // when an unsigned long decrements from 0-1 it goes to largest +ve value, ie 4,294,967,295
      // which would in likely be much much greater than maxstep
      newpos = (newpos > ControllerData->get_maxstep()) ? ControllerData->get_maxstep() : newpos;
      ftargetPosition = newpos;
      debug_server_print(db33);
      debug_server_println(joyval);
    }

    // handle switch
    if (this->_joystick2_swstate == true) {
      // switch is pressed

      // user defined code here to handle switch pressed down
      // could be a halt
      // could be a home
      // could be a preset
      // insert code here

      // OR

      // use driverboard->get_joystick2_swstate()

      // finally reset joystick switch state to allow more pushes
      this->_joystick2_swstate = false;
    }
  } else {
    // SET False
    // state is false
    debug_server_println(db34);
    // disable joystick2
    if (this->_joystick2_loaded == V_ENABLED) {
      // detach interrupt
      detachInterrupt(ControllerData->get_brdpb2pin());
    }
    this->_joystick2_swstate = false;
    this->_joystick2_loaded = false;
    ControllerData->set_joystick2_enable(V_NOTENABLED);
  }
}

// ----------------------------------------------------------------------
// bool driverboard->get_joystick2_swstate();
// returns true if the switch was pressed
// reading the switch state clears the switch state to false, allowing
// further button presses.
// ----------------------------------------------------------------------
bool DRIVER_BOARD::get_joystick2_swstate(void) {
  bool _state = this->_joystick2_swstate;
  this->_joystick2_swstate = false;
  return _state;
}

// ----------------------------------------------------------------------
// void driverboard->set_joystick2_swstate(bool);
// Used by the ISR routine to set the switch state
// ----------------------------------------------------------------------
void DRIVER_BOARD::set_joystick2_swstate(bool state) {
  this->_joystick2_swstate = state;
  update_joystick2();
}


// ----------------------------------------------------------------------
// Basic rule for setting stepmode in this order
// Set DRIVER_BOARD->setstepmode(xx);
// this sets the physical pins
// driver->setstepmode() also writes to ControllerData->set_brdstepmode(xx);
// ----------------------------------------------------------------------
void DRIVER_BOARD::setstepmode(int smode) {
  do {
#if (DRVBRD == PRO2ESP32R3WEMOS)|| (DRVBRD == PRO2ESP32ST6128)|| (DRVBRD == PRO2ESP32LOLINS2MINI) 
      if ((this->_boardnum == PRO2ESP32R3WEMOS) || (this->_boardnum == PRO2ESP32ST6128) || (this->_boardnum == PRO2ESP32LOLINS2MINI)) {
      // stepmode is set in hardware jumpers, cannot set by software
      // ControllerData->set_brdstepmode(ControllerData->get_brdfixedstepmode());
      // ignore request
    }
#elif (DRVBRD == PRO2ESP32DRV8825)
    switch (smode) {
      case STEP1:
        digitalWrite(ControllerData->get_brdboardpins(0), 0);
        digitalWrite(ControllerData->get_brdboardpins(1), 0);
        digitalWrite(ControllerData->get_brdboardpins(2), 0);
        break;
      case STEP2:
        digitalWrite(ControllerData->get_brdboardpins(0), 1);
        digitalWrite(ControllerData->get_brdboardpins(1), 0);
        digitalWrite(ControllerData->get_brdboardpins(2), 0);
        break;
      case STEP4:
        digitalWrite(ControllerData->get_brdboardpins(0), 0);
        digitalWrite(ControllerData->get_brdboardpins(1), 1);
        digitalWrite(ControllerData->get_brdboardpins(2), 0);
        break;
      case STEP8:
        digitalWrite(ControllerData->get_brdboardpins(0), 1);
        digitalWrite(ControllerData->get_brdboardpins(1), 1);
        digitalWrite(ControllerData->get_brdboardpins(2), 0);
        break;
      case STEP16:
        digitalWrite(ControllerData->get_brdboardpins(0), 0);
        digitalWrite(ControllerData->get_brdboardpins(1), 0);
        digitalWrite(ControllerData->get_brdboardpins(2), 1);
        break;
      case STEP32:
        digitalWrite(ControllerData->get_brdboardpins(0), 1);
        digitalWrite(ControllerData->get_brdboardpins(1), 0);
        digitalWrite(ControllerData->get_brdboardpins(2), 1);
        break;
      default:
        digitalWrite(ControllerData->get_brdboardpins(0), 0);
        digitalWrite(ControllerData->get_brdboardpins(1), 0);
        digitalWrite(ControllerData->get_brdboardpins(2), 0);
        smode = STEP1;
        break;
    }
    // update boardconfig.jsn
    ControllerData->set_brdstepmode(smode);
#elif (DRVBRD == PRO2ESP32ULN2003) || (DRVBRD == PRO2ESP32L298N) || (DRVBRD == PRO2ESP32L293DMINI) \
  || (DRVBRD == PRO2ESP32L9110S)
    switch (smode) {
      case STEP1:
        myhstepper->SetSteppingMode(SteppingMode::FULL);
        break;
      case STEP2:
        myhstepper->SetSteppingMode(SteppingMode::HALF);
        break;
      default:
        smode = STEP1;
        myhstepper->SetSteppingMode(SteppingMode::FULL);
        break;
    }
    // update boardconfig.jsn
    ControllerData->set_brdstepmode(smode);
#elif (DRVBRD == PRO2ESP32TMC2225) || (DRVBRD == PRO2ESP32TMC2209) || (DRVBRD == PRO2ESP32TMC2209P)
    smode = (smode < STEP1) ? STEP1 : smode;
    smode = (smode > STEP256) ? STEP256 : smode;
    ControllerData->set_brdstepmode(smode);
    // handle full stepmode
    smode = (smode == STEP1) ? 0 : smode;
    mytmcstepper->microsteps(smode);
    // controller uses 1 as full step mode
    if (smode == 0) {
      smode = STEP1;
    }
    // update boardconfig.jsn
    ControllerData->set_brdstepmode(smode);
#endif
  } while (0);
}

// ----------------------------------------------------------------------
// ENABLE MOTOR
// driverboard->enablemotor()
// Must be done whenever the motor is required to step
// ----------------------------------------------------------------------
void DRIVER_BOARD::enablemotor(void) {
#if (DRVBRD == PRO2ESP32DRV8825) || (DRVBRD == PRO2ESP32R3WEMOS) || (DRVBRD == PRO2ESP32TMC2225) \
  || (DRVBRD == PRO2ESP32TMC2209) || (DRVBRD == PRO2ESP32TMC2209P) || (DRVBRD == PRO2ESP32ST6128) \
  ||(DRVBRD == PRO2ESP32LOLINS2MINI)
  digitalWrite(ControllerData->get_brdenablepin(), 0);
  // boards require 1ms before stepping can occur
  delay(1);
#elif (DRVBRD == PRO2ESP32L293DMINI) //MN use enablepin with L293D
    digitalWrite(ControllerData->get_brdenablepin(), 1); // MN enable L293D to drive motor
    delay(1);
#endif
}

// ----------------------------------------------------------------------
// RELEASE MOTOR
// driverboard->releasemotor()
// Turns off coil power current to the motor.
// ----------------------------------------------------------------------
void DRIVER_BOARD::releasemotor(void) {
#if (DRVBRD == PRO2ESP32DRV8825) || (DRVBRD == PRO2ESP32R3WEMOS) || (DRVBRD == PRO2ESP32TMC2225) \
  || (DRVBRD == PRO2ESP32TMC2209) || (DRVBRD == PRO2ESP32TMC2209P) || (DRVBRD == PRO2ESP32ST6128) \
  ||(DRVBRD == PRO2ESP32LOLINS2MINI)

#elif (DRVBRD == PRO2ESP32ULN2003) || (DRVBRD == PRO2ESP32L298N) || (DRVBRD == PRO2ESP32L293DMINI) \
  || (DRVBRD == PRO2ESP32L9110S)
  digitalWrite(this->_inputpins[0], 0);
  digitalWrite(this->_inputpins[1], 0);
  digitalWrite(this->_inputpins[2], 0);
  digitalWrite(this->_inputpins[3], 0);
#endif
}

// ----------------------------------------------------------------------
// MOVE MOTOR
// driverboard->movemotor(byte direction, bool updatefocuser position when moving)
// ----------------------------------------------------------------------
void DRIVER_BOARD::movemotor(byte ddir, bool updatefpos) {
  // the fixed step mode board does not have any move associated with them in driver_board.cpp
  // only ESP32 boards have in out leds
  stepdir = ddir;

  // Basic assumption rule: If associated pin is -1 then cannot set enable
  // turn on leds
  if ((this->_leds_loaded == V_ENABLED) && (this->_ledmode == LEDPULSE)) {
    (stepdir == moving_in) ? digitalWrite(ControllerData->get_brdinledpin(), 1) : digitalWrite(ControllerData->get_brdoutledpin(), 1);
  }

  // do direction, enable and step motor
#if (DRVBRD == PRO2ESP32DRV8825) || (DRVBRD == PRO2ESP32R3WEMOS) || (DRVBRD == PRO2ESP32TMC2225) \
  || (DRVBRD == PRO2ESP32TMC2209) || (DRVBRD == PRO2ESP32TMC2209P) || (DRVBRD == PRO2ESP32ST6128) \
  || (DRVBRD == PRO2ESP32LOLINS2MINI)
  // set Direction of travel
  if (ControllerData->get_reverse_enable() == V_ENABLED) {
    digitalWrite(ControllerData->get_brddirpin(), !stepdir);
  } else {
    digitalWrite(ControllerData->get_brddirpin(), stepdir);
  }
  // board is enabled by init_motor() before timer starts, so not required here
  // Step pin on
  digitalWrite(ControllerData->get_brdsteppin(), 1);
  // assume clock frequency is 240mHz
  // ESP32 must be 2uS delay for DRV8825 chip
  asm1uS();
  asm1uS();
  asm1uS();
  asm1uS();
  asm1uS();
  asm1uS();
  // Step pin off
  digitalWrite(ControllerData->get_brdsteppin(), 0);
#elif (DRVBRD == PRO2ESP32ULN2003) || (DRVBRD == PRO2ESP32L298N) || (DRVBRD == PRO2ESP32L293DMINI) \
  || (DRVBRD == PRO2ESP32L9110S)
  if (stepdir == moving_in) {
    if (ControllerData->get_reverse_enable() == V_ENABLED) {
      myhstepper->step(1);
    } else {
      myhstepper->step(-1);
    }
  } else {
    if (ControllerData->get_reverse_enable() == V_ENABLED) {
      myhstepper->step(-1);
    } else {
      myhstepper->step(1);
    }
  }
  asm1uS();
  asm1uS();
#endif

  // turn off leds
  if ((this->_leds_loaded == V_ENABLED) && (this->_ledmode == LEDPULSE)) {
    (stepdir == moving_in) ? digitalWrite(ControllerData->get_brdinledpin(), 0) : digitalWrite(ControllerData->get_brdoutledpin(), 0);
  }

  // update focuser position
  if (updatefpos) {
    (stepdir == moving_in) ? this->_focuserposition-- : this->_focuserposition++;
  }
}

// ----------------------------------------------------------------------
// INIT MOVE
// driverboard->initmove(direction, steps to move)
// This enables the move timer and sets the leds for the required mode
// ----------------------------------------------------------------------
void DRIVER_BOARD::initmove(bool mdir, long steps) {
  stepdir = mdir;
  // make sure stepcount is 0 when DRIVER_BOARD created
  portENTER_CRITICAL(&stepcountMux);
  stepcount = steps;
  portEXIT_CRITICAL(&stepcountMux);
  DRIVER_BOARD::enablemotor();
  portENTER_CRITICAL(&timerSemaphoreMux);
  timerSemaphore = false;
  portEXIT_CRITICAL(&timerSemaphoreMux);

  debug_server_print(db37);
  debug_server_println(steps);

  // cache led mode at start of move because it may have changed
  this->_ledmode = ControllerData->get_inoutled_mode();

  // if ledmode is ledmove then turn on leds now
  if (this->_ledmode == LEDMOVE) {
    (stepdir == moving_in) ? digitalWrite(ControllerData->get_brdinledpin(), 1) : digitalWrite(ControllerData->get_brdoutledpin(), 1);
  }

  // get current board speed delay value
  unsigned long curspd = ControllerData->get_brdmsdelay();

  // handle the board step delays for TMC22xx steppers differently
  if (this->_boardnum == PRO2ESP32TMC2225 || this->_boardnum == PRO2ESP32TMC2209 || this->_boardnum == PRO2ESP32TMC2209P) {
    switch (ControllerData->get_brdstepmode()) {
      case STEP1:
        //curspd = curspd;
        break;
      case STEP2:
        curspd = curspd / 2;
        break;
      case STEP4:
        curspd = curspd / 4;
        break;
      case STEP8:
        curspd = curspd / 8;
        break;
      case STEP16:
        curspd = curspd / 16;
        break;
      case STEP32:
        curspd = curspd / 32;
        break;
      case STEP64:
        curspd = curspd / 64;
        break;
      case STEP128:
        curspd = curspd / 128;
        break;
      case STEP256:
        curspd = curspd / 256;
        break;
      default:
        curspd = curspd / 4;
        break;
    }
  }  // if( this->_boardnum == PRO2ESP32TMC2225 || this->_boardnum == PRO2ESP32TMC2209 || this->_boardnum == PRO2ESP32TMC2209P)

  debug_server_print(db38);
  debug_server_println(curspd);

#if (DRVBRD == PRO2ESP32TMC2209) || (DRVBRD == PRO2ESP32TMC2209P)
  // for TMC2209 stall guard, setting varies with speed setting so we need to adjust sgval for best results
  // handle different motor peeds
  if (ControllerData->get_stallguard_state() == Use_Stallguard) {
    byte sgval = ControllerData->get_stallguard_value();
    switch (ControllerData->get_motorspeed()) {
      case 0:  // slow, 1/3rd the speed
        curspd *= 3;
        // no need to change stall guard
        break;
      case 1:  // med, 1/2 the speed
        curspd *= 2;
        sgval = sgval / 2;
        break;
      case 2:  // fast, 1/1 the speed
        // curspd obviously not needed
        // curspd *= 1;
        sgval = sgval / 6;
        break;
    }
    debug_server_print(db39);
    debug_server_println(sgval);
    // don't change the value in ControllerData : this is for a speed calculation
    mytmcstepper->SGTHRS(sgval);
  }
#endif

  // timer-number, prescaler, count up (true) or down (false)
  movetimer = timerBegin(1, 80, true);
  // handler name, address of function int handler, edge=true
  timerAttachInterrupt(movetimer, &onTimer, true);

  // Set alarm to call onTimer function every interval value curspd (value in microseconds).
  // Repeat the alarm (third parameter)
  // timer for ISR, interval time, reload=true
  timerAlarmWrite(movetimer, curspd, true);
  timerAlarmEnable(movetimer);
}

// ----------------------------------------------------------------------
// END MOVE
// driverboard->end_move()
// when a move has completed [or halted], we need to detach/disable movetimer
// ----------------------------------------------------------------------
void DRIVER_BOARD::end_move(void) {
  debug_server_println(db40);

  // stop the timer
  timerStop(movetimer);
  timerAlarmDisable(movetimer);
  timerDetachInterrupt(movetimer);
  digitalWrite(ControllerData->get_brdenablepin(), 0); // MN use of enablepin for L293D

  // if using led move mode then turn off leds at end of move
  if ((this->_leds_loaded == V_ENABLED) && (this->_ledmode == LEDMOVE)) {
    digitalWrite(ControllerData->get_brdinledpin(), 0);
    digitalWrite(ControllerData->get_brdoutledpin(), 0);
  }
}

// ----------------------------------------------------------------------
// GET DIRECTION OF MOVE
// driverboard->getdirection()
// ----------------------------------------------------------------------
bool DRIVER_BOARD::getdirection(void) {
  return stepdir;
}

// ----------------------------------------------------------------------
// GET POSITION
// driverboard->position()
// ----------------------------------------------------------------------
long DRIVER_BOARD::getposition(void) {
  return this->_focuserposition;
}

// ----------------------------------------------------------------------
// SET POSITION
// driverboard->setdirection(21345)
// ----------------------------------------------------------------------
void DRIVER_BOARD::setposition(long newpos) {
  this->_focuserposition = newpos;
}

// ----------------------------------------------------------------------
// GET STALL GUARD VALUE TMC2209
// driverboard->getstallguardvalue()
// ----------------------------------------------------------------------
byte DRIVER_BOARD::getstallguardvalue(void) {
#if (DRVBRD == PRO2ESP32TMC2209 || DRVBRD == PRO2ESP32TMC2209P)
  // protection required around mytmcstepper - it is not defined if not using tmc2209 or tmc2225
  // read sgthreshold
  byte sgval = mytmcstepper->SGTHRS();
  ControllerData->set_stallguard_value(sgval);
#endif
  return ControllerData->get_stallguard_value();
}

// ----------------------------------------------------------------------
// SET STALL GUARD VALUE TMC2209
// driverboard->setstallguardvalue(240)
// ----------------------------------------------------------------------
void DRIVER_BOARD::setstallguardvalue(byte newval) {
#if (DRVBRD == PRO2ESP32TMC2209 || DRVBRD == PRO2ESP32TMC2209P)
  // protection required around mytmcstepper - it is not defined if not using tmc2209 or tmc2225
  // write sgthreshold
  mytmcstepper->SGTHRS(newval);
#endif
  ControllerData->set_stallguard_value(newval);
}

// ----------------------------------------------------------------------
// SET TMC2209 CURRENT
// driverboard->settmc2209current(600)
// ----------------------------------------------------------------------
void DRIVER_BOARD::settmc2209current(int newval) {
  ControllerData->set_tmc2209current(newval);
#if (DRVBRD == PRO2ESP32TMC2209 || DRVBRD == PRO2ESP32TMC2209P)
  // protection required around mytmcstepper - it is not defined if not using tmc2209 or tmc2225
  // Set driver current
  mytmcstepper->rms_current(ControllerData->get_tmc2209current());
#endif
}

// ----------------------------------------------------------------------
// SET TMC2225 CURRENT
// driverboard->settmc2225current(450)
// ----------------------------------------------------------------------
void DRIVER_BOARD::settmc2225current(int newval) {
  ControllerData->set_tmc2225current(newval);
#if (DRVBRD == PRO2ESP32TMC2225)
  // protection around mytmcstepper - it is not defined if not using tmc2209 or tmc2225
  // Set driver current
  mytmcstepper->rms_current(ControllerData->get_tmc2225current());
#endif
}
