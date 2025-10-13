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
#include <ArduinoJson.h>
#include <FS.h>
#include <SD.h>
#include "keys.h"
#define BUFFPIXEL 20

TFT_eSPI tft = TFT_eSPI();

const char* ssid = WIFI;
const char* password = WIFI_PASS;

String apiKey = String(API_KEY);
String location_Lincoln = "Lincoln,NE";
String location_Omaha = "Omaha,NE";

void tftInit() {
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(10,10);
}

void rectMeter(int fishScore){
  int16_t y;
  y = tft.getCursorY();
  int barWidth = map(fishScore, 0, 100, 0, tft.width() - 20);

  uint16_t color = TFT_RED;
  if (fishScore >= 80) {
        color = TFT_GREEN;
      } else if (fishScore >= 60) {
        color = TFT_YELLOW;
      } else if (fishScore >= 40) {
        color = TFT_ORANGE;
      }
  tft.drawRect(10, y+10, tft.width()-20, 20, TFT_WHITE);
  tft.fillRect(10, y+10, barWidth, 20, color);
}

int fishScore(int cloud,int wind_mph,float pressure,float temp_f,int rainChance) {
  int rating = 0;

      //Cloud ideal 80+
      if (cloud >= 80){
        rating += 20;
      } else if (cloud >= 50 && cloud < 80){
        rating += 10;
      } else {
        rating += 5;
      }

      //Wind ideal < 10
      if (wind_mph <= 10.0){
        rating += 20;
      } else if (wind_mph > 10.0 && wind_mph <= 20.0){
        rating += 10;
      } else {
        rating += 5;
      }

      //Pressure ideal
      if (pressure >= 29.8 && pressure <= 30.2) {
        rating += 20;
      } else if (pressure < 29.6) {
        rating += 5;
      }

      //Temp ideal 60-75 F
      if (temp_f >= 60.0 && temp_f <= 75.0) {
        rating += 20;
      } else if (temp_f >= 50.0 || temp_f <= 85.0) {
        rating += 10;
      } else {
        rating += 5;
      }

      //Rain
      if (rainChance >= 0 && rainChance <= 30) {
        rating += 20;
      } else if (rainChance > 60) {
        rating += 5;
      }
    
  return rating;
}

void fetchWeather(String location) {
  if (WiFi.status() == WL_CONNECTED) {
    HTTPClient http;

    String url = "http://api.weatherapi.com/v1/forecast.json?key=" + apiKey +
                 "&q=" + location + "&days=1&aqi=no&alerts=no";
    http.begin(url);
    int httpCode = http.GET();

    if (httpCode > 0) {
      String payload = http.getString();
      Serial.println(payload); // Debug: see raw JSON

      // Build a filter to only keep what we care about
      StaticJsonDocument<512> filter;
      filter["current"]["temp_f"] = true;
      filter["current"]["wind_mph"] = true;
      filter["current"]["wind_dir"] = true;
      filter["forecast"]["forecastday"][0]["astro"]["sunrise"] = true;
      filter["forecast"]["forecastday"][0]["astro"]["sunset"] = true;
      filter["current"]["pressure_in"] = true;
      filter["forecast"]["forecastday"][0]["day"]["daily_chance_of_rain"] = true;
      filter["current"]["cloud"] = true;
      

      // Document to hold filtered data
      DynamicJsonDocument doc(4096);

      // Deserialize with filter
      DeserializationError error = deserializeJson(
          doc, payload, DeserializationOption::Filter(filter));

      if (error) {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.f_str());
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(10, 10);
        tft.println("PARSE ERROR");
        return;
      }

      // Handle WeatherAPI errors
      if (doc.containsKey("error")) {
        const char* msg = doc["error"]["message"];
        Serial.print("WeatherAPI error: ");
        Serial.println(msg);
        tft.fillScreen(TFT_BLACK);
        tft.setCursor(10, 10);
        tft.printf("API error: %s\n", msg);
        return;
      }

      // Extract filtered values
      float temp_f = doc["current"]["temp_f"] | -99.0;
      int wind_mph = doc["current"]["wind_mph"] | -1.0;
      const char* wind_dir = doc["current"]["wind_dir"] | "?";
      const char* sunrise = doc["forecast"]["forecastday"][0]["astro"]["sunrise"] | "N/A";
      const char* sunset = doc["forecast"]["forecastday"][0]["astro"]["sunset"] | "N/A";
      float pressure = doc["current"]["pressure_in"] | -1;
      int rainChance = doc["forecast"]["forecastday"][0]["day"]["daily_chance_of_rain"] | -1;
      int cloud = doc["current"]["cloud"] | -1;

      int score = fishScore(cloud, wind_mph, pressure, temp_f, rainChance);

      String fishRate;
      
      if (score >= 80) {
        fishRate = "Excellent";
      } else if (score >= 60) {
        fishRate = "Good";
      } else if (score >= 40) {
        fishRate = "Fair";
      } else {
        fishRate = "Poor";
      }

      // Display on TFT
      tft.printf("Location: %s\n", location);
      tft.printf("Temp: %.1f F\n", temp_f);
      tft.printf("Wind: %d mph %s\n", wind_mph, wind_dir);
      tft.printf("Sunrise: %s\n", sunrise);
      tft.printf("Sunset: %s\n", sunset);
      tft.printf("Fishing Score: %d\n", score);
      tft.printf("Rating: %s\n", fishRate.c_str());
      
      rectMeter(score);
      tft.println();
      tft.println();

    } else {
      Serial.printf("HTTP GET failed, code: %d\n", httpCode);
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(10, 10);
      tft.printf("HTTP error %d\n", httpCode);
    }

    http.end();
  }
  
}


