#include <Arduino.h>
#include <TFT_eSPI.h>
#include <SPI.h>
#include <SD.h>

#define BUFFPIXEL 20

extern TFT_eSPI tft;

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