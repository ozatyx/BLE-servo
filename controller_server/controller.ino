//Code for the remote control board. Has 2 buttons: one for selecting the mode and the other to trigger the servo
//Mode 1 sets the servo to actuate for 5 seconds then deactivate
//Mode 2 turns on the servo indefinitely

//Code for BLE taken from an example from the ESP32_BLE_Arduino library: https://github.com/nkolban/ESP32_BLE_Arduino
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <ButtonPress.h>

BLEServer* pServer = NULL;
BLECharacteristic* servoCharacteristic = NULL;
BLECharacteristic* modeCharacteristic = NULL;
bool deviceConnected = false;
bool oldDeviceConnected = false;

bool doorState = false;
bool modeState = false;
bool lastModeState = false;
bool lastDoorState = false;
char doorStr[8];
char modeStr[8];
ButtonPress doorServo(19);
ButtonPress doorMode(21);

#define SERVICE_UUID        "fda0057b-6bc8-4ad1-b3dd-076eac6cf809"
#define SERVOCHARACTERISTIC_UUID "ebf99a28-5150-47cf-9923-2495caea58e9"
#define MODECHARACTERISTIC_UUID "c0b5a5f3-c677-4e4b-aec0-956a9078a992"

class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      deviceConnected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      deviceConnected = false;
    }
};



void setup() {
  Serial.begin(115200);

  // Create the BLE Device
  BLEDevice::init("CONTROLLER");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new MyServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create a BLE Characteristic
  servoCharacteristic = pService->createCharacteristic(
                      SERVOCHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  modeCharacteristic = pService->createCharacteristic(
                      MODECHARACTERISTIC_UUID,
                      BLECharacteristic::PROPERTY_READ   |
                      BLECharacteristic::PROPERTY_WRITE  |
                      BLECharacteristic::PROPERTY_NOTIFY |
                      BLECharacteristic::PROPERTY_INDICATE
                    );

  // https://www.bluetooth.com/specifications/gatt/viewer?attributeXmlFile=org.bluetooth.descriptor.gatt.client_characteristic_configuration.xml
  // Create a BLE Descriptor
  servoCharacteristic->addDescriptor(new BLE2902());
  modeCharacteristic->addDescriptor(new BLE2902());

  // Start the service
  pService->start();

  // Start advertising
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(false);
  pAdvertising->setMinPreferred(0x0);  // set value to 0x00 to not advertise this parameter
  BLEDevice::startAdvertising();
  Serial.println("Waiting a client connection to notify...");
  doorState = 0;
  itoa(doorState, doorStr, 10);
  servoCharacteristic->setValue(doorStr);

  servoCharacteristic->notify();
  pinMode(23, OUTPUT);
  pinMode(18, OUTPUT);
}

void loop() {
  doorState = doorServo.onOff();
  modeState = doorMode.onOff();
  if(modeState){
    digitalWrite(23, HIGH);
    }else{
      digitalWrite(23, LOW);
    }
    // notify changed value
    if (deviceConnected) {
      
      if(modeState != lastModeState){
        itoa(modeState, modeStr, 10);
        modeCharacteristic->setValue(modeStr);
        modeCharacteristic->notify();
        lastModeState = modeState;

      }

      if(doorState != lastDoorState){
        itoa(doorState, doorStr, 10);
        servoCharacteristic->setValue(doorStr);
        servoCharacteristic->notify();
        digitalWrite(18, HIGH);
        delay(30);
        digitalWrite(18, LOW);
        lastDoorState = doorState;
      }
        
 // bluetooth stack will go into congestion, if too many packets are sent, in 6 hours test i was able to go as low as 3ms
    }
    // disconnecting
    if (!deviceConnected && oldDeviceConnected) {
        delay(10); // give the bluetooth stack the chance to get things ready
        pServer->startAdvertising(); // restart advertising
        Serial.println("start advertising");
        oldDeviceConnected = deviceConnected;
    }
    // connecting
    if (deviceConnected && !oldDeviceConnected) {
        // do stuff here on connecting
        oldDeviceConnected = deviceConnected;
    }
}