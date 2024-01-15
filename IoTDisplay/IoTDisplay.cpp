#include <Arduino.h>
#include <SPI.h>

// ESP32-S3 variant
const int kSw0Pin = 8;
const int kSw1Pin = 18;
const int kSw2Pin = 17;
const int kOledRstPin = 2;
const int kOledDcPin = 39;
const int kOledCsPin = 38;
const int kEpdBusyPin = 42;

const int kOledSckPin = 40;
const int kOledMosiPin = 41;

const int kLedR = 7;
const int kLedG = 15;
const int kLedB = 16;

SPIClass spi(HSPI);  // for ESP32S3


// ESP32-C6 variant
// Currently requires the esp32 3.0.0-alpha board
// add this to the Arduino IDE board managers URL (comma-separated):
// https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_dev_index.json
// does not appear to be supported by PlatformIO yet
// const int kSw0Pin = 11;
// const int kSw1Pin = 10;
// const int kSw2Pin = 8;
// const int kOledRstPin = 3;
// const int kOledDcPin = 22;
// const int kOledCsPin = 21;
// const int kEpdBusyPin = 15;

// const int kOledSckPin = 23;
// const int kOledMosiPin = 20;  // NOTE - needs a jumper, otherwise pad on PCB is NC!

// const int kLedR = 7;
// const int kLedG = 0;
// const int kLedB = 1;

// SPIClass spi(FSPI);  // for ESP32C6 - only has FSPI


#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
// Full displays list at
// https://github.com/ZinggJM/GxEPD2/tree/master/examples/GxEPD2_Example
// GxEPD2_BW<GxEPD2_290_I6FD, GxEPD2_290_I6FD::HEIGHT> display(GxEPD2_290_I6FD(kOledCsPin, kOledDcPin, kOledRstPin, kEpdBusyPin)); // compatible w/ Waveshare 2.9 flexible

// These in concept should support ER-EPD-027-1/2 but don't work =()
// #include <epd3c/GxEPD2_270c.h>  // default include file is broken
// GxEPD2_BW<GxEPD2_270, GxEPD2_270::HEIGHT> display(GxEPD2_270(kOledCsPin, kOledDcPin, kOledRstPin, kEpdBusyPin)); // GDEW027W3 176x264, EK79652 (IL91874)
// GxEPD2_3C<GxEPD2_270c, GxEPD2_270c::HEIGHT> display(GxEPD2_270c(kOledCsPin, kOledDcPin, kOledRstPin, kEpdBusyPin)); // GDEW027C44 176x264, IL91874

// GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(kOledCsPin, kOledDcPin, kOledRstPin, kEpdBusyPin));  // SSD1680, compatible w/ ER-EPD029-2R

GxEPD2_7C<GxEPD2_565c, GxEPD2_565c::HEIGHT / 4> display(GxEPD2_565c(kOledCsPin, kOledDcPin, kOledRstPin, kEpdBusyPin)); // Waveshare 5.65" 7-color


#include "esp_wifi.h"  // support wifi stop
#include <WiFi.h>
#include <HTTPClient.h>
#include "WifiConfig.h"  // must define 'const char* ssid' and 'const char* password'
const char* kNtpServer = "time.google.com";
const char* kTimezone = "PST8PDT,M3.2.0,M11.1.0";  // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
const char* kHttpGetUrl = "http://192.168.2.188:8000";

#include <PNGdec.h>
PNG png;


uint8_t streamData[65536] = {0};  // allocate in static memory
uint8_t* streamDataPtr = streamData;


void PNGDraw(PNGDRAW *pDraw) {
  uint16_t usPixels[448];
  uint8_t ucMask[448/8];
  
  png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  png.getAlphaMask(pDraw, ucMask, 255);

  for (size_t i=0; i<pDraw->iWidth; i++) {
    if (((usPixels[i] & 0x1f) < 0x10) && ((ucMask[i/8] >> ((7-i) % 8)) & 0x1)) {  // darker than grey
      display.drawPixel(i, pDraw->y, GxEPD_BLACK);
    }
  }
}

