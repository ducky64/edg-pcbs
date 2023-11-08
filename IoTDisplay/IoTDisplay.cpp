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

GxEPD2_3C<GxEPD2_290_C90c, GxEPD2_290_C90c::HEIGHT> display(GxEPD2_290_C90c(kOledCsPin, kOledDcPin, kOledRstPin, kEpdBusyPin));  // SSD1680, compatible w/ ER-EPD029-2R


#include "esp_wifi.h"  // support wifi stop
#include <WiFi.h>
#include <HTTPClient.h>
#include "WifiConfig.h"  // must define 'const char* ssid' and 'const char* password'
const char* kNtpServer = "time.google.com";
const char* kTimezone = "PST8PDT,M3.2.0,M11.1.0";  // https://github.com/nayarsystems/posix_tz_db/blob/master/zones.csv
const long utcOffsetSeconds = -8*3600;

// note, the non-HTTPS URL 301s to the HTTPS URL
const char* kIcsUrl = "https://calendar.google.com/calendar/ical/gv8rblqs5t8hm6br9muf9uo2f0%40group.calendar.google.com/public/basic.ics";


#include <uICAL.h>


const char kHelloWorld[] = "Hello World!";


void einkHelloWorld() {
  display.setRotation(1);
  display.setFont(&FreeMonoBold9pt7b);
  display.setTextColor(GxEPD_BLACK);
  int16_t tbx, tby; uint16_t tbw, tbh;
  display.getTextBounds(kHelloWorld, 0, 0, &tbx, &tby, &tbw, &tbh);
  // center the bounding box by transposition of the origin:
  uint16_t x = ((display.width() - tbw) / 2) - tbx;
  uint16_t y = ((display.height() - tbh) / 2) - tby;
  display.setFullWindow();
  display.firstPage();
  do
  {
    display.fillScreen(GxEPD_WHITE);
    display.setCursor(x, y);
    display.print(kHelloWorld);
  }
  while (display.nextPage());
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  esp_log_level_set("*", ESP_LOG_DEBUG);

  pinMode(kLedR, OUTPUT);
  pinMode(kLedG, OUTPUT);
  pinMode(kLedB, OUTPUT);

  digitalWrite(kLedR, 1);
  digitalWrite(kLedG, 0);
  digitalWrite(kLedB, 0);

  log_i("Total heap: %d, PSRAM: %d", ESP.getHeapSize(), ESP.getPsramSize());

  long int timeStartWifi = millis();
  log_i("Connect WiFi");
  WiFi.begin(ssid, password);
  while(WiFi.status() != WL_CONNECTED) {
    delay(500);
    log_i("...");
  }
  log_i("Connected WiFi: %s, RSSI=%i", WiFi.localIP().toString(), WiFi.RSSI());

  // see https://randomnerdtutorials.com/esp32-ntp-timezones-daylight-saving/
  long int timeStartNtp = millis();
  log_i("Sync NTP time");
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
  log_i("GET ICS");
  HTTPClient http;
  http.useHTTP10(true);  // disabe chunked encoding, since the stream doesn't remove metadata
  http.begin(kIcsUrl);
  int httpResponseCode = http.GET();
  int httpResponseLen = http.getSize();
  log_i("GET: %i (%i KiB) <= %s", httpResponseCode, httpResponseLen / 1024, kIcsUrl);

  try {
    uICAL::istream_Stream istm(http.getStream());
    uICAL::DateTime begin("20191016T102000Z");
    uICAL::DateTime end("20191017T103000Z");
    auto cal = uICAL::Calendar::load(istm, [=](const uICAL::VEvent& event){
        return event.start > begin && event.end < end;
    });
  
    auto calIt = uICAL::new_ptr<uICAL::CalendarIter>(cal, begin, end);
    while (calIt->next()) {
      uICAL::CalendarEntry_ptr entry = calIt->current();
      log_d("%s", entry->summary().c_str());
    }
  } catch (const uICAL::Error& err) {
    log_e("Error during parsing: %s", err.message.c_str());
  }

  http.end();

  // done with all network tasks, stop wifi to save power
  WiFi.disconnect();
  if (esp_wifi_stop() != ESP_OK) {
    log_e("Failed disable WiFi");
  } else {
    log_i("Disabled WiFi");
  }
  long int timeStopWifi = millis();
  log_i("Total network active time: %.1f", (float)(timeStopWifi - timeStartWifi) / 1000);

  // spi.begin(kOledSckPin, -1, kOledMosiPin, -1);
  // display.epd2.selectSPI(spi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  // display.init(115200);
  // digitalWrite(kLedR, 0);
  // einkHelloWorld();
  // digitalWrite(kLedG, 1);

  // display.hibernate();
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(kLedG, 0);
  delay(100);
  digitalWrite(kLedG, 1);
  delay(100);
}
