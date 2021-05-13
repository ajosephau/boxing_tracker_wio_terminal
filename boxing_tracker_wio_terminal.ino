/*
Wio terminal boxing tracker
*/

// BLE
#include <rpcBLEDevice.h>
#include <BLEServer.h>
#include <BLE2902.h>

// Battery pack
#include <SparkFunBQ27441.h>

// SD card 
#include <Seeed_FS.h>
#undef USESPIFLASH
#ifdef USESPIFLASH
#define DEV SPIFLASH
#include "SFUD/Seeed_SFUD.h"
#else
#define DEV SD
#include "SD/Seeed_SD.h"
#endif 

#ifdef _SAMD21_
#define SDCARD_SS_PIN 1
#define SDCARD_SPI SPI
#endif  

// LCD
#include <TFT_eSPI.h> // Graphics and font library for ILI9341 driver chip
#include <SPI.h>

// LCD
TFT_eSPI tft = TFT_eSPI();  // Invoke library
char *punches[] = {"Jab", "Cross", "Hook", "Bodyshot", "Uppercut", "Overhand"};
const int NUM_PUNCHES = 6;
int current_punch = 0;
bool left_hand_connected = false;
bool right_hand_connected = false;
bool center_connected = false;
bool data_updated = false;
bool data_stroke_updated = false;
bool force_redraw = true;

// BLE
BLEServer *pServer = NULL;
#define SERVICE_UUID           "E7CDFF01-8262-4851-847D-CD1961238F47"
#define CHARACTERISTIC_UUID_DATA "E7CDFF02-8262-4851-847D-CD1961238F47"
#define CHARACTERISTIC_UUID_STROKE "E7CDFF03-8262-4851-847D-CD1961238F47"

// battery pack
const unsigned int BATTERY_CAPACITY = 650; // Set Wio Terminal Battery's Capacity 

// SD card
File right_hand_file; 
bool sd_card_write = true;
bool start_recording = false;
bool record_punch_type = false;
unsigned long record_start_time = 0;

// logging
char left_hand_log[15]; 
char right_hand_log[15]; 
char center_log[15]; 
char left_data_hand_log[15]; 
char right_data_hand_log[15]; 
char center_data_log[15]; 

// tracking
String ble_data = "";
String last_accel = "";
String last_mag = "";
String last_gyro = "";
String ble_stroke_data = "";
String last_stroke = "";
unsigned long record_interval = 2000;
const unsigned long RECORD_INTERVAL_DIFF = 500;

// logistics
unsigned long current_time = 0;
unsigned long last_redraw_time = 0;
unsigned long last_button_press_time = 0;

void writeFile(fs::FS& fs, const char* path, const char* message) {
    Serial.print("Writing file: ");
    Serial.println(path);
    File file = fs.open(path, FILE_WRITE);
    if (!file) {
        Serial.println("Failed to open file for writing");
        return;
    }
    
    if (file.print(message)) {
        Serial.println("File written");
    } else {
        Serial.println("Write failed");
    }
    file.flush();
    file.close();

}

void appendFile(fs::FS& fs, const char* path, const char* message) {
    Serial.print("Appending to file: ");
    Serial.println(path);

    File file = fs.open(path, FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open file for appending");
        return;
    }
    if (file.print(message)) {
        Serial.print("Message appended:");
        Serial.println(message);
    } else {
        Serial.println("Append failed");
    }
    file.flush();
    file.close();
}

void appendFile(fs::FS& fs, const char* path, char message) {
    Serial.print("Appending to file: ");
    Serial.println(path);

    File file = fs.open(path, FILE_APPEND);
    if (!file) {
        Serial.println("Failed to open file for appending");
        return;
    }
    if (file.print(message)) {
        Serial.print("Message appended:");
        Serial.println(message);
    } else {
        Serial.println("Append failed");
    }
    file.flush();
    file.close();
}
 
void setupBQ27441(void)
{
  // Use lipo.begin() to initialize the BQ27441-G1A and confirm that it's
  // connected and communicating.
  if (!lipo.begin()) // begin() will return true if communication is successful
  {
  // If communication fails, print an error message and loop forever.
    Serial.println("Error: Unable to communicate with BQ27441.");
    Serial.println("  Check wiring and try again.");
    Serial.println("  (Battery must be plugged into Battery Babysitter!)");
    tft.setTextColor(TFT_RED);
    tft.setCursor((320 - tft.textWidth("Battery Not Initialised!"))/2, 120);
    tft.print("Battery Not Initialised!");
    while (1) ;
  }
  Serial.println("Connected to BQ27441!");
 
  // Uset lipo.setCapacity(BATTERY_CAPACITY) to set the design capacity
  // of your battery.
  lipo.setCapacity(BATTERY_CAPACITY);
}

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
  writeFile(DEV, filename, "Data,x,y,z\n");
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
      data_updated = true;
    }
};

class StrokeCallback: public BLECharacteristicCallbacks {
    void onWrite(BLECharacteristic *pCharacteristic) {
      std::string rxValue = pCharacteristic->getValue();
      ble_stroke_data = "";
      if (rxValue.length() > 0) {
        for (int i = 0; i < rxValue.length(); i++) {
          // Serial.print(rxValue[i]);
          ble_stroke_data.concat(rxValue[i]);
        }
      }
      last_stroke = ble_stroke_data;
      // Serial.println(last_stroke);
      data_stroke_updated = true;
    }
};

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
    sd_card_write = false;
  }
  Serial.println("initialization done.");

  if(sd_card_write) {
    find_file_name(left_hand_log,  "/00_LEFT.TXT");
    find_file_name(right_hand_log, "/00_RIGHT.TXT");
    find_file_name(center_log,     "/00_CENTER.TXT");
    find_file_name(left_data_hand_log,  "/00_DLEFT.TXT");
    find_file_name(right_data_hand_log, "/00_DRIGHT.TXT");
    find_file_name(center_data_log,     "/00_DCENTR.TXT");
  }

  setupBQ27441();

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

  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);

  pinMode(WIO_5S_UP, INPUT_PULLUP);
  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);
}

