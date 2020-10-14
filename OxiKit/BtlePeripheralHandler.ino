//
// This file continuously:
// 1) scans for BTLE advertisements
// 2) looks for a "known" BTLE device
// 3) connects to a "known" BTLE device
// 4) reads the data from the connected BTLE device
//    Note: If the connection is lost, the process shall start over at step 1
//
// The currnetly "known" BTLE devices are:
// - Jumper PulseOximter
// - Berry Med PulseOximeter
//
// The readings are placed in the global vars gReadingPulseOx, gReadingPulse, gReadingPerfusionIndexTimesTen and gReadingLastTimeStamp
// after they are read from the pulse ox devcie. These variables should be defined in the "main" ino file.


// Private Global Vars and local defines
//**************************************************************************

// Various states of this code
#define DEVICE_STATE_NOT_SETUP 0
#define DEVICE_STATE_IDLE 1
#define DEVICE_STATE_SCANNING 2

#define DEVICE_STATE_FOUND_JUMPER 20
#define DEVICE_STATE_FOUND_BERRYMED 21

#define DEVICE_STATE_CONNECTED_JUMPER 30
#define DEVICE_STATE_CONNECTED_BERRYMED 31

#define DEVICE_STATE_READING_JUMPER 40
#define DEVICE_STATE_READING_BERRYMED 41

// Current state
unsigned int iDeviceState = DEVICE_STATE_NOT_SETUP;

unsigned long lastBtleCallingLoopTime = 0;
unsigned long currentLoopDelay = 1500;

BLEDevice peripheral;
BLECharacteristic pulseOxCharacteristic;


// Public Functions
//**************************************************************************

void btlePeripheralHandlerLoop()
{
  unsigned long curTime = millis();
  // Run this loop every x seconds, where x is variable (currentLoopDelay) depending on what state we are in
  if ((curTime - lastBtleCallingLoopTime) > currentLoopDelay)
  {
    lastBtleCallingLoopTime = curTime;
    btleThreadWorker();
  }
}

// Private Functions
//**************************************************************************

void gotReadingsFromDevice(unsigned int pulseOx, unsigned int pulse, unsigned int perfusionIndexTimesTen)
{
  gReadingPulseOx = pulseOx;
  gReadingPulse = pulse;
  gReadingPerfusionIndexTimesTen = perfusionIndexTimesTen;
  gReadingLastTimeStamp = millis();
}

void btleThreadWorker()
{
  Log.trace("-------btleThreadWorker---------- Device State %d", iDeviceState);
  // check if a peripheral has been discovered
  switch (iDeviceState)
  {
  case DEVICE_STATE_NOT_SETUP:
    btleSetup();
    break;

  case DEVICE_STATE_IDLE:
    startScan();
    break;

  case DEVICE_STATE_SCANNING:
    if (scanDeviceHandler())
    {
      if (!setupConnection())
      {
        // Could not connect, so start scanning again
        startScan();
      }
    }
    break;

  case DEVICE_STATE_CONNECTED_JUMPER:
    if (!setupJumperSubscribe())
    {
      // Error setting up jumper subscription
      startScan();
    }
    break;

  case DEVICE_STATE_CONNECTED_BERRYMED:
    if (!setupBerryMedSubscribe())
    {
      // Error setting up jumper subscription
      startScan();
    }
    break;

  case DEVICE_STATE_READING_JUMPER:
    if (!monitorJumperPulseOx())
    {
      startScan();
    }
    break;

  case DEVICE_STATE_READING_BERRYMED:
    if (!monitorBerryMedPulseOx())
    {
      startScan();
    }
    break;
  }

  Log.trace("^^^^^ btleThreadWorker ^^^^^ ");
}

void btleSetup()
{
  // Add for very verbose library debugging
  //BLE.debug(Serial);
  // begin initialization
  if (!BLE.begin())
  {
    Log.error("starting BLE failed!");
  }
  else
  {
    iDeviceState = DEVICE_STATE_IDLE;
  }
}

void startScan()
{
  Log.trace("starting BLE scan");
  BLE.scan();
  iDeviceState = DEVICE_STATE_SCANNING;
}

