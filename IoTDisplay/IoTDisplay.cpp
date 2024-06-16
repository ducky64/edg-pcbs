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
#include <Update.h>
#include "esp_ota_ops.h"  // get current partition
#include "WifiConfig.h"  // must define 'const char* ssid' and 'const char* password' and 'const char* kHttpServer'
// ssid and password are self-explanatory, http server is the IP address to the base , eg "http://10.0.0.2"
const char* kRenderPostfix = "/render";
const char* kMetadataPostfix = "/meta";  // URL postfix to get metadata JSON, incl time to next update
const char* kImagePostfix = "/image";  // URL postfix to get image to render
const char* kOtaPostfix = "/ota";  // URL postfix to get OTA firmware binary
const size_t kMaxChunk = 2048;  // process data in smaller chunks, so the loop polls regularly for eg LEDs

#include <PNGdec.h>
PNG png;


uint8_t streamData[32768] = {0};  // allocate in static memory, contains PNG image data
StaticJsonDocument<256> doc;

size_t maxWidth = 480;


const char* kFwVerStr = "4";

RTC_DATA_ATTR int bootCount = 0;
RTC_DATA_ATTR int failureCount = 0;

const int kMaxWifiConnectSec = 30;  // max to wait for wifi to come up before sleeping
const int kRetrySleepSec = 120;  // on error, how long to wait to retry
const int kMaxErrorCount = 3;  // number of consecutive network failures before displaying error
const int kErrSleepSec = 3600;  // on exceeding max errors, how long to wait until next attempt


void PNGDraw(PNGDRAW *pDraw) {
  uint16_t usPixels[maxWidth];
  uint8_t ucMask[maxWidth/8];
  
  png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  png.getAlphaMask(pDraw, ucMask, 127);

  for (size_t i=0; i<pDraw->iWidth; i++) {
    if (((usPixels[i] & 0x001f) < 0x10) && (((usPixels[i] & 0xf800) >> 11) < 0x10)
        && ((ucMask[i/8] >> ((7-i) % 8)) & 0x1)) {  // darker than grey
      display.drawPixel(i, pDraw->y, GxEPD_BLACK);
    } else if (((usPixels[i] & 0x001f) > 0x10) && (((usPixels[i] & 0xf800) >> 11) < 0x10) 
        && ((ucMask[i/8] >> ((7-i) % 8)) & 0x1)) {  // red
      display.drawPixel(i, pDraw->y, GxEPD_RED);
    }
  }
}

