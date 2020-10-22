//
// This file manages the periodioc calling of the cloud API server.
// The WiFi status needs to be connected in order for this to function. 
//
// The code here periodiocally posts the data in gReadingPulseOx, gReadingPulse, gReadingPerfusionIndexTimesTen 
// to the API server as long as gReadingLastTimeStamp is changing.

// Private Global Vars
//**************************************************************************
char serverIp[] = SERVER_IP;
unsigned int serverPort = SERVER_PORT;
char serverPath[] = SERVER_PATH;
char serverAuthToken[] = SERVER_AUTH;

unsigned long lastApiCallingLoopTime = 0;
unsigned long lastApiInboudDataCheckLoopTime = 0;

unsigned int lastReadingTimeStampSentToServer = 0;

// This is the object we use to make the socket connection to our cloud api server
WiFiClient clientWifi;


// Public Functions
//**************************************************************************

void apiServerHandlerLoop()
{
  if(gWifiIsActive == false)
  {
    return;
  }

  unsigned long curTime = millis();
  // only do the api stuff if we are connected
  if (gStatusWifi == WL_CONNECTED)
  {
    // Run this loop every 10 seconds.
    if ((curTime - lastApiCallingLoopTime) > 10000)
    {
      lastApiCallingLoopTime = curTime;
      apiCallingLoop();
    }

    // Run this loop every 200ms
    if ((curTime - lastApiInboudDataCheckLoopTime) > 200)
    {
      lastApiInboudDataCheckLoopTime = curTime;
      clientInboundDataCheckLoop();
    }
  }
}

// Private Functions
//**************************************************************************

void apiCallingLoop()
{

  // Use arduionjson to generate our payload that gets posted to the server
  // https://arduinojson.org/v6/example/generator/
  // https://arduinojson.org/v6/doc/upgrade/
  StaticJsonDocument<200> doc;

  // Check gReadingLastTimeStamp before posting to see if the time has changed. No need to re-post the same values if the data has not changed.
  // Just POST a deviceStatusUpdate so the server can see this device is at least communicating.
  if(gReadingLastTimeStamp == 0 || gReadingLastTimeStamp == lastReadingTimeStampSentToServer)
  {
    // If the global gReadingLastTimeStamp has not changed, then just POST a deviceStatusUpdate
    doc["readingType"] = "deviceStatusUpdate";
  }
  else 
  {
    // The values have been updated since last time since gReadingLastTimeStamp changed, so POST these readings
    doc["readingType"] = "oxikit";
    doc["pulseOx"] = gReadingPulseOx;
    doc["pulse"] = gReadingPulse;
    // Update so we don't post these values again until gReadingLastTimeStamp changes
    lastReadingTimeStampSentToServer = gReadingLastTimeStamp;
  }
  Log.trace("Starting connection to server...");
  
  if (clientWifi.connect(serverIp, serverPort))
  {
    Log.trace("connected to server");
    
    // Here we are manually creating the HTTP POST "protocol" over the open socket

    // The HTTP Request header -- all in one line like this:
    //POST /v1/patientData/555 HTTP/1.1
    clientWifi.print("POST ");
    clientWifi.print(serverPath);
    clientWifi.println(" HTTP/1.1");

    // The Host request header is all one line (with Host IP:Port all contacatinated)...
    clientWifi.print("Host: ");
    clientWifi.print(serverIp);
    clientWifi.print(":");
    clientWifi.println(serverPort);

    // Auth request header
    clientWifi.print("Authorization: Bearer ");
    clientWifi.println(serverAuthToken);

    // Content Type header
    clientWifi.println("Content-Type: application/json");
    //clientWifi.println("Connection: close");
    clientWifi.print("Content-Length: ");
    clientWifi.println(measureJson(doc));

    // Extra line seperating the request headers from the posted data.
    clientWifi.println();

    // The POSTED data (serialize the doc object to a JSON string using the arduinojson lib)
    serializeJson(doc, clientWifi);
  }
}

void clientInboundDataCheckLoop()
{
  // Note: We currently are not expecting any type of meaningful response from the server,
  // but the response will be here in case we ever do want it
  while (clientWifi.available())
  {
    // TODO: Check the exact respose, but for now any response tells us we are good.
    gDataPostedToServer = true;
    char c = clientWifi.read();
#ifndef DISABLE_LOGGING
    Serial.write(c);
#endif
  }
}