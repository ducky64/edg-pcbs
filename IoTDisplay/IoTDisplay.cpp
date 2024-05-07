#include <Arduino.h>
#include <ArduinoJson.h>
#include "base64.hpp"
#include <SPI.h>
#include "esp_sleep.h"

// ESP32-S3 variant, new version
// touch_duck=TOUCH13, 21,
// touch_lemur=TOUCH14, 22,
// vbat_sense=ADC1_CH6, 7,
// epd=SPI2,
// epd.sck=GPIO40, 33,
// epd.mosi=GPIO42, 35,
// sd=SPI3, 
// sd.sck=GPIO9, 17, 
// sd.mosi=GPIO10, 18, 
// sd.miso=GPIO3, 15, 
// ledr=GPIO1, 39, 
// ledg=GPIO2, 38, 
// ledb=GPIO4, 4, 
// sw=GPIO5, 5, 
// vbat_sense_gate=GPIO6, 6, 
// epd_rst=GPIO15, 8, 
// epd_dc=GPIO38, 31, 
// epd_cs=GPIO39, 32, 
// epd_busy=GPIO16, 9, 
// sd_cs=GPIO11, 19

const int kSwPin = 5;
const int kEpdRstPin = 15;
const int kEpdDcPin = 38;
const int kEpdCsPin = 39;
const int kEpdBusyPin = 16;

const int kEpdSckPin = 40;
const int kEpdMosiPin = 42;

const int kLedR = 1;
const int kLedG = 2;
const int kLedB = 4;

const int kVsenseGate = 6;
const int kVsense = 7;

SPIClass spi(HSPI);  // for ESP32S3


#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeMonoBold9pt7b.h>
// Full displays list at
// https://github.com/ZinggJM/GxEPD2/tree/master/examples/GxEPD2_Example
// GxEPD2_7C<GxEPD2_565c, GxEPD2_565c::HEIGHT/2> display(GxEPD2_565c(kEpdCsPin, kEpdDcPin, kOledRstPin, kEpdBusyPin)); // Waveshare 5.65" 7-color, flakey
GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT> display(GxEPD2_750c_Z08(kEpdCsPin, kEpdDcPin, kEpdRstPin, kEpdBusyPin)); // works with Waveshare 3C 7.5" B

#include "esp_wifi.h"  // support wifi stop
#include <WiFi.h>
#include <HTTPClient.h>
#include "WifiConfig.h"  // must define 'const char* ssid' and 'const char* password'
const char* kHttpGetUrl = "http://192.168.2.188:8080/render";

#include <PNGdec.h>
PNG png;


uint8_t streamData[32768] = {0};  // allocate in static memory
uint8_t* streamDataPtr = streamData;
uint8_t imageData[32768] = {0};  // decoded image data
JsonDocument doc;

size_t maxWidth = 480;


RTC_DATA_ATTR int failureCount = 0;

const int kMaxWifiConnectSec = 30;  // max to wait for wifi to come up before sleeping
const int kRetrySleepSec = 120;  // on error, how long to wait to retry
const int kMaxErrorCount = 3;  // number of consecutive network failures before displaying error
const int kErrSleepSec = 3600;  // on exceeding max errors, how long to wait until next attempt


