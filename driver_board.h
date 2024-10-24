// ----------------------------------------------------------------------
// myFP2ESP32 BOARD DRIVER CLASS DEFINITIONS
// Copyright Robert Brown 2014-2023. All Rights Reserved.
// Copyright Holger M, 2019-2021. All Rights Reserved.
// Copyright Paul P, 2021-2022. All Rights Reserved. TMC22xx code
// driver_board.h
// ----------------------------------------------------------------------
#ifndef _driver_board_h
#define _driver_board_h

// required for DRVBRD
#include "controller_config.h"  // includes boarddefs.h and controller_defines.h

#if (DRVBRD == PRO2ESP32ULN2003) || (DRVBRD == PRO2ESP32L298N) || (DRVBRD == PRO2ESP32L293DMINI) || (DRVBRD == PRO2ESP32L9110S)
#include <myHalfStepperESP32.h>  // includes myStepperESP32.h
#endif

// tmcstepper library https://github.com/teemuatlut/TMCStepper
#if (DRVBRD == PRO2ESP32TMC2209 || DRVBRD == PRO2ESP32TMC2209P || DRVBRD == PRO2ESP32TMC2225)
#define SERIAL_PORT2 Serial2
#include <TMCStepper.h>
#endif


// ----------------------------------------------------------------------
// JOYSTICK1
// 2-AXIS Analog Thumb Joystick for Arduino
// ----------------------------------------------------------------------
// https://www.ebay.com/itm/1PCS-New-PSP-2-Axis-Analog-Thumb-GAME-Joystick-Module-3V-5V-For-arduino-PSP/401236361097
// https://www.ebay.com/itm/1PCS-New-PSP-2-Axis-Analog-Thumb-GAME-Joystick-Module-3V-5V-For-arduino-PSP/232426858990
//
// On ESP32 analog input is 0-4095. GND=GND, VCC=3.3V
// ADC2 pins cannot be used when WiFi is being used
// ADC2 [GPIO4/GPIO2/GPIO15/GPIO13/GPIO12/GPIO14/GPIO27/GPIO26/GPIO25]
// If using WiFi use ADC1 pins
// ADC1 [GPIO33/GPIO32/GPIO35/GPIO34/GPIO39/GPIO36]
// ----------------------------------------------------------------------


// ----------------------------------------------------------------------
// JOYSTICK2
// Keyes KY-023 PS2 style 2-Axis Joystick with Switch
// ----------------------------------------------------------------------
// https://www.ebay.com/sch/i.html?_from=R40&_trksid=p2510209.m570.l1313&_nkw=Keyes+KY-023+PS2+style+2-Axis+Joystick+with+Switch&_sacat=0
//
// On ESP32 analog input is 0-4095. GND=GND, VCC=3.3V
// ADC2 pins cannot be used when WiFi is being used
// ADC2 [GPIO4/GPIO2/GPIO15/GPIO13/GPIO12/GPIO14/GPIO27/GPIO26/GPIO25]
// If using WiFi use ADC1 pins
// ADC1 [GPIO33/GPIO32/GPIO35/GPIO34/GPIO39/GPIO36]

// If using JOYSTICK TYPE2 WITH SWITCH
// Wire SW to J15-y HEADER ON PCB, and install jumper on J16-PB0EN
// ----------------------------------------------------------------------


// ----------------------------------------------------------------------
// DRIVER BOARD CLASS : DO NOT CHANGE
// ----------------------------------------------------------------------
class DRIVER_BOARD {
  public:
    DRIVER_BOARD();       // constructor
    ~DRIVER_BOARD(void);  // destructor
    void start(long);
    void initmove(bool, long);   // prepare to move
    void movemotor(byte, bool);  // move the motor
    bool init_hpsw(void);        // initialize home position switch
    void init_tmc2209(void);
    void init_tmc2225(void);
    bool hpsw_alert(void);  // check for HPSW, and for TMC2209 stall guard or physical switch
    void end_move(void);    // end a move

    bool set_leds(bool);
    bool get_leds_loaded(void);
    // no need for leds_enable because it is in ControllerData

