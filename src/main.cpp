/*
Author: Christopher Zheng
Project: Fishing Weather Report
Description: Connects to weatherapi service to retreive weather information
in order to calculate fishing quality for the day to load onto a TFT display
*/

#include <Arduino.h>
#include <HTTPClient.h>
#include <TFT_eSPI.h>
#include <SPI.h>

TFT_eSPI tft = TFT_eSPI();

void tftInit() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10,10);
  tft.println("TEST");
}

void setup() {
  tftInit();
}

void loop() {
  
}