bool scanDeviceHandler()
{

  Log.trace("-------scanDeviceHandler----------");
  peripheral = BLE.available();
  if (peripheral)
  {
    // discovered a peripheral, print out address, local name, and advertised service
    Log.trace("BTLE Found Address:%s  Name:%s   Adv Service UUID:%s", peripheral.address(), peripheral.localName(), peripheral.advertisedServiceUuid());

    // The Jumper BTLE device proivdes a service UUID in the advertisement, so look for that.
    if (peripheral.advertisedServiceUuid() == "cdeacb80-5235-4c07-8846-93a37ee6b86d")
    {
      Log.trace("Found Jumper");
      // stop scanning
      BLE.stopScan();
      iDeviceState = DEVICE_STATE_FOUND_JUMPER;
      return true;
    }
    // The Berry Med BTLE device only provides a name in the advertisement, so look for that.
    if (peripheral.localName() == "BerryMed")
    {
      Log.trace("Found Berry Med");
      // stop scanning
      BLE.stopScan();
      iDeviceState = DEVICE_STATE_FOUND_BERRYMED;
      return true;
    }
  }
  Log.trace("Done ^^^^^^^^^^^");
  return false;
}

bool setupConnection()
{

  // connect to the peripheral
  Log.trace("BTLE Connecting ...");

  if (peripheral.connect())
  {
    Log.trace("BTLE Connected");
  }
  else
  {
    Log.trace("BTLE Failed to connect!");
    return false;
  }

  // discover peripheral attributes
  Log.trace("Discovering attributes ...");
  if (peripheral.discoverAttributes())
  {
    Log.trace("Attributes discovered");
    if (iDeviceState == DEVICE_STATE_FOUND_JUMPER)
    {
      iDeviceState = DEVICE_STATE_CONNECTED_JUMPER;
    }
    if (iDeviceState == DEVICE_STATE_FOUND_BERRYMED)
    {
      iDeviceState = DEVICE_STATE_CONNECTED_BERRYMED;
    }
    return true;
  }
  else
  {
    Log.trace("Attribute discovery failed!");
    peripheral.disconnect();
    return false;
  }
}



// Jumper Device Specific code
//**************************************************************************

bool setupJumperSubscribe()
{
  pulseOxCharacteristic = peripheral.characteristic("cdeacb81-5235-4c07-8846-93a37ee6b86d");
  Log.trace("Subscribing to characteristic ...");
  if (!pulseOxCharacteristic)
  {
    Log.trace("no simple key characteristic found!");
    peripheral.disconnect();
    return false;
  }
  else if (!pulseOxCharacteristic.canSubscribe())
  {
    Log.trace(" characteristic is not subscribable!");
    peripheral.disconnect();
    return false;
  }
  else if (!pulseOxCharacteristic.subscribe())
  {
    Log.trace("subscription failed!");
    peripheral.disconnect();
    return false;
  }
  else
  {
    Log.trace("Subscribed");
    iDeviceState = DEVICE_STATE_READING_JUMPER;
    return true;
  }
}

bool monitorJumperPulseOx()
{

  unsigned char buffer[70];

  if (peripheral.connected() == false)
  {
    return false;
  }

  //while (peripheral.connected()) {
  for (char i = 0; i < 5; i++)
  {
    // while the peripheral is connected

    // check if the value of the simple key characteristic has been updated
    if (pulseOxCharacteristic.valueUpdated())
    {

      int count = pulseOxCharacteristic.readValue(buffer, 40);
      //Serial.print("Data: ");
      //printData(buffer, count);
      if (buffer[0] == 0x81)
      {
        unsigned int pulse = buffer[1];
        unsigned int O2 = buffer[2];
        unsigned int pi = buffer[3];
        if (pulse < 255 && O2 < 127)
        {
          Log.trace("BTLE Data Found. Pulse:%d, O2:%d, PI:%d", pulse, O2, pi);
          gotReadingsFromDevice(O2, pulse, pi);
        }
      }
    }
  }
  return true;
}


// Berry Med Device Specific code
//**************************************************************************


bool setupBerryMedSubscribe()
{
  pulseOxCharacteristic = peripheral.characteristic("49535343-1e4d-4bd9-ba61-23c647249616");
  Log.trace("Subscribing to characteristic ...");
  if (!pulseOxCharacteristic)
  {
    Log.trace("no simple key characteristic found!");
    peripheral.disconnect();
    return false;
  }
  else if (!pulseOxCharacteristic.canSubscribe())
  {
    Log.trace(" characteristic is not subscribable!");
    peripheral.disconnect();
    return false;
  }
  else if (!pulseOxCharacteristic.subscribe())
  {
    Log.trace("subscription failed!");
    peripheral.disconnect();
    return false;
  }
  else
  {
    Log.trace("Subscribed");
    iDeviceState = DEVICE_STATE_READING_BERRYMED;
    return true;
  }
}

