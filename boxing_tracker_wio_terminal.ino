/*
Wio terminal boxing tracker
*/


#include <TFT_eSPI.h> // Graphics and font library for ILI9341 driver chip
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();  // Invoke library


void setup(void) {
    tft.init();
    tft.setRotation(2);
}

void print_sensor_status(char * hand) {
    tft.setTextColor(TFT_WHITE, TFT_BLACK);  tft.setTextSize(1);
    tft.print("- ");
    tft.print(hand);
    tft.print(": ");
    tft.setTextColor(TFT_RED, TFT_BLACK);  tft.setTextSize(1);
    tft.println("DISCONNECTED");
}

void loop() {
    tft.fillScreen(TFT_BLACK);

    tft.setCursor(0, 0, 2);
    tft.setTextColor(TFT_BLUE, TFT_BLACK);  tft.setTextSize(1);
    // Header
    tft.println("Boxing tracker v1");

    print_sensor_status("Left");
    print_sensor_status("Center");
    print_sensor_status("Right");

    delay(10000);
}
