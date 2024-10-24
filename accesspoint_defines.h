// -------------------------------------------------------------------------
// myFP2ESP32 ACCESSPOINT CONFIGURATION
// Copyright Robert Brown 2020-2023. All Rights Reserved.
// /defines/accesspoint_defines.h
// -------------------------------------------------------------------------


// -------------------------------------------------------------------------
// CONTROLLER TCP/IP ADDRESS WHEN IN ACCESSPOINT MODE
// Change the network address if there is a conflict with your local network
// settings.
// -------------------------------------------------------------------------
IPAddress ap_ip(192, 168, 4, 1);		// AccessPoint TCP/IP address
IPAddress ap_dns(192, 168, 4, 1);		// set it to the same address as ip
IPAddress ap_gateway(192, 168, 4, 1);	// set it to the same address as ip
IPAddress ap_subnet(255, 255, 255, 0);


// -------------------------------------------------------------------------
// CONTROLLER WIFI NETWORK CONFIGURATION
// -------------------------------------------------------------------------
// This is the default WiFi network name (SSID) and password credentials for
// the ACCESSPOINT. Password length must be 8 digits minimum and 63 digits maximum
char myAPSSID[64] = "myfp2eap";
char myAPPASSWORD[64] = "myfp2eap";
