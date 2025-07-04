#include <Arduino.h>
#include <ArduinoJson.h>
#include "base64.hpp"
#include <SPI.h>

#include "esp_sleep.h"
#include "esp_adc_cal.h"
#include "esp_task_wdt.h"

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

const int kEpdGate = 17;
const int kMemGate = 21;

SPIClass spi(HSPI);  // for ESP32S3


const int kBlinkIntervalMs = 250;
const int kBusyBlinkIntervalMs = 1000;


#include <GxEPD2_BW.h>
#include <GxEPD2_3C.h>
#include <GxEPD2_7C.h>
#include <Fonts/FreeSans9pt7b.h>
const GFXfont &kFont = FreeSans9pt7b;

// Full displays list at
// https://github.com/ZinggJM/GxEPD2/tree/master/examples/GxEPD2_Example
#ifdef DISPLAY_565C
  GxEPD2_7C<GxEPD2_565c, GxEPD2_565c::HEIGHT/2> display(GxEPD2_565c(kEpdCsPin, kEpdDcPin, kEpdRstPin, kEpdBusyPin)); // Waveshare 5.65" 7-color, BROKEN
#elif DISPLAY_750C_Z08
  GxEPD2_3C<GxEPD2_750c_Z08, GxEPD2_750c_Z08::HEIGHT> display(GxEPD2_750c_Z08(kEpdCsPin, kEpdDcPin, kEpdRstPin, kEpdBusyPin)); // Waveshare 3C 7.5" B
#elif DISPLAY_1330C_GDEM133Z91
  GxEPD2_3C<GxEPD2_1330c_GDEM133Z91, GxEPD2_1330c_GDEM133Z91::HEIGHT/2> display(GxEPD2_1330c_GDEM133Z91(kEpdCsPin, kEpdDcPin, kEpdRstPin, kEpdBusyPin)); // Waveshare 3C 13.3" B
#else
  static_assert(false, "no display defined");
#endif
const size_t kMaxWidth = 680;

const int kMinDisplayGoodMs = 15000;  // minimum time the display should take for a full refresh to be considered good
const int kMaxDisplayGoodMs = 45000;  // maximum time the display should take for a full refresh to be considered good

#include "esp_wifi.h"  // support wifi stop
#include <WiFi.h>
#include <HTTPClient.h>
#include "WifiConfig.h"  // must define 'const char* ssid' and 'const char* password' and 'const char* kHttpServer'
// ssid and password are self-explanatory, http server is the IP address to the base , eg "http://10.0.0.2"

const char* kMetadataPostfix = "/meta";  // URL postfix to get metadata JSON, incl time to next update
const char* kImagePostfix = "/image";  // URL postfix to get image to render
const char* kOtaPostfix = "/ota";  // URL postfix to get OTA firmware binary
const size_t kMaxChunk = 2048;  // process data in smaller chunks, so the loop polls regularly for eg LEDs


#include <Update.h>
#include "esp_ota_ops.h"  // get current partition
extern "C" bool verifyRollbackLater(){ return true; }  // rollback is manually verified


#include <PNGdec.h>
PNG png;


uint8_t streamData[32768] = {0};  // allocate in static memory, contains PNG image data
StaticJsonDocument<256> doc;


const char* kFwVerStr = "12";

// these are RTC_NOINIT_ATTR to survive a WDT reset
// they are manually init'd to 0 post-reset depending on the reset reason
RTC_NOINIT_ATTR int bootCount;
RTC_NOINIT_ATTR int failureCount;
RTC_DATA_ATTR int lastDisplayTimeMsec = 0;

const int kMaxWifiConnectSec = 20;  // max to wait for wifi to come up before sleeping
const int kRetrySleepSec = 120;  // on error, how long to wait to retry
const int kMaxErrorCount = 3;  // number of consecutive network failures before displaying error
const int kErrSleepSec = 3600;  // on exceeding max errors, how long to wait until next attempt

const int kBusyGraceMsec = 25000;


void ledWrite(int ledPin, bool state) {
  if (state) {
    pinMode(ledPin, INPUT_PULLUP);
  } else {
    pinMode(ledPin, INPUT_PULLDOWN);
  }
}