void setup() {
  bootCount++;

  const char* resetReason = "";
  switch (esp_reset_reason()) {
      case ESP_RST_POWERON: resetReason = "POWERON"; break;
      case ESP_RST_SW: resetReason = "SW"; break;
      case ESP_RST_PANIC: resetReason = "PANIC"; break;
      case ESP_RST_INT_WDT: resetReason = "INT_WDT"; break;
      case ESP_RST_WDT: resetReason = "WDT"; break;
      case ESP_RST_DEEPSLEEP: resetReason = "DEEPSLEEP"; break;
      case ESP_RST_BROWNOUT: resetReason = "BROWNOUT"; break;
      case ESP_RST_UNKNOWN: resetReason = "UNKNOWN"; break;
      default: resetReason = "other"; break;
      break;
  }

  const esp_partition_t* bootPartition = esp_ota_get_boot_partition();

  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_INFO);
  const char* errorStatus = NULL;  // set when an error occurs, to a short descriptive string

  setCpuFrequencyMhz(80);  // downclock to reduce power draw

  log_i("Boot %d, %s, part=%s", bootCount, resetReason, bootPartition->label);

  pinMode(kLedR, OUTPUT);
  pinMode(kLedG, OUTPUT);
  pinMode(kLedB, OUTPUT);
  pinMode(kVsenseGate, OUTPUT);

  digitalWrite(kLedR, 0);
  digitalWrite(kLedG, 0);
  digitalWrite(kLedB, 0);


  digitalWrite(kVsenseGate, 1);
  delay(2);
  int vbatMv = (uint32_t)analogRead(kVsense) * 3300 * (47+10) / 10 / 4096;
  log_i("Vbat: %d", vbatMv);
  digitalWrite(kVsenseGate, 0);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[13];
  sprintf(macStr, "%02x%02x%02x", mac[3], mac[4], mac[5]);

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
    digitalWrite(kLedR, millis() % 200 >= 100);
    if ((millis() - timeStartWifi) > kMaxWifiConnectSec * 1000) {
      errorStatus = "no wifi";
      break;
    }
  }
  long int timeConnectWifi = millis();
  log_i("Connected WiFi: %.1fs, %s, RSSI=%i", (float)(timeConnectWifi - timeStartWifi) / 1000, WiFi.localIP().toString(), WiFi.RSSI());
  digitalWrite(kLedR, 1);

  // fetch metadata
  unsigned long sleepTimeSec = 0;
  bool runOta = false;
  if (errorStatus == NULL) {
    long int timeStartGet = millis();
    HTTPClient http;
    http.useHTTP10(true);  // disabe chunked encoding, since the stream doesn't remove metadata
    http.setTimeout(15*1000);
    String httpUrl = (String) kHttpServer + kMetadataPostfix +
        "?mac=" + macStr + "&vbat=" + vbatMv + "&fwVer=" + kFwVerStr +
        "&boot=" + bootCount + "&rst=" + resetReason + "&part=" + bootPartition->label;
    if (failureCount > 0) {
      httpUrl = httpUrl + "&fail=" + failureCount;
    }
    http.begin(httpUrl);
    int httpResponseCode = http.GET();
    int httpResponseLen = http.getSize();

    log_i("Meta: GET: %i (%i KiB) <= %s", httpResponseCode, httpResponseLen / 1024, httpUrl.c_str());
    if (httpResponseCode == 200) {
      WiFiClient* stream = http.getStreamPtr();
      uint8_t* streamDataPtr = streamData;
      while(http.connected()) {
        size_t streamAvailable = stream->available();
        size_t bufferLeft = sizeof(streamData) - (streamDataPtr - streamData);
        int c = stream->readBytes(streamDataPtr, min(streamAvailable, min(bufferLeft, kMaxChunk)));
        streamDataPtr += c;

        log_d("  %i KiB", (size_t)(streamDataPtr - streamData) / 1024);
        if (bufferLeft <= 0) {
          break;
        }
      }
      http.end();
      log_i("Meta: got %i", streamDataPtr - streamData);

      DeserializationError error = deserializeJson(doc, streamData);
      if (!error) {
        sleepTimeSec = doc["nextUpdateSec"].as<unsigned long>();
        runOta = doc["ota"].as<bool>();
        log_i("Meta: Deserialized: sleep %d, runOta %i", sleepTimeSec, runOta);
      } else {
        errorStatus = "bad meta deserialize";
        log_e("Meta: JSON error: %s", error.c_str());
      }
    } else {
      errorStatus = "meta response error";
    }
  }

  if (errorStatus == NULL && runOta) {
    log_i("Ota: start");

    String httpUrl = (String) kHttpServer + kOtaPostfix + "?mac=" + macStr;
    if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
      log_e("Ota: Update.begin == false");
    } else {
      long int timeStartGet = millis();
      HTTPClient http;
      http.useHTTP10(true);  // disabe chunked encoding, since the stream doesn't remove metadata
      http.setTimeout(15*1000);
      String httpUrl = (String) kHttpServer + kOtaPostfix + "?mac=" + macStr;
      http.begin(httpUrl);
      int httpResponseCode = http.GET();
      int httpResponseLen = http.getSize();

      log_i("Ota: GET: %i (%i KiB) <= %s", httpResponseCode, httpResponseLen / 1024, httpUrl.c_str());
      size_t otaBytes = 0, otaWritten = 0;
      if (httpResponseCode == 200) {
        WiFiClient* stream = http.getStreamPtr();
        while(http.connected()) {
          size_t streamAvailable = stream->available();
          int c = stream->readBytes(streamData, min(streamAvailable, min(sizeof(streamData), kMaxChunk)));
          otaBytes += c;

          size_t chunkOtaWritten = Update.write(streamData, c);
          otaWritten += chunkOtaWritten;
          if (chunkOtaWritten != c) {
            log_e("Ota: bytes unwritten, %i / %i", chunkOtaWritten, otaBytes);
            break;
          }
          log_d("  %i KiB", otaBytes / 1024);
          digitalWrite(kLedG, millis() % 200 >= 100);
        }
        http.end();
        log_i("Ota: got %i, wrote %i", otaBytes, otaWritten);
        
        if (otaWritten == otaBytes) {
          if (Update.end(true)) {  // evenIfRemaining set b/c the size is not known ahead of time
            log_i("Ota: success, rebooting");
            delay(10);
            ESP.restart();
          } else {
            log_i("Ota: failed: %s", Update.errorString());
          }
        }
      } else {
        log_e("Ota: response error %i", httpResponseCode);
      }
    }
    digitalWrite(kLedG, 0);
  }

  // fetch image data
  if (errorStatus == NULL) {
    long int timeStartGet = millis();
    HTTPClient http;
    http.useHTTP10(true);  // disabe chunked encoding, since the stream doesn't remove metadata
    http.setTimeout(15*1000);
    String httpUrl = (String) kHttpServer + kImagePostfix + "?mac=" + macStr;
    http.begin(httpUrl);
    int httpResponseCode = http.GET();
    int httpResponseLen = http.getSize();

    log_i("Image: GET: %i (%i KiB) <= %s", httpResponseCode, httpResponseLen / 1024, httpUrl.c_str());
    if (httpResponseCode == 200) {
      WiFiClient* stream = http.getStreamPtr();
      uint8_t* streamDataPtr = streamData;
      while(http.connected()) {
        size_t streamAvailable = stream->available();
        size_t bufferLeft = sizeof(streamData) - (streamDataPtr - streamData);
        int c = stream->readBytes(streamDataPtr, min(streamAvailable, min(bufferLeft, kMaxChunk)));
        streamDataPtr += c;

        log_d("  %i KiB", (size_t)(streamDataPtr - streamData) / 1024);
        if (bufferLeft <= 0) {
          break;
        }
      }
      http.end();
      log_i("Image: Got %i", streamDataPtr - streamData);
    } else {
      errorStatus = "image response error";
    }
  }

  // done with all network tasks, stop wifi to save power
  WiFi.disconnect();
  if (esp_wifi_stop() != ESP_OK) {
    log_e("Failed disable WiFi");
  }
  long int timeStopWifi = millis();
  log_i("Network active: %.1fs", (float)(timeStopWifi - timeStartWifi) / 1000);
  digitalWrite(kLedR, 0);
  digitalWrite(kLedB, 1);

  if (errorStatus != NULL) {
    failureCount++;
    log_e("Failure %d, error: %s", failureCount, errorStatus);
  } else {
    failureCount = 0;
  }

  // DISPLAY RENDERING CODE
  //
  display.setRotation(3);
  display.setFont(&FreeMonoBold9pt7b);

  // last character gets cut off for some reason
  String selfData = String(kFwVerStr) + " " + macStr + " " + vbatMv / 1000 + "." + (vbatMv % 1000 / 10) + "V  ";
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(selfData, 0, 0, &tbx, &tby, &tbw, &tbh);

  if (errorStatus == NULL) {  
    display.firstPage();
    do {
      int rc = png.openRAM((uint8_t *)streamData, sizeof(streamData), PNGDraw);
      if (rc == PNG_SUCCESS) {
        rc = png.decode(NULL, 0);
        png.close();
      }

      display.setTextColor(GxEPD_BLACK);
      display.setCursor(display.width() - tbw, display.height() - tbh);
      display.print(selfData);
    } while (display.nextPage());
  } else if (failureCount >= kMaxErrorCount) {
    display.firstPage();
    do {
      display.setTextColor(GxEPD_BLACK);
      display.setCursor(display.width() - tbw, display.height() - tbh);
      display.print(selfData);

      display.setTextColor(GxEPD_RED);
      display.getTextBounds(errorStatus, 0, 0, &tbx, &tby, &tbw, &tbh);
      display.setCursor((display.width() - tbw) / 2, (display.height() - tbh) / 2);
      display.print(errorStatus);
    } while (display.nextPage());
  }
  display.hibernate();
  log_i("Display done");

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

  esp_sleep_enable_timer_wakeup(sleepTimeSec * 1000000ull);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_deep_sleep_start();
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(kLedB, 0);
  delay(100);
  digitalWrite(kLedB, 1);
  delay(100);
}