// Found on internet:
// https://github.com/zh2x/BCI_Protocol/blob/master/BCI%20Protocol%20V1.2.pdf
// https://github.com/zh2x/SpO2-BLE-for-Android/blob/master/app/src/main/java/com/berry_med/spo2_ble/data/DataParser.java
bool monitorBerryMedPulseOx()
{

  unsigned char buffer[10];

  if (peripheral.connected() == false)
  {
    return false;
  }

  //while (peripheral.connected()) {
  for (char i = 0; i < 5; i++)
  {
    // while the peripheral is connected

    // check if the value of the simple key characteristic has been updated
    if (pulseOxCharacteristic.valueUpdated())
    {

      int count = pulseOxCharacteristic.readValue(buffer, 10);
      //Serial.print("Data: ");
      //printData(buffer, count);
      if ((buffer[0] & 0x80) > 0)
      {
        unsigned int pulse = buffer[3] | ((buffer[2] & 0x40) << 1);
        unsigned int O2 = buffer[4];
        unsigned int pi = buffer[1] & 0x0f * 10; // Doing a x10 to make it match the same as the jumper PI result where jumper gets down to the first decimal 10.5 and is represented via 105.
        if (O2 < 127)
        {
          Log.trace("BTLE Data Found. Pulse:%d, O2:%d, PI:%d", pulse, O2, pi);
          gotReadingsFromDevice(O2, pulse, pi);
        }
      }
    }
  }
  return true;
}

// Helper functions used during development of connecting to a new BTLE device. Not needed for actual production, but useful enough to keep around.
//**************************************************************************

#if 0

void explorerPeripheral(BLEDevice peripheral) {

  // connect to the peripheral
  Serial.println("Connecting ...");

  if (peripheral.connect()) {
    Serial.println("Connected");
  } else {
    Serial.println("Failed to connect!");
    return;
  }

  // discover peripheral attributes
  Serial.println("Discovering attributes ...");
  if (peripheral.discoverAttributes()) {
    Serial.println("Attributes discovered");
  } else {
    Serial.println("Attribute discovery failed!");
    peripheral.disconnect();
    return;
  }

  // read and print device name of peripheral
  Serial.println();
  Serial.print("Device name: ");
  Serial.println(peripheral.deviceName());
  Serial.print("Appearance: 0x");
  Serial.println(peripheral.appearance(), HEX);
  Serial.println();

  // loop the services of the peripheral and explore each
  for (int i = 0; i < peripheral.serviceCount(); i++) {
    BLEService service = peripheral.service(i);

    exploreService(service);
  }

  Serial.println();

  // we are done exploring, disconnect
  Serial.println("Disconnecting ...");
  peripheral.disconnect();
  Serial.println("Disconnected");
}

void exploreService(BLEService service) {
  // print the UUID of the service
  Serial.print("Service ");
  Serial.println(service.uuid());

  // loop the characteristics of the service and explore each
  for (int i = 0; i < service.characteristicCount(); i++) {
    BLECharacteristic characteristic = service.characteristic(i);

    exploreCharacteristic(characteristic);
  }
}

void exploreCharacteristic(BLECharacteristic characteristic) {
  // print the UUID and properties of the characteristic
  Serial.print("\tCharacteristic ");
  Serial.print(characteristic.uuid());
  Serial.print(", properties 0x");
  Serial.print(characteristic.properties(), HEX);

  // check if the characteristic is readable
  if (characteristic.canRead()) {
    // read the characteristic value
    characteristic.read();

    if (characteristic.valueLength() > 0) {
      // print out the value of the characteristic
      Serial.print(", value 0x");
      printData(characteristic.value(), characteristic.valueLength());
    }
  }
  Serial.println();

  // loop the descriptors of the characteristic and explore each
  for (int i = 0; i < characteristic.descriptorCount(); i++) {
    BLEDescriptor descriptor = characteristic.descriptor(i);

    exploreDescriptor(descriptor);
  }
}

void exploreDescriptor(BLEDescriptor descriptor) {
  // print the UUID of the descriptor
  Serial.print("\t\tDescriptor ");
  Serial.print(descriptor.uuid());

  // read the descriptor value
  descriptor.read();

  // print out the value of the descriptor
  Serial.print(", value 0x");
  printData(descriptor.value(), descriptor.valueLength());

  Serial.println();
}

void printData(const unsigned char data[], int length) {
  for (int i = 0; i < length; i++) {
    unsigned char b = data[i];

    if (b < 16) {
      Serial.print("0");
    }

    Serial.print(b, HEX);
  }
}

#endif