    bool set_pushbuttons(bool);
    bool get_pushbuttons_loaded(void);
    void update_pushbuttons(void);
    // no need for pushbuttons_enable because it is in ControllerData

    bool set_joystick1(bool);
    bool get_joystick1_loaded(void);
    void update_joystick1(void);
    // no need for joystick1_enable because it is in ControllerData

    bool set_joystick2(bool);
    bool get_joystick2_loaded(void);
    void update_joystick2(void);
    bool get_joystick2_swstate(void);
    void set_joystick2_swstate(bool);  // needed by ISR to set the switch flag, joystick2

    // no need for joystick2_enable because it is in ControllerData

    // get
    long getposition(void);
    byte getstallguardvalue(void);
    bool getdirection(void);

    // set
    void enablemotor(void);
    void releasemotor(void);
    void setposition(long);
    void setstepmode(int);

    void setstallguardvalue(byte);  // value
    void settmc2225current(int);
    void settmc2209current(int);

  private:
#if (DRVBRD == PRO2ESP32ULN2003) || (DRVBRD == PRO2ESP32L298N) || (DRVBRD == PRO2ESP32L293DMINI) || (DRVBRD == PRO2ESP32L9110S)
    HalfStepper* myhstepper;
    Stepper* mystepper;
#endif

#if (DRVBRD == PRO2ESP32TMC2225)
    // protection around mytmcstepper - it is not defined if not using tmc2209 or tmc2225
    TMC2208Stepper* mytmcstepper;
#endif  // #if (DRVBRD == PRO2ESP32TMC2225)
#if (DRVBRD == PRO2ESP32TMC2209 || DRVBRD == PRO2ESP32TMC2209P)
    // protection around mytmcstepper - it is not defined if not using tmc2209 or tmc2225
    TMC2209Stepper* mytmcstepper;
#endif  // DRVBRD == PRO2ESP32TMC2209  || DRVBRD == PRO2ESP32TMC2209P

    unsigned int _clock_frequency;  // clock frequency used to generate 2us delay for ESP32 160Mhz/240Mhz
    long _focuserposition;          // current focuser position
    int _inputpins[4];              // input pins for driving stepper boards
    int _boardnum;                  // get the board number from mySetupData
    bool _leds_loaded = false;
    byte _ledmode = LEDPULSE;  // cached from ControllerData for faster access when moving
    bool _pushbuttons_loaded = false;
    bool _joystick1_loaded = false;
    bool _joystick2_loaded = false;
    bool _joystick2_swstate = false;

    const char *db2 = "-init_hpsw ok";
    const char *db3 = "-set_leds ok";
    const char *db4 = "-PB ok";
    const char *db5 = "-J1 ok";
    const char *db6 = "-J2 ok";
    const char *db7 = "DB-init_hpsw start";
    const char *db8 = "-err hpsw !supported";
    const char *db9 = "-stall guard";
    const char *db10 = "-physical switch";
    const char *db11 = "-none";

    const char *db12 = "DB-inittmc2209";
    const char *db13 = "-microsteps ";
    const char *db14 = "-status ";
    const char *db15 = "-brd enable ";

    const char *db16 = "DB-inittmc2225";

    const char *db17 = "DB-hpsw_alert";  
    const char *db18 = "-stallguard";
    const char *db19 = "-physical switch";
    const char *db20 = "-None";

    const char *db21 = "DB-LEDS set";
    const char *db22 = "-err !supported";

    const char *db23 = "DB-PB-set";
    const char *db24 = "-err (J1)";
    const char *db25 = "-err (J2)";
    const char *db26 = "-enabled";
    const char *db27 = "-disabled";

    const char *db28 = "DB-J1-set";
    const char *db29 = "-err (PB)";

    const char *db30 = "DB-J1-update";
    const char *db31 = "-Raw joyval ";
    const char *db32 = "-IN  ";
    const char *db33 = "-OUT ";
    const char *db34 = "-!loaded";

    const char *db35 = "DB-J2-set";

    const char *db36 = "DB-J2-update";

    const char *db37 = "DB-init_move, steps ";
    const char *db38 = "-speeddelay ";
    const char *db39 = "-SG value ";
    const char *db40 = "DB-end_move()";

};



#endif  // _driverboards_h
