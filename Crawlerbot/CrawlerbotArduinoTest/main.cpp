// pin assigns from HDL
// "servo0_fb=ADC1_CH1, 38",
// "servo1_fb=ADC1_CH0, 39",
// "servo2_fb=ADC1_CH4, 5",
// "servo3_fb=ADC1_CH6, 7",
// "i2c=I2CEXT0",
// "i2c.scl=GPIO17, 10",
// "i2c.sda=GPIO16, 9",
// "led=GPIO40, 33",
// "oled_reset=GPIO15, 8",
// "servo0=GPIO41, 34",
// "servo1=GPIO42, 35",
// "servo2=GPIO4, 4",
// "servo3=GPIO6, 6",
// "rgb=GPIO39, 32",
// "cam=DVP",
// "cam.xclk=GPIO9, 17",
// "cam.pclk=GPIO12, 20",
// "cam.href=GPIO20, 14",
// "cam.vsync=GPIO19, 13",
// "cam.y0=GPIO14, 22",
// "cam.y1=GPIO47, 24",
// "cam.y2=GPIO48, 25",
// "cam.y3=GPIO21, 23",
// "cam.y4=GPIO13, 21",
// "cam.y5=GPIO11, 19",
// "cam.y6=GPIO10, 18",
// "cam.y7=GPIO3, 15",
// "srv_rst=GPIO8, 12"

int kPinLed = 40;
int kPinNeoPixel = 39;
int kPinI2cScl = 17;
int kPinI2cSda = 16;

#include <Wire.h>

#include <NeoPixelBrightnessBus.h>
const int kNeoPixelCount = 10;
NeoPixelBrightnessBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> NeoPixels(kNeoPixelCount, kPinNeoPixel);


void setup() {
  digitalWrite(kPinLed, 0);

  Serial.begin(115200);
  Serial.println("\r\n\n\nStart\r\n");

  Wire.begin(kPinI2cSda, kPinI2cScl);

  NeoPixels.Begin();
  NeoPixels.SetBrightness(32);

  pinMode(kPinLed, OUTPUT);
  digitalWrite(kPinLed, 1);
  Serial.println("Setup complete\r\n");
}

class PwmCoprocessor {
public:
  bool writeServoValue(uint8_t index, uint8_t value) {
    Wire.beginTransmission(kI2cAddress);
    Wire.write(index);  // servo index
    Wire.write(value);  // value
    return !Wire.endTransmission();
  }

  bool readServoFeedback(uint8_t index, uint16_t* valueOut) {
    Wire.beginTransmission(kI2cAddress);
    Wire.write(0x80);  // set read index
    Wire.write(index);  // read feedback servo index
    if (Wire.endTransmission()) {  // if failed
      return false;
    }

    Wire.requestFrom(kI2cAddress, 2);
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    if (Wire.endTransmission()) {
      return false;
    }
    *valueOut = (msb << 8) | lsb;
    return true;
  }

protected:
  const int kI2cAddress = 0x42;
};

PwmCoprocessor Stm32;

int servoUpdateIndex = 0;
int lastServoValueMillis = 0;
const int kServoUpdateMs = 500;

int lastFbReadMillis = 0;
const int kFbReadMs = 50;

void loop() {
  Serial.println("Loop\r\n");

  int thisMillis = millis();
  if (thisMillis - lastServoValueMillis >= kServoUpdateMs) {
    digitalWrite(kPinLed, 1);

    uint8_t servoValue = 0;  // rotate through servo positions
    if (servoUpdateIndex % 3 == 1) {
      servoValue = 127;
    } else if (servoUpdateIndex % 3 == 2) {
      servoValue = 255;
    }

    if (Stm32.writeServoValue(0, servoValue)) {
      NeoPixels.SetPixelColor(0, RgbColor(0, 255, 0));
    } else {  // failure
      NeoPixels.SetPixelColor(0, RgbColor(255, 0, 0));
    }
    digitalWrite(kPinLed, 0);
    NeoPixels.Show();

    servoUpdateIndex++;
    lastServoValueMillis = lastServoValueMillis + kServoUpdateMs;
  }

  if (thisMillis - lastFbReadMillis >= kFbReadMs) {
    uint16_t fbValue;
    if (Stm32.readServoFeedback(0, &fbValue)) {
      uint8_t ledValue = fbValue / 256;
      NeoPixels.SetPixelColor(4, RgbColor(ledValue, ledValue, ledValue));
    } else {
      NeoPixels.SetPixelColor(4, RgbColor(0, 0, 0));
    }
    NeoPixels.Show();
  }
}
