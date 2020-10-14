
// Uncomment for production builds (used in this project and in src/ArduinoLog files)
//#define DISABLE_LOGGING

// Standard Included Libraries
//**************************************************************************
#include <SPI.h>

// External Libs you MUST add using the Library Manager
// The version developed/tested with is specified below for each lib. 
//**************************************************************************
// Currently using Library ArduinoBLE 1.1.3
#include <ArduinoBLE.h>
// Currently using Library WiFiNINA 1.7.1
#include <WiFiNINA.h>
// Currently using Library ArduinoJson 6.16.1
#include <ArduinoJson.h>

// Libraries included in this source code repository (see the src folder)
//**************************************************************************
#include "src/ArduinoLog/ArduinoLog.h"


// Our custom secrets/settings for this program (wifi connection, server URLs, etc)
//**************************************************************************
#include "arduino_secrets.h"

// Global Variables Shared Across all files
//**************************************************************************
int gStatusWifi = WL_IDLE_STATUS;

unsigned int gReadingPulseOx = 0;
unsigned int gReadingPulse = 0;
unsigned int gReadingPerfusionIndexTimesTen = 0; // This is x10 because it is normally a decimal value like 5.4, so the value here would be 54.
unsigned long gReadingLastTimeStamp = 0;

#ifndef DISABLE_LOGGING
// Helper to make the logs out the serial port look better
// Used in setSuffix below
void logSuffix(Print* out) {
  out->print("\r\n");
}
#endif

void setup()
{

#ifndef DISABLE_LOGGING
  //Initialize Logging (if not production)
  Serial.begin(9600);
  Log.begin(LOG_LEVEL_VERBOSE, &Serial, true);
  Log.setSuffix(&logSuffix);
  while (!Serial)
    ; // wait for serial port to connect. Needed if we want to see serial logs at startup.
  Log.trace("Log Test %d", 1);
#endif
}

void loop()
{
  // Each function handler in this loop is responsible to manage the amount of time it uses and to ignore the call
  // if it has nothing to do. Each of the calls below are separated into unique files for better functional organization of the code.

  // Handle any Wifi Connection Updates
  // Goal is to keep the WiFi connected
  wiFiConnectionHandlerLoop();

  // Handle any Server API Data Posting
  // Goal is to periodiocally POST Pulse Ox / device status data to the cloud api server
  apiServerHandlerLoop();

  // Scan for, connect to and read any known BTLE peripherals
  // Goal is to find advertising BTLE peripherals and connect to them and collect their readings
  //btlePeripheralHandlerLoop();  // TODO Note: Currently commented out because there is some bug that when used with wiFiConnectionHandlerLoop above, the system crashes/stops.

  // Manage OxiKit Hardware
  // Goal is to keep the OixKit device running using the various IO on this arduino.
  // OxiKitDeviceHandlerLoop();   // TODO: TBD
}