void setup() {
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_DEBUG);

  pinMode(kLedR, OUTPUT);
  pinMode(kLedG, OUTPUT);
  pinMode(kLedB, OUTPUT);

  digitalWrite(kLedR, 1);
  digitalWrite(kLedG, 0);
  digitalWrite(kLedB, 0);

  spi.begin(kOledSckPin, -1, kOledMosiPin, -1);
  display.epd2.selectSPI(spi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200);

  display.setRotation(1);
  display.setFullWindow();
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);
  } while (display.nextPage());

  log_i("Total heap: %d, PSRAM: %d", ESP.getHeapSize(), ESP.getPsramSize());

  // NETWORK CODE
  //
  long int timeStartWifi = millis();
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(100);
    log_i("...");
  }
  log_i("Connected WiFi: %s, RSSI=%i", WiFi.localIP().toString(), WiFi.RSSI());

  // see https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/
  long int timeStartNtp = millis();
  configTime(0, 0, "pool.ntp.org");
  setenv("TZ", kTimezone, 1);
  tzset();
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)){
    log_e("Failed to get NTP time");
  } else {
  }
  log_i("%04i-%02i-%02i %02i:%02i:%02i%s", timeinfo.tm_year + 1900, timeinfo.tm_mon + 1, timeinfo.tm_mday, 
      timeinfo.tm_hour, timeinfo.tm_min, timeinfo.tm_sec, 
      timeinfo.tm_isdst ? " DST" : "");

  long int timeStartGet = millis();
  HTTPClient http;
  http.useHTTP10(true);  // disabe chunked encoding, since the stream doesn't remove metadata
  http.begin(kHttpGetUrl);
  int httpResponseCode = http.GET();
  int httpResponseLen = http.getSize();
  log_i("GET: %i (%i KiB) <= %s", httpResponseCode, httpResponseLen / 1024, kHttpGetUrl);

  if (httpResponseCode == 200) {
    WiFiClient* stream = http.getStreamPtr();
    while(http.connected()) {
      size_t streamSize = stream->available();
      size_t bufferLeft = sizeof(streamData) - (streamDataPtr - streamData);
      log_i("  Stream: %i / %i free", streamSize, bufferLeft);

      int c = stream->readBytes(streamDataPtr, ((streamSize > bufferLeft) ? bufferLeft : streamSize));
      streamDataPtr += c;

      if (streamSize <= 0 || bufferLeft <= 0) {
        break;
      }
    }
  }

  http.end();

  // done with all network tasks, stop wifi to save power
  WiFi.disconnect();
  if (esp_wifi_stop() != ESP_OK) {
    log_e("Failed disable WiFi");
  }
  long int timeStopWifi = millis();
  log_i("Total network active time: %.1f", (float)(timeStopWifi - timeStartWifi) / 1000);
  
  delay(100);

  // DISPLAY RENDERING CODE
  //
  display.firstPage();
  do {
    display.fillScreen(GxEPD_WHITE);

    if (streamDataPtr > streamData) {  // got a PNG
      int rc = png.openRAM((uint8_t *)streamData, sizeof(streamData), PNGDraw);
      if (rc == PNG_SUCCESS) {
        rc = png.decode(NULL, 0);
        png.close();
      }
    } else {  // failed for whatever reason
      int16_t tbx, tby; uint16_t tbw, tbh;

      const char* kErrMsg = "error fetching image";
      display.getTextBounds(kErrMsg, 0, 0, &tbx, &tby, &tbw, &tbh);
      // center the bounding box by transposition of the origin:
      uint16_t x = ((display.width() - tbw) / 2) - tbx;
      uint16_t y = ((display.height() - tbh) / 2) - tby;

      display.fillScreen(GxEPD_YELLOW);

      display.setTextColor(GxEPD_RED);
      display.setFont(&FreeMonoBold9pt7b);
      display.setCursor(x, y);
      display.print(kErrMsg);
    }
  } while (display.nextPage());

  digitalWrite(kLedR, 0);
  digitalWrite(kLedG, 1);

  display.hibernate();
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(kLedG, 0);
  delay(100);
  digitalWrite(kLedG, 1);
  delay(100);
}
