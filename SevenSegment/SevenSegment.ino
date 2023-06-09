#include <Freenove_WS2812_Lib_for_ESP32.h>

// pinning for ESP32E module

// "v5v_sense=ADC1_CH7, 7",
// "i2c=I2CEXT0",
// "i2c.scl=GPIO33, 9",
// "i2c.sda=GPIO32, 8",
// "sw0=GPIO18, 30",
// "sw1=GPIO19, 31",
// "sw3=GPIO21, 33",
// "rgb=GPIO27, 12",
// "spk=GPIO5, 29",

// int kPinLedRed = 14;
// int kPinLedGrn = 27;
// int kPinLedBlu = 12;
// int kPinSw = 13;

// int kPinOutput = 4;

int kPinSw0 = 18;
int kPinSw1 = 19;
int kPinSw3 = 21;
int kPinNeopixel = 27;

int kLedsPerSeg = 2;
int kOffsetDigit1 = 0;
int kOffsetDigit2 = 14;
int kOffsetMeta = 28;
int kOffsetColon = 30;
int kOffsetDigit3 = 32;
int kOffsetDigit4 = 46;
int kLedsCount = kOffsetDigit4 + 2 * 7;


Freenove_ESP32_WS2812 Neopixels(kLedsCount, kPinNeopixel, 0, TYPE_GRB);


void hsv_to_rgb_uint8(uint16_t h_cdeg, uint8_t s, uint8_t v,
    uint8_t *r_out, uint8_t *g_out, uint8_t *b_out) {
  uint8_t C = (uint16_t)v * s / 255;
  int32_t h_millipct = (((int32_t)h_cdeg * 10 / 60) % 2000) - 1000;  // 1000x
  uint16_t X = C * (1000 - abs(h_millipct)) / 1000;
  uint16_t m = v - C;

  h_cdeg = h_cdeg % 36000;
  if (0 <= h_cdeg && h_cdeg < 6000) {
    *r_out = C;  *g_out = X;  *b_out = 0;
  } else if (6000 <= h_cdeg && h_cdeg < 12000) {
    *r_out = X;  *g_out = C;  *b_out = 0;
  } else if (12000 <= h_cdeg && h_cdeg < 18000) {
    *r_out = 0;  *g_out = C;  *b_out = X;
  } else if (18000 <= h_cdeg && h_cdeg < 24000) {
    *r_out = 0;  *g_out = X;  *b_out = C;
  } else if (24000 <= h_cdeg && h_cdeg < 30000) {
    *r_out = X;  *g_out = 0;  *b_out = C;
  } else {  // 300 <= H < 360
    *r_out = C;  *g_out = 0;  *b_out = X;
  }
  *r_out = *r_out + m;
  *g_out = *g_out + m;
  *b_out = *b_out + m;
}


void setup() {
    Neopixels.begin();
    Neopixels.setBrightness(25);

    // start up with all lights white
    for (int i = 0; i < kLedsCount; i++) {
      Neopixels.setLedColorData(i, 255, 255, 255);
    }
    Neopixels.show();

    delay(500);
}

int counter = 0;


void loop() {
  while(!digitalRead(kPinSw0) || !digitalRead(kPinSw1) || !digitalRead(kPinSw3));  // freeze when switch is down

  for (int i = 0; i < kLedsCount; i++) {
    uint8_t r, g, b;
    hsv_to_rgb_uint8((counter + i) * 90, 255, 255, &r, &g, &b);
    Neopixels.setLedColorData(i, r, g, b);
  }
  Neopixels.show();
  counter = counter + 1;

  delay(25);
}
