/*
Wio terminal boxing tracker
*/

// BLE
#include <rpcBLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

// SD card 
#include <Seeed_FS.h>
#include "SD/Seeed_SD.h"
 
// LCD
#include <TFT_eSPI.h> // Graphics and font library for ILI9341 driver chip
#include <SPI.h>

// LCD
TFT_eSPI tft = TFT_eSPI();  // Invoke library

// BLE
BLEServer *pServer = NULL;
#define SERVICE_UUID           "E7CDFF01-8262-4851-847D-CD1961238F47"
#define CHARACTERISTIC_UUID_DATA "E7CDFF02-8262-4851-847D-CD1961238F47"
#define CHARACTERISTIC_UUID_STROKE "E7CDFF03-8262-4851-847D-CD1961238F47"

// SD card
File right_hand_file; 

// logging
char left_hand_log[15]; 
char right_hand_log[15]; 
char center_log[15]; 
String ble_data = "";
String last_accel = "";
String last_mag = "";
String last_gyro = "";
String last_rssi = "";

unsigned long current_time = 0;
unsigned long last_redraw_time = 0;

bool left_hand_connected = false;
bool right_hand_connected = false;
bool center_connected = false;

void find_file_name(char * filename, char * file_name_format) {
  strcpy(filename, file_name_format);

  for (uint8_t i = 0; i < 100; i++) {
    filename[1] = '0' + i / 10;
    filename[2] = '0' + i % 10;
    // create if does not exist, do not open existing, write, sync after write
    if (! SD.exists(filename)) {
      break;
      Serial.print("filename: ");
      Serial.println(filename);
    }
  }
  right_hand_file = SD.open(filename, FILE_WRITE); //Writing Mode
  right_hand_file.print("");
  right_hand_file.close();
}

class RightHandServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      right_hand_connected = true;
    };

    void onDisconnect(BLEServer* pServer) {
      right_hand_connected = false;
    }
};

class DataCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      ble_data = "";
      if (rxValue.length() > 0) {
        for (int i = 0; i < rxValue.length(); i++) {
          // Serial.print(rxValue[i]);
          ble_data.concat(rxValue[i]);
        }
      }
      if(ble_data.startsWith("accel")) {
        last_accel = String(ble_data);
      }      
      if(ble_data.startsWith("mag")) {
        last_mag = String(ble_data);
      }      
      if(ble_data.startsWith("gyro")) {
        last_gyro = String(ble_data);
      }      
      if(ble_data.startsWith("rssi")) {
        last_rssi = String(ble_data);
      }      
    }
};

class StrokeCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      if (rxValue.length() > 0) {
        for (int i = 0; i < rxValue.length(); i++) {
          Serial.print(rxValue[i]);
        }
      }
    }
};


void setup(void) {
  Serial.begin(115200);
  // while (!Serial) {}

  // *******************
  // setup LCD
  // *******************
  tft.init();
  tft.setRotation(2);

  // *******************
  // setup SD card
  // *******************
  Serial.print("Initializing SD card...");
  if (!SD.begin(SDCARD_SS_PIN, SDCARD_SPI)) {
    Serial.println("initialization failed!");
    while (1);
  }
  Serial.println("initialization done.");

  find_file_name(left_hand_log,  "/00_LEFT.TXT");
  find_file_name(right_hand_log, "/00_RIGHT.TXT");
  find_file_name(center_log,     "/00_CENTER.TXT");

  // *******************
  // setup BLE
  // *******************
  BLEDevice::init("BoxingTrackerServer");

  // Create the BLE Server
  pServer = BLEDevice::createServer();
  pServer->setCallbacks(new RightHandServerCallbacks());

  // Create the BLE Service
  BLEService *pService = pServer->createService(SERVICE_UUID);

  // Create the BLE Characteristic (Data)
  BLECharacteristic * pDataCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_DATA,
											BLECharacteristic::PROPERTY_WRITE
										);
  pDataCharacteristic->setAccessPermissions(GATT_PERM_READ | GATT_PERM_WRITE);           

  pDataCharacteristic->setCallbacks(new DataCallback());

  // Create the BLE Characteristic (Stroke)
  BLECharacteristic * pStrokeCharacteristic = pService->createCharacteristic(
											 CHARACTERISTIC_UUID_STROKE,
											BLECharacteristic::PROPERTY_WRITE
										);
  pStrokeCharacteristic->setAccessPermissions(GATT_PERM_READ | GATT_PERM_WRITE);           

  pStrokeCharacteristic->setCallbacks(new StrokeCallback());

  // Start the service
  pService->start();

  // Start advertising
  pServer->getAdvertising()->start();

  right_hand_file = SD.open(right_hand_log, FILE_WRITE); //Writing Mode
}

void print_sensor_status(char * hand, char * log_name, bool status) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);  tft.setTextSize(1);
    tft.print("- ");
    tft.print(hand);
    tft.print(": ");
    if (status) {
      tft.setTextColor(TFT_GREEN, TFT_BLACK);  tft.setTextSize(1);
      tft.println("CONNECTED");      
    } 
    else {
      tft.setTextColor(TFT_RED, TFT_BLACK);  tft.setTextSize(1);
      tft.println("DISCONNECTED");  
    }
    tft.print(" ");
    tft.setTextColor(TFT_WHITE, TFT_BLACK);  tft.setTextSize(1);
    tft.print("(");
    tft.print(log_name);
    tft.println(")");
}

void loop() {
  current_time = millis();
  if(current_time - last_redraw_time > 250) {
    last_redraw_time = current_time;

    tft.fillScreen(TFT_BLACK);

    tft.setCursor(0, 0, 2);
    tft.setTextColor(TFT_BLUE, TFT_BLACK);  tft.setTextSize(1);
    // Header
    tft.println("Boxing tracker v1");

    // System time
    tft.setTextColor(TFT_WHITE, TFT_BLACK);  tft.setTextSize(1);
    tft.print("Current time: ");
    tft.println(current_time);
    tft.println("");

    print_sensor_status("Left", left_hand_log, left_hand_connected);
    print_sensor_status("Center", center_log, center_connected);
    print_sensor_status("Right", right_hand_log, right_hand_connected);

    if(right_hand_connected) {
      tft.println("");
      tft.setTextColor(TFT_WHITE, TFT_BLACK);  tft.setTextSize(1);
      tft.print("- ");
      tft.print(last_accel);
      tft.print("- ");
      tft.print(last_gyro);
      tft.print("- ");
      tft.print(last_mag);
      tft.print("- ");
      tft.print(last_rssi);
    }
  }
  // delay(300);
}