// Draw a BMP file from SD card to the TFT
uint16_t read16(fs::File &f) {
  uint16_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read(); // MSB
  return result;
}

uint32_t read32(fs::File &f) {
  uint32_t result;
  ((uint8_t *)&result)[0] = f.read(); // LSB
  ((uint8_t *)&result)[1] = f.read();
  ((uint8_t *)&result)[2] = f.read();
  ((uint8_t *)&result)[3] = f.read(); // MSB
  return result;
}

void drawBmp(const char *filename, int16_t x, int16_t y) {
  Serial.print("Opening "); Serial.println(filename);
  fs::File bmpFile = SD.open(filename);
  if (!bmpFile) {
    Serial.println("File not found!");
    return;
  } else{
    Serial.println("File Found");
  }

  uint32_t fileSize = bmpFile.size();
  Serial.print("File size: "); Serial.println(fileSize);

  if (read16(bmpFile) != 0x4D42) {
    Serial.println("Not a BMP!");
    bmpFile.close();
    return;
  } else {
    Serial.println("BMP");
  }

  (void)read32(bmpFile); // file size 
  (void)read32(bmpFile); // reserved
  uint32_t imageOffset = read32(bmpFile);
  (void)read32(bmpFile); // header size
  int32_t w = read32(bmpFile);
  int32_t h = read32(bmpFile);
  if (read16(bmpFile) != 1) { bmpFile.close(); return; }
  uint16_t depth = read16(bmpFile);
  uint32_t compression = read32(bmpFile);

  Serial.printf("W=%d H=%d depth=%u comp=%u off=%u\n", w, h, depth, compression, imageOffset);

  if (depth != 24 || compression != 0) {
    Serial.println("Unsupported BMP format (need 24-bit, no compression)");
    bmpFile.close();
    return;
  } else {
    Serial.println("Supported BMP format");
  }

  if (w <= 0 || h == 0) {
    Serial.println("Invalid dimensions");
    bmpFile.close();
    return;
  } else {
    Serial.println("Valid dimensions");
  }

  bool flip = true;
  if (h < 0) { h = -h; flip = false; }

  // Clip to screen bounds
  if (x + w > tft.width() || y + h > tft.height()) {
    Serial.println("Image exceeds screen bounds, aborting");
    bmpFile.close();
    return;
  } else {
    Serial.println("Image within screen bounds");
  }

  int rowSize = (w * 3 + 3) & ~3;
  if (imageOffset >= fileSize) {
    Serial.println("Bad imageOffset");
    bmpFile.close();
    return;
  } else {
    Serial.println("Pass imageOffset");
  }

  const int bufPixels = BUFFPIXEL;
  uint8_t sdbuffer[3 * bufPixels];
  uint16_t lcdbuffer[bufPixels];

  for (int row = 0; row < h; ++row) {
    // Allow background tasks / watchdog
    yield();

    uint32_t rowPos = imageOffset + (flip ? (uint32_t)(h - 1 - row) * rowSize
                                         : (uint32_t)row * rowSize);
    if (rowPos >= fileSize) {
      Serial.printf("Row pos %u out of range, file size %u\n", rowPos, fileSize);
      break;
    } else {
      Serial.println("Row pos in range");
    }

    if (!bmpFile.seek(rowPos)) {
      Serial.printf("seek failed to %u\n", rowPos);
      break;
    } else {
      Serial.println("Seek passed");
    }

    for (int col = 0; col < w; col += bufPixels) {
      int block = min(bufPixels, w - col);
      size_t bytesNeeded = block * 3;
      int bytesRead = bmpFile.read(sdbuffer, bytesNeeded);
      if (bytesRead <= 0) {
        Serial.printf("Read error at row %d col %d (requested %u got %d)\n", row, col, bytesNeeded, bytesRead);
        memset(sdbuffer, 0, bytesNeeded);
      } else if ((size_t)bytesRead < bytesNeeded) {
        memset(sdbuffer + bytesRead, 0, bytesNeeded - bytesRead);
        Serial.printf("Partial read %d/%u\n", bytesRead, bytesNeeded);
      }

      for (int i = 0; i < block; ++i) {
        uint8_t b = sdbuffer[i * 3 + 0];
        uint8_t g = sdbuffer[i * 3 + 1];
        uint8_t r = sdbuffer[i * 3 + 2];
        lcdbuffer[i] = tft.color565(r, g, b);
      }

      // Acquire display SPI only for the actual pushImage call
      tft.startWrite();
      tft.pushImage(x + col, y + row, block, 1, lcdbuffer);
      tft.endWrite();
    }
  }

  bmpFile.close();
  Serial.println("BMP draw complete");
}


void setup() {
  Serial.begin(9600);
  tftInit();
  WiFi.begin(ssid, password);
  tft.println("Connecting to network");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    tft.print(".");
  }
  tft.println("\n Connected!");
  if (!SD.begin(15)) {
    tft.println("SD Card Failed");
  }
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,10);
  fetchWeather(location_Lincoln);
  fetchWeather(location_Omaha);
  drawBmp("/catfish.bmp", 60, 320);
}

void loop() {
  delay(3600000);
  tft.fillScreen(TFT_BLACK);
  tft.setCursor(0,10);
  fetchWeather(location_Lincoln);
  fetchWeather(location_Omaha);
  drawBmp("/fish.bmp", 60, 320);
}


