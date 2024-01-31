//Code for the receiving board that turns the servo when receiving a command over Bluetooth

//Code for BLE taken from an example from the ESP32_BLE_Arduino library: https://github.com/nkolban/ESP32_BLE_Arduino
//Code for servo taken from the adafruit library: https://learn.adafruit.com/16-channel-pwm-servo-driver/library-reference

#include "BLEDevice.h"
#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

// The remote service we wish to connect to.
static BLEUUID serviceUUID("fda0057b-6bc8-4ad1-b3dd-076eac6cf809");
// The characteristic of the remote service we are interested in.
static BLEUUID    charUUID("ebf99a28-5150-47cf-9923-2495caea58e9");

static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;

RTC_DATA_ATTR int lastDoorState;
RTC_DATA_ATTR int ledState;
RTC_DATA_ATTR int doorState;
uint8_t servonum = 4;

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();

#define MIN_PULSE_WIDTH       600
#define MAX_PULSE_WIDTH       2650
#define DEFAULT_PULSE_WIDTH   1500
#define FREQUENCY             50

#define uS_TO_S_FACTOR 1000000ULL
#define TIME_TO_SLEEP  3

#define SDA 21
#define SCL 22

std::string serverAddress = "a0:b7:65:4f:3c:32";


static void notifyCallback(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    Serial.print("Notify callback for characteristic ");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);
    Serial.print("data: ");
    Serial.write(pData, length);
    Serial.println();
}

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient) {
  }

  void onDisconnect(BLEClient* pclient) {
    connected = false;
  }
};

bool connectToServer() {
    Serial.print("Forming a connection to ");
    
    BLEClient*  pClient  = BLEDevice::createClient();
    Serial.println(" - Created client");

    pClient->setClientCallbacks(new MyClientCallback());

    // Connect to the remove BLE Server.
    pClient->connect(serverAddress);  // (myDevice) if you pass BLEAdvertisedDevice instead of address, it will be recognized type of peer device address (public or private)
    Serial.println(" - Connected to server");
    pClient->setMTU(517); //set client to request maximum MTU from server (default is 23 otherwise)
  
    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");


    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic");

    // Read the value of the characteristic.
    if(pRemoteCharacteristic->canRead()) {
      std::string value = pRemoteCharacteristic->readValue();
      Serial.print("The characteristic value was: ");
      Serial.println(value.c_str());
    }

    if(pRemoteCharacteristic->canNotify()){
      pRemoteCharacteristic->registerForNotify(notifyCallback);

    }


    connected = true;
    return true;
}
/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
 /**
   * Called for each advertising BLE server.
   */
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    Serial.print("BLE Advertised Device found: ");
    Serial.println(advertisedDevice.toString().c_str());

    // We have found a device, let us now see if it contains the service we are looking for.
    if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {

      BLEDevice::getScan()->stop();
      myDevice = new BLEAdvertisedDevice(advertisedDevice);
      doConnect = true;
      doScan = true;
    } // Found our server
  } // onResult
}; // MyAdvertisedDeviceCallbacks

void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}

int pulseWidth(int angle){
  int pulse_wide, analog_value;

  pulse_wide = map(angle, 0, 180, MIN_PULSE_WIDTH, MAX_PULSE_WIDTH);
  analog_value = int(float(pulse_wide) / 1000000 * FREQUENCY * 4096);

  return analog_value;
}


void setup() {
  Serial.begin(115200);
  print_wakeup_reason();
  Serial.println("Starting Arduino BLE Client application...");
  Wire.begin(SDA, SCL);
  BLEDevice::init("");
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  pwm.begin();
  pwm.setPWMFreq(FREQUENCY);  
  doConnect = true;
}


void loop() {

  // If the flag "doConnect" is true then we have scanned for and found the desired
  // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // connected we set the connected flag to be true.
  if (doConnect == true) {
    if (connectToServer()) {
      Serial.println("We are now connected to the BLE Server.");
    } else {
      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
    }
    doConnect = false;
  }

  // If we are connected to a peer BLE Server, update the characteristic each time we are reached
  // with the current time since boot.
  if (connected) {
    std::string value = pRemoteCharacteristic->readValue();
    Serial.println("CONNECTED");
    Serial.println(value.c_str());

    doorState = atoi(value.c_str());

    if(doorState != lastDoorState){

      if(doorState == 0){
        pwm.setPWM(4, 0, pulseWidth(0));
        Serial.println("locking...");
        delay(500);
      }

      if(doorState == 1){
        pwm.setPWM(4, 0, pulseWidth(120));
        Serial.println("unlocking...");
        digitalWrite(4, LOW);
        delay(500);
      }
      lastDoorState = doorState;
    }

  }else if(connected==false){
    setup();
    loop();
  }
  Serial.print(millis());
  Serial.println("time since boot:");
  
  Serial.println("going to sleep!");
  esp_deep_sleep_start();
}