void PNGDraw(PNGDRAW *pDraw) {
  if (pDraw->iWidth > kMaxWidth) {  // would create a buffer overflow
    for (size_t i=0; i<pDraw->iWidth; i+=2) {
      display.drawPixel(i, pDraw->y, GxEPD_RED);
    }
    return;
  } 

  uint16_t usPixels[kMaxWidth];
  uint8_t ucMask[kMaxWidth/8];
  
  png.getLineAsRGB565(pDraw, usPixels, PNG_RGB565_BIG_ENDIAN, 0xffffffff);
  png.getAlphaMask(pDraw, ucMask, 127);

  for (size_t i=0; i<pDraw->iWidth; i++) {
    if (!((ucMask[i/8] >> ((7-i) % 8)) & 0x1)) {
      continue;  // alpha - masked out
    }
    if (((usPixels[i] & 0x001f) < 0x04) && (((usPixels[i] >> 11) & 0x001f) < 0x04)) {  // black
      display.drawPixel(i, pDraw->y, GxEPD_BLACK);
    } else if (((usPixels[i] & 0x001f) > 0x10) && (((usPixels[i] >> 11) & 0x001f) < 0x04)) {  // red
      display.drawPixel(i, pDraw->y, GxEPD_RED);
    } else if (((usPixels[i] & 0x001f) < 0x1d) && (((usPixels[i] >> 11) & 0x001f) < 0x1d)) {  // mid-grey
      if ((i % 2) == (pDraw->y % 2)) {  // dithering
        display.drawPixel(i, pDraw->y, GxEPD_BLACK);
      }
    }
  }
}

void busyCallback(const void*) {
  esp_task_wdt_reset();
  ledWrite(kLedG, millis() % kBusyBlinkIntervalMs <= kBlinkIntervalMs/2);
  esp_sleep_enable_timer_wakeup(kBlinkIntervalMs/4 * 1000ull);
  esp_light_sleep_start();
}

uint16_t analogReadCalibratedMv(int pin, adc_attenuation_t atten = ADC_11db) {
  analogSetPinAttenuation(pin, atten);
  uint16_t rawAdc = analogReadRaw(pin);

  adc_atten_t calAtten = ADC_ATTEN_DB_12;
  switch (atten) {
    case ADC_11db: calAtten = ADC_ATTEN_DB_12; break;
    case ADC_6db: calAtten = ADC_ATTEN_DB_6; break;
    case ADC_2_5db: calAtten = ADC_ATTEN_DB_2_5; break;
    case ADC_0db: calAtten = ADC_ATTEN_DB_0; break;
  }

  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, calAtten, ADC_WIDTH_BIT_12, 0, &adc_chars);
  uint16_t adcMv = esp_adc_cal_raw_to_voltage(rawAdc, &adc_chars);
  //Check type of calibration value used to characterize ADC
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
      log_i("eFuse Vref, %d => %d", rawAdc, adcMv);
  } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
      log_i("eFuse TP, %d => %d", rawAdc, adcMv);
  } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP_FIT) {
      log_i("eFuse TP Fit, %d => %d", rawAdc, adcMv);
  } else {
      log_i("Default, %d => %d", rawAdc, adcMv);
  }
  return adcMv;
}

