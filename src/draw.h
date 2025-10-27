#ifndef DRAW_H
#define DRAW_H

#include <TFT_eSPI.h>
#include <Arduino.h>

extern TFT_eSPI tft;
void drawBmp(const char *filename, int16_t x, int16_t y);
uint16_t read16(fs::File &f);
uint32_t read32(fs::File &f);

#endif