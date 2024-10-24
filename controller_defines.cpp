// ----------------------------------------------------------------------
// myFP2ESP32 GENERAL DEFINITIONS
// Copyright Robert Brown 2014-2023. All Rights Reserved.
// Copyright Holger M, 2019-2021. All Rights Reserved.
// controller_defines.h
// ----------------------------------------------------------------------

#include <Arduino.h>
#include "controller_defines.h"

// Controller Data References

const char *TEXTPAGETYPE = "text/html";
const char *PLAINTEXTPAGETYPE = "text/plain";
const char *JSONTEXTPAGETYPE = "text/json";
const char *JSONPAGETYPE = "application/json";

const char *H_FILENOTFOUNDSTR = "<html><head><title>myFP2ESP32</title></head><body><p>myFP2ESP32</p><p>File not found</p><p><form action=\"/\" method=\"GET\"><input type=\"submit\" value=\"HOMEPAGE\"></form></p></body></html>";
const char *H_FSNOTLOADEDSTR = "<html><head><title>myFP2ESP32</title></head><body><p>myFP2ESP32</p><p>err: File-system not started.</p><p><form action=\"/\" method=\"GET\"><input type=\"submit\" value=\"HOMEPAGE\"></form></p></body></html>";

const char *DEFAULTTITLECOLOR = "8E44AD";
const char *DEFAULTSUBTITLECOLOR = "3399FF";
const char *DEFAULTHEADERCOLOR = "2B65EC";
const char *DEFAULTTEXTCOLLOR = "5D6D7E";
const char *DEFAULTBACKCOLOR = "333333";

const char *T_ACCESSPOINT = "ACCESSPOINT";
const char *T_STATION = "STATION";
const char *T_CELSIUS = "Celsius";
const char *T_FAHRENHEIT = "Fahrenheit";
const char *T_SERIAL = "Serial";
const char *T_I2C = "I2C";
const char *T_HPSW = "HPSW ";
const char *T_CONTROLLERDATA = "ControllerData";
const char *T_DRIVERBOARD = "Driverboard ";
const char *T_DISPLAY = "Display ";
const char *T_OTA = "OTA";
const char *T_TEMPPROBE = "Tempprobe ";
const char *T_DUCKDNS = "Duckdns ";
const char *T_TASKTIMER = "Tasktimer";
const char *T_COILPOWER = "CoilPower ";
const char *T_IRREMOTE = "IRRemote";
const char *T_WATCHDOGTIMER = "WatchDogTimer";

const char *T_ALPACA = "Alpaca ";
const char *T_DISCOVERY = "Discovery";
const char *T_DEBUGSERVER = "Debug Server ";
const char *T_MANAGEMENTSERVER = "Management Server ";
const char *T_TCPIPSERVER = "TCPIP Server ";
const char *T_WEBSERVER = "Web Server ";

const char *T_READWIFICONFIG = "ReadWifiConfig ";
const char *T_MULTIAP = "MultiAP ";
const char *T_STATICIP = "StaticIP ";
const char *T_DYNAMICIP = "DynamicIP ";
const char *T_SSID = "SSID ";
const char *T_PWD = "PWD  ";
const char *T_ALTCREDENTIALS = "Alt credentials";
const char *T_IPADDRESS = "IPAddr ";
const char *T_HOSTNAME = "Hostname ";
const char *T_SYSTEMUPTIME = "SystemUpTime ";

const char *T_READY = "READY";

const char *T_OFF = "OFF";
const char *T_ON = "ON";
const char *T_NO = "No";
const char *T_YES = "Yes";

const char *T_DISABLED = "Disabled";
const char *T_ENABLED = "Enabled";
const char *T_FOUND = "found ";
const char *T_NOTFOUND = "err not found ";
const char *T_IN = "In";
const char *T_OUT = "Out";
const char *T_OK = "OK";
const char *T_NOTOK = "!OK";

const char *T_START = "Start ";
const char *T_ERRSTART = "err start ";
const char *T_STOP = "Stop ";
const char *T_STOPPED = "Stopped";
const char *T_RUNNING = "Running";
const char *T_ERROR = "Err ";
const char *T_INIT = "Init ";
const char *T_PARKED = "Parked";
const char *T_NOTPARKED = "!Parked";
const char *T_DESERIALISEERROR = "err deserialise ";
const char *T_LOAD = "Load ";
const char *T_LOADED = "Loaded";
const char *T_NOTLOADED = "!Loaded";
const char *T_SAVED = "Saved ";
const char *T_OPENFILE = "Open ";
const char *T_OPENERROR = "err open ";
const char *T_ERRORFILESYSTEM = "err FS";
const char *T_DELETE = "Delete ";
const char *T_NOTENABLED = "err !enabled ";
//const char *T_ERRNOTENABLED = "err !enabled ";
const char *T_NOTSUPPORTED = "err !supported ";
const char *T_CONNECTED = "Connected ";
const char *T_MOVING = "err focuser is moving";

const char *T_MOVETO = "Move ";
const char *T_TARGET = "Target ";
const char *T_GOTO = "Goto ";
const char *T_TRUE = "True";
const char *T_FALSE = "False";
