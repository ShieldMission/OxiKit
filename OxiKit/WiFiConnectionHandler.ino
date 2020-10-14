//
// This file manages the WiFi connection. It works to keep it connected.
//

// Private Global Vars
//**************************************************************************
char ssid[] = SECRET_SSID; // wifi network SSID (name)
char pass[] = SECRET_PASS; // wifi network key

unsigned long lastWiFiLoopTime = 0;

// Public Functions
//**************************************************************************

void wiFiConnectionHandlerLoop()
{
  unsigned long curTime = millis();
  // Run this loop every 3 seconds.
  if ((curTime - lastWiFiLoopTime) > 3000)
  {
    lastWiFiLoopTime = curTime;
    testWiFiConnection(); //test connection, and reconnect if necessary
    long rssi = WiFi.RSSI();
    Log.trace("RSSI %d", rssi);
  }
}

// Private Functions
//**************************************************************************

void testWiFiConnection()
{
  Log.trace("Testing WiFi...");
  gStatusWifi = WiFi.status();
  Log.notice("WiFi Status %d", gStatusWifi);

  if (gStatusWifi == WL_IDLE_STATUS || gStatusWifi == WL_CONNECTION_LOST || gStatusWifi == WL_DISCONNECTED || gStatusWifi == WL_SCAN_COMPLETED) //if no connection
  {
    Log.trace("WiFi Disconnected");
    if (scanSSIDs())
      wiFiConnect(); //if my SSID is present, connect
  }
}

void wiFiConnect()
{
  unsigned char errorCount = 0;
  gStatusWifi = WL_IDLE_STATUS;
  while (gStatusWifi != WL_CONNECTED)
  {
    gStatusWifi = WiFi.begin(ssid, pass);
    if (gStatusWifi == WL_CONNECTED)
    {
      Log.trace("WiFi Connected");
    }
    else
    {
      Log.notice("WiFi Status in loop %d", gStatusWifi);

      if (errorCount > 3)
      {
        Log.error("Error connecting. Giving Up Trying");
        break;
      }
      else
      {
        delay(500);
      }
    }
  }
}

//scan SSIDs, and if our SSID is present return 1
char scanSSIDs()
{
  char score = 0;
  int numSsid = WiFi.scanNetworks();
  if (numSsid == -1)
    return (0); //error
  for (int thisNet = 0; thisNet < numSsid; thisNet++)
    if (strcmp(WiFi.SSID(thisNet), ssid) == 0)
      score = 1; //if one is = to my SSID
  return (score);
}