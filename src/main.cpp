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
#include <draw.h>
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