void PNGDraw(PNGDRAW *pDraw) {
  uint16_t usPixels[maxWidth];
  uint8_t ucMask[maxWidth/8];
  
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
  const char* errorStatus = NULL;  // set when an error occurs, to a short descriptive string

  pinMode(kLedR, OUTPUT);
  pinMode(kLedG, OUTPUT);
  pinMode(kLedB, OUTPUT);
  pinMode(kVsenseGate, OUTPUT);

  digitalWrite(kLedR, 1);
  digitalWrite(kLedG, 1);
  digitalWrite(kLedB, 1);


  digitalWrite(kVsenseGate, 1);
  delay(10);
  int vbat = analogRead(kVsense);
  log_i("Vbat: %d", vbat);
  digitalWrite(kVsenseGate, 0);

  log_i("Total heap: %d, PSRAM: %d", ESP.getHeapSize(), ESP.getPsramSize());
  digitalWrite(kLedR, 0);
  digitalWrite(kLedG, 0);
  digitalWrite(kLedB, 0);

  spi.begin(kEpdSckPin, -1, kEpdMosiPin, -1);
  display.epd2.selectSPI(spi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(0);

  // NETWORK CODE
  //
  long int timeStartWifi = millis();
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    if (millis() % 200 >= 100) {
      digitalWrite(kLedR, 1);
    } else {
      digitalWrite(kLedR, 0);
    }
    if ((millis() - timeStartWifi) > kMaxWifiConnectSec * 1000) {
      errorStatus = "no wifi";
      break;
    }
  }
  log_i("Connected WiFi: %s, RSSI=%i", WiFi.localIP().toString(), WiFi.RSSI());
  digitalWrite(kLedR, 1);
  digitalWrite(kLedB, 1);

  if (errorStatus == NULL) {
    long int timeStartGet = millis();
    HTTPClient http;
    http.useHTTP10(true);  // disabe chunked encoding, since the stream doesn't remove metadata
    http.setTimeout(15*1000);
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

        if (bufferLeft <= 0) {
          break;
        }
        if (streamSize == 0) {  // wait for more data transfer
          delay(1);
        }
      }
      http.end();
    } else {
      errorStatus = "response error";
    }
  }

  // done with all network tasks, stop wifi to save power
  WiFi.disconnect();
  if (esp_wifi_stop() != ESP_OK) {
    log_e("Failed disable WiFi");
  }
  long int timeStopWifi = millis();
  log_i("Total network active time: %.1f", (float)(timeStopWifi - timeStartWifi) / 1000);
  digitalWrite(kLedR, 0);
  digitalWrite(kLedB, 1);

  unsigned long sleepTimeSec = 0;
  if (errorStatus == NULL) {
    DeserializationError error = deserializeJson(doc, streamData);
    if (!error) {
      const unsigned char* base64Data = doc["image_b64"].as<const unsigned char*>();
      unsigned int decodedLength = decode_base64(base64Data, imageData);
      sleepTimeSec = doc["nextUpdateSec"].as<unsigned long>();
      log_i("Decoded %i, sleep %i", decodedLength, sleepTimeSec);
    } else {
      errorStatus = "bad decode";
      log_e("JSON error: %s", error.c_str());
    }
  }

  if (errorStatus != NULL) {
    failureCount++;
    log_e("Failure %d, error: %s", failureCount, errorStatus);
  } else {
    failureCount = 0;
  }

  // DISPLAY RENDERING CODE
  //
  display.setRotation(3);
  if (errorStatus == NULL) {  
    display.firstPage();
    do {
      int rc = png.openRAM((uint8_t *)imageData, sizeof(imageData), PNGDraw);
      if (rc == PNG_SUCCESS) {
        rc = png.decode(NULL, 0);
        png.close();
      }    
    } while (display.nextPage());
  } else if (failureCount >= kMaxErrorCount) {
    display.firstPage();
    do {
      // center error text on the screen
      int16_t tbx, tby; uint16_t tbw, tbh;
      display.getTextBounds(errorStatus, 0, 0, &tbx, &tby, &tbw, &tbh);
      uint16_t x = (display.width() - tbw) / 2;
      uint16_t y = (display.height() - tbh) / 2;

      display.setTextColor(GxEPD_RED);
      display.setFont(&FreeMonoBold9pt7b);
      display.setCursor(x, y);
      display.print(errorStatus);
    } while (display.nextPage());
  }
  display.hibernate();
  log_i("Display done");

  digitalWrite(kLedR, 0);
  digitalWrite(kLedB, 0);

  // put device to sleep
  if (failureCount >= kMaxErrorCount) {
    sleepTimeSec = kErrSleepSec;
  } else if (errorStatus != NULL) {
    sleepTimeSec = kRetrySleepSec;
  }
  
  if (sleepTimeSec == 0) {
    sleepTimeSec = 60 * 60;  // default one hour
  }

  esp_sleep_enable_timer_wakeup(sleepTimeSec * 1000000);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_deep_sleep_start();
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(kLedG, 0);
  delay(100);
  digitalWrite(kLedG, 1);
  delay(100);
}