void loop() {
  current_time = millis();

  if (current_time - last_button_press_time > 200) {
    if (digitalRead(WIO_KEY_A) == LOW || digitalRead(WIO_KEY_B) == LOW || digitalRead(WIO_KEY_C) == LOW) {
      force_redraw = true;
      last_button_press_time = current_time;
    }

    if (digitalRead(WIO_5S_UP) == LOW) {
      // press button right
      current_punch = current_punch + 1;
      if(current_punch >= NUM_PUNCHES) {
        current_punch = 0;
      }
      last_button_press_time = current_time;
      force_redraw = true;
    }
    else if (digitalRead(WIO_5S_DOWN) == LOW) {
      // press button left
      current_punch = current_punch - 1;
      if(current_punch < 0) {
        current_punch = NUM_PUNCHES - 1;
      }
      last_button_press_time = current_time;
      force_redraw = true;
    }
    else if (digitalRead(WIO_5S_LEFT) == LOW) {
      // press button up
      record_interval = record_interval + RECORD_INTERVAL_DIFF;
      if(record_interval > 5000) {
        record_interval = 5000;
      }
      last_button_press_time = current_time;
      force_redraw = true;
    }
    else if (digitalRead(WIO_5S_RIGHT) == LOW) {
      // press button down
      record_interval = record_interval - RECORD_INTERVAL_DIFF;
      if(current_punch < 1000) {
        record_interval = 1000;
      }
      last_button_press_time = current_time;
      force_redraw = true;
    }
    else if (digitalRead(WIO_5S_PRESS) == LOW) {
      start_recording = true;
      record_punch_type = true;
      record_start_time = current_time;

      last_button_press_time = current_time;
      force_redraw = true;
    }
  }

  if(start_recording && (current_time - record_start_time > record_interval)) {
    start_recording = false;
    force_redraw = true;
  }

  if(force_redraw || (current_time - last_redraw_time > 4000)) {
    force_redraw = false;
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

    // Battery level
    unsigned int full_capacity = lipo.capacity(FULL); // Read full capacity (mAh)
    unsigned int capacity = lipo.capacity(REMAIN); // Read remaining capacity (mAh)

    tft.print("Battery level: ");
    tft.print(capacity);
    tft.print("/");
    tft.println(full_capacity);
    tft.println("");

    // Record status
    if(start_recording) {
      tft.setTextColor(TFT_WHITE, TFT_RED);  tft.setTextSize(1);
      tft.println("Recording...");
      tft.setTextColor(TFT_WHITE, TFT_BLACK);  tft.setTextSize(1);
    }
    else {
      tft.setTextColor(TFT_WHITE, TFT_BLACK);  tft.setTextSize(1);
      tft.println("Not recording...");
    }

    // Record interval
    tft.print("Sample record interval: ");
    tft.print(record_interval);
    tft.println("ms");

    // Punch type
    tft.print("Punch: ");
    tft.print(punches[current_punch]);
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
    }
  }
  if(right_hand_connected && data_updated) {
    data_updated = false;
    Serial.print("time,");
    Serial.println(String(current_time));
    if(record_punch_type) {
      Serial.print("punch-type,");
      Serial.println(punches[current_punch]);
    }    
    Serial.print(last_accel.c_str());
    Serial.print(last_gyro.c_str());
    Serial.print(last_mag.c_str());
    if(sd_card_write) {
      appendFile(DEV, right_hand_log, "time,");
      appendFile(DEV, right_hand_log, String(current_time).c_str());
      appendFile(DEV, right_hand_log, "\n");
      if(record_punch_type) {
        appendFile(DEV, right_hand_log, "punch-type,");
        appendFile(DEV, right_hand_log, punches[current_punch]);
        appendFile(DEV, right_hand_log, "\n");
      }
      appendFile(DEV, right_hand_log, last_accel.c_str());
      appendFile(DEV, right_hand_log, last_gyro.c_str());
      appendFile(DEV, right_hand_log, last_mag.c_str());
    }
  }

  if(right_hand_connected && data_stroke_updated && start_recording) {
    data_stroke_updated = false;
    Serial.print("stroke-time,");
    Serial.println(String(current_time));
    if(record_punch_type) {
      Serial.print("punch-type,");
      Serial.println(punches[current_punch]);
    }    
    Serial.println(last_stroke);
    if(sd_card_write) {
      // appendFile(DEV, right_data_hand_log, "time,");
      // appendFile(DEV, right_data_hand_log, String(current_time).c_str());
      // appendFile(DEV, right_data_hand_log, "\n");
      // if(record_punch_type) {
      //   appendFile(DEV, right_hand_log, "punch-type,");
      //   appendFile(DEV, right_hand_log, punches[current_punch]);
      //   appendFile(DEV, right_hand_log, "\n");
      // }
      // if (last_stroke.length() > 0) {
      //   for(int i=0;i<last_stroke.length();i++) {
      //     appendFile(DEV, right_data_hand_log, last_stroke[i]);
      //   }
      //   appendFile(DEV, right_data_hand_log, "\n");
      // }
    }
  }

  if(record_punch_type) {
  record_punch_type = false;
  }    
}