void setup() {
  setCpuFrequencyMhz(80);  // downclock to reduce power draw
  esp_task_wdt_init(5, true);
  esp_task_wdt_add(NULL);

  const char* errorStatus = NULL;  // set when an error occurs, to a short descriptive string

  const char* resetReason = "";
  switch (esp_reset_reason()) {
      case ESP_RST_POWERON: resetReason = "POWERON"; break;
      case ESP_RST_SW: resetReason = "SW"; break;
      case ESP_RST_PANIC: resetReason = "PANIC"; break;
      case ESP_RST_INT_WDT: resetReason = "INT_WDT"; break;
      case ESP_RST_TASK_WDT: resetReason = "TASK_WDT"; break;
      case ESP_RST_WDT: resetReason = "WDT"; break;
      case ESP_RST_DEEPSLEEP: resetReason = "DEEPSLEEP"; break;
      case ESP_RST_BROWNOUT: resetReason = "BROWNOUT"; break;
      case ESP_RST_UNKNOWN: resetReason = "UNKNOWN"; break;
      default: resetReason = "other"; break;
  }

  switch (esp_reset_reason()) {
      case ESP_RST_INT_WDT:
      case ESP_RST_TASK_WDT:
      case ESP_RST_WDT:
        bootCount++;
        failureCount++;
        if (failureCount >= 3) {
          errorStatus = "wdt";
        }
        break;
      case ESP_RST_PANIC:      
      case ESP_RST_BROWNOUT:
        bootCount++;
        failureCount++;
        break;
      case ESP_RST_DEEPSLEEP:
        bootCount++;
        break;
      default: 
        bootCount = 0;
        failureCount = 0;      
  }

  const int savedLastDisplayTimeMsec = lastDisplayTimeMsec;  // store and clear
  lastDisplayTimeMsec = 0;

  const esp_partition_t* bootPartition = esp_ota_get_boot_partition();

  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_INFO);

  log_i("Boot %d (fail %d), %s, part=%s %s", bootCount, failureCount, resetReason, 
      bootPartition->label, esp_ota_check_rollback_is_possible() ? "R" : "");

  pinMode(kLedR, OUTPUT);
  pinMode(kLedG, OUTPUT);
  pinMode(kLedB, OUTPUT);
  pinMode(kVsenseGate, OUTPUT);
  pinMode(kEpdGate, OUTPUT);
  pinMode(kMemGate, OUTPUT);

  ledWrite(kLedR, 0);
  ledWrite(kLedG, 0);
  ledWrite(kLedB, 0);
  gpio_hold_dis((gpio_num_t)kEpdGate);
  gpio_hold_dis((gpio_num_t)kMemGate);
  digitalWrite(kEpdGate, 1);  // start off
  digitalWrite(kMemGate, 1);

  // SENSE VOLTAGE
  //
  gpio_hold_dis((gpio_num_t)kVsenseGate);
  digitalWrite(kVsenseGate, 1);
  delay(5);  // wait for vsense to stabilize
  uint16_t vBatAdcMv = analogReadCalibratedMv(kVsense, ADC_11db);
  adc_attenuation_t atten = ADC_11db;
  if (vBatAdcMv < 1100) {  // lower attenuation ranges have lower errors, re-sample in a lower range
    atten = ADC_2_5db;
  } else if (vBatAdcMv < 1600) {
    atten = ADC_6db;
  }
  uint32_t vbatMv = 0;
  for (size_t i=0; i<8; i++) {  // average for stability
    delay(1);
    vbatMv += analogReadCalibratedMv(kVsense, atten);
  }
  vbatMv = vbatMv / 8 * (47+10) / 10;
  log_i("Vbat: %d mV", vbatMv);
  digitalWrite(kVsenseGate, 0);

  uint8_t mac[6];
  WiFi.macAddress(mac);
  char macStr[13];
  sprintf(macStr, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);

  log_i("Total heap: %d, PSRAM: %d", ESP.getHeapSize(), ESP.getPsramSize());
  ledWrite(kLedR, 0);
  ledWrite(kLedG, 0);
  ledWrite(kLedB, 0);

  // NETWORK CODE
  //
  esp_task_wdt_reset();
  int rssi = 0;
  long int timeStartWifi = millis();
  if (errorStatus == NULL) {
    WiFi.begin(ssid, password);
    while(WiFi.status() != WL_CONNECTED) {
      esp_task_wdt_reset();
      ledWrite(kLedR, millis() % kBlinkIntervalMs <= kBlinkIntervalMs/2);
      if ((millis() - timeStartWifi) > kMaxWifiConnectSec * 1000) {
        errorStatus = "no wifi";
        break;
      }
      delay(1);
      yield();
    }
    long int timeConnectWifi = millis();
    rssi = WiFi.RSSI();
    log_i("Connected WiFi: %.1fs, %s, RSSI=%i", (float)(timeConnectWifi - timeStartWifi) / 1000, WiFi.localIP().toString(), rssi);
    ledWrite(kLedR, 1);
  }

  // fetch metadata
  unsigned long sleepTimeSec = 0;
  bool runOta = false;
  if (errorStatus == NULL) {
    esp_task_wdt_reset();
    long int timeStartGet = millis();
    HTTPClient http;
    http.useHTTP10(true);  // disabe chunked encoding, since the stream doesn't remove metadata
    http.setTimeout(15*1000);
    String httpUrl = (String) kHttpServer + kMetadataPostfix +
        "?mac=" + macStr + "&vbat=" + vbatMv + "&fwVer=" + kFwVerStr +
        "&boot=" + bootCount + "&rst=" + resetReason + "&part=" + bootPartition->label +
        "&rssi=" + rssi;
    if (savedLastDisplayTimeMsec > 0) {
      httpUrl = httpUrl + "&lastDisplayTime=" + savedLastDisplayTimeMsec;
    }
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
        esp_task_wdt_reset();
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
  long int timeMetaDone = millis();

  if (errorStatus == NULL && runOta) {
    esp_task_wdt_reset();
    log_i("Ota: start");
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
          esp_task_wdt_reset();
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
          ledWrite(kLedB, millis() % 200 >= 100);
        }
        http.end();
        log_i("Ota: got %i, wrote %i", otaBytes, otaWritten);
        
        if (otaWritten == otaBytes) {
          if (Update.end(true)) {  // evenIfRemaining set b/c the size is not known ahead of time
            log_i("Ota: success, rebooting");
            delay(5);
            ESP.restart();
          } else {
            log_i("Ota: failed: %s", Update.errorString());
          }
        }
      } else {
        log_e("Ota: response error %i", httpResponseCode);
      }
    }
    ledWrite(kLedB, 0);
  }

  // fetch image data
  if (errorStatus == NULL) {
    esp_task_wdt_reset();
    long int timeStartGet = millis();
    HTTPClient http;
    http.useHTTP10(true);  // disable chunked encoding, since the stream doesn't remove metadata
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
        esp_task_wdt_reset();
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
  esp_task_wdt_reset();
  WiFi.disconnect();
  if (esp_wifi_stop() != ESP_OK) {
    log_e("Failed disable WiFi");
  }
  long int timeStopWifi = millis();
  log_i("Network active: %.1fs", (float)(timeStopWifi - timeStartWifi) / 1000);
  ledWrite(kLedR, 0);
  ledWrite(kLedG, 1);

  // DISPLAY RENDERING CODE
  //
  esp_task_wdt_reset();
  digitalWrite(kEpdGate, 0);  // turn on display
  delay(5);  // wait for power to stabilize
  spi.begin(kEpdSckPin, -1, kEpdMosiPin, -1);
  display.epd2.selectSPI(spi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(0);

  display.epd2.setBusyCallback(&busyCallback);

  display.setRotation(3);
  display.setFont(&kFont);

  // last character gets cut off for some reason
  char shortMacStr[13];
  sprintf(shortMacStr, "%02x%02x%02x", mac[3], mac[4], mac[5]);
  char voltageStr[7];
  sprintf(voltageStr, "%.2f", (float)vbatMv / 1000);
  String selfData = String(kFwVerStr) + " " + String(shortMacStr) + " " + voltageStr + "v  ";
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(selfData, 0, 0, &tbx, &tby, &tbw, &tbh);

  long int timeStartDisplay = millis();
  if (errorStatus == NULL) {  
    log_i("No errors");
    failureCount = 0;

    log_i("Display: show image");
    display.firstPage();
    do {
      int rc = png.openRAM((uint8_t *)streamData, sizeof(streamData), PNGDraw);
      if (rc == PNG_SUCCESS) {
        rc = png.decode(NULL, 0);
        png.close();
      }

      display.setTextColor(GxEPD_BLACK);
      display.setCursor(display.width() - tbw, display.height() - 1 - kFont.glyph[0].yOffset);
      display.print(selfData);
    } while (display.nextPage());
  } else {  
    failureCount++;
    log_e("Failure %d, error: %s", failureCount, errorStatus);

    if (failureCount >= kMaxErrorCount) {
      log_i("Display: show error");
      display.firstPage();
      do {
        display.setTextColor(GxEPD_BLACK);
        display.setCursor(display.width() - tbw, display.height() - 1 - kFont.glyph[0].yOffset);
        display.print(selfData);

        display.setTextColor(GxEPD_RED);
        display.getTextBounds(errorStatus, 0, 0, &tbx, &tby, &tbw, &tbh);
        display.setCursor((display.width() - tbw) / 2, (display.height() - tbh) / 2);
        display.print(errorStatus);
      } while (display.nextPage());
    }
  }

  // allow extra grace period for EPD BUSY signal
  const long int timeEndGrace = millis() + kBusyGraceMsec;
  while (millis() < timeEndGrace && digitalRead(kEpdBusyPin) == 1) {
    esp_task_wdt_reset();
    bool ledOn = millis() % kBusyBlinkIntervalMs <= kBlinkIntervalMs/2;
    bool altOn = millis() % (kBusyBlinkIntervalMs*2) <= kBusyBlinkIntervalMs;
    ledWrite(kLedR, ledOn & altOn);
    ledWrite(kLedG, ledOn & !altOn);
    esp_sleep_enable_timer_wakeup(kBlinkIntervalMs/4 * 1000ull);
    esp_light_sleep_start();
  }

  long int timeDisplay = millis() - timeStartDisplay;
  lastDisplayTimeMsec = timeDisplay;
  log_i("Display done: %.1fs", (float)timeDisplay / 1000);
  if (timeDisplay < kMinDisplayGoodMs || timeDisplay > kMaxDisplayGoodMs) {
    errorStatus = "Display refresh unexpected time";
  }

  ledWrite(kLedR, 0);
  ledWrite(kLedG, 0);
  display.hibernate();

  // CHECK FOR ROLLBACK
  //
  esp_task_wdt_reset();
  if (errorStatus == NULL && esp_ota_check_rollback_is_possible()) {
    log_i("Ota: validate");
    esp_ota_mark_app_valid_cancel_rollback();
    esp_ota_erase_last_boot_app_partition();
  } else if (errorStatus != NULL && esp_ota_check_rollback_is_possible()) {
    log_i("Ota: rollback");
    esp_err_t rollbackStatus = esp_ota_mark_app_invalid_rollback_and_reboot();
    if (rollbackStatus != ESP_OK) {
      log_e("Ota: rollback failed: %i", rollbackStatus);
    }
  }

  // release pins (put into INPUT mode) before shutting off power
  spi.end();  // release SPI pins
  display.end();  // release CS, DC, RST
  delay(5);

  // put device to sleep
  digitalWrite(kEpdGate, 1);
  digitalWrite(kMemGate, 1);
  gpio_hold_en((gpio_num_t)kEpdGate);
  gpio_hold_en((gpio_num_t)kMemGate);
  gpio_hold_en((gpio_num_t)kVsenseGate);

  if (failureCount >= kMaxErrorCount) {
    sleepTimeSec = kErrSleepSec;
  } else if (errorStatus != NULL) {
    sleepTimeSec = kRetrySleepSec;
  }
  
  if (sleepTimeSec == 0) {
    sleepTimeSec = 60 * 60;  // default one hour
  } else {  // correct sleep for computational time
    int sleepCorrectSec = min((millis() - timeMetaDone) / 1000, sleepTimeSec);
    sleepTimeSec -= sleepCorrectSec;
  }

  if (sleepTimeSec < 1) {
    sleepTimeSec = 1;
  }

  log_i("Sleep %i s", sleepTimeSec);
  esp_sleep_enable_timer_wakeup(sleepTimeSec * 1000000ull);
  esp_sleep_pd_config(ESP_PD_DOMAIN_RTC_PERIPH, ESP_PD_OPTION_OFF);
  esp_deep_sleep_start();
}

void loop() {
  // put your main code here, to run repeatedly:
  ledWrite(kLedG, 0);
  delay(100);
  ledWrite(kLedG, 1);
  delay(100);
}
