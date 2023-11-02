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

// ESP32-C6 variant
const int kSw0Pin = 11;
const int kSw1Pin = 10;
const int kSw2Pin = 8;
const int kOledRstPin = 3;
const int kOledDcPin = 22;
const int kOledCsPin = 21;
const int kEpdBusyPin = 15;

const int kOledSckPin = 23;
const int kOledMosiPin = 20;  // NOTE - needs a jumper, otherwise pad on PCB is NC!

const int kLedR = 7;
const int kLedG = 0;
const int kLedB = 1;


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

SPIClass hspi(HSPI);  // for ESP32




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
  pinMode(kLedR, OUTPUT);
  pinMode(kLedG, OUTPUT);
  pinMode(kLedB, OUTPUT);

  digitalWrite(kLedR, 1);
  digitalWrite(kLedG, 0);
  digitalWrite(kLedB, 0);

  hspi.begin(kOledSckPin, -1, kOledMosiPin, -1);
  display.epd2.selectSPI(hspi, SPISettings(4000000, MSBFIRST, SPI_MODE0));
  display.init(115200);
  digitalWrite(kLedR, 0);
  einkHelloWorld();
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
