// -------------------------------------------------------------------------
// myFP2ESP32 STATION CONFIGURATION
// Copyright Robert Brown 2020-2023. All Rights Reserved.
// /defines/station_defines.h
// -------------------------------------------------------------------------


// -------------------------------------------------------------------------
// EXISTING WiFi NETWORK SSID/PASSWORD
// -------------------------------------------------------------------------
// This is the Name and Password credentials used to log on to your
// WiFi network. You will need to change these.
// Password length must be 8 digits minimum and 63 digits maximum 

char mySSID[64] = "nb32-1";
char myPASSWORD[64] = "BS32-85591V-lmn";

// ALTERNATIVE NETWORK CREDENTIALS
char mySSID_1[64]     = "nb32v";            
char myPASSWORD_1[64] = "BS32-85591V-lmn";  


// -------------------------------------------------------------------------
// STATIC TCP/IP ADDRESS CONFIGURATION
// -------------------------------------------------------------------------
// If using a static TCP/IP address, define the TCP/IP configuration here

// If connecting to your local WiFi network, the 192.168.2 portion of the
// values below will need changing

// Static TCP/IP address for controller as STATION
IPAddress station_ip(192, 168, 178, 30); 
// Your local network gateway - normally address of your router
IPAddress station_gateway(192, 168, 178, 1); 
IPAddress station_subnet(255, 255, 255, 0);  
IPAddress station_dns1(192, 168, 178, 1);
IPAddress station_dns2(192, 168, 178, 1);
