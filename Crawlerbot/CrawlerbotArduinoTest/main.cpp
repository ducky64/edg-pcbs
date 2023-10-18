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


const int kOledReset = 15;

class Ssd1357 {
public:
  bool begin() {
    pinMode(kOledReset, OUTPUT);
    digitalWrite(kOledReset, 0);
    delay(10);
    digitalWrite(kOledReset, 1);
    delay(10);

    bool success = true;
    success &= command(kCmdSetCommandLock, kDataUnlockOledDriver);
    success &= command(kCmdSetSleepModeOn);
    success &= command(kCmdSetRemap, 0x71, 0x00);  // 65k color depth
    success &= command(kCmdSetDisplayStart, 0x00);
    success &= command(kCmdSetDisplayOffset, 0x00);
    success &= command(kCmdSetResetPrecharge, 0x84);
    success &= command(kCmdSetClock, 0x20);  // 105Hz
    success &= command(kCmdSetSecondPrecharge, 0x01);
    success &= command(kCmdSetPrechargeVoltage, 0x00);
    success &= command(kCmdSetVcomh, 0x07);  // 0.86*VCC
    success &= command(kCmdMasterContrastCurrent, 0x0f);  // Vcc=15v
    success &= command(kCmdSetContrastCurrent, 0x32, 0x29, 0x53);  // 0.86*VCC
    success &= command(kCmdSetMuxRatio, 0x7f);
    success &= command(kCmdSetSleepModeOff);
    return success;
  }

  bool test() {
    bool success = true;
    for (uint8_t i=0; i<32; i++) {
      success &= drawPoint(0, i, 255, 0, 0);
      success &= drawPoint(1, i, 0, 255, 0);
      success &= drawPoint(2, i, 0, 0, 255);
      success &= drawPoint(3, i, 255, 255, 0);
      success &= drawPoint(4, i, 0, 255, 255);
      success &= drawPoint(5, i, 255, 0, 255);
      success &= drawPoint(6, i, 0, 0, 0);
      success &= drawPoint(7, i, 255, 255, 255);
    }
    return success;
  }

  bool command(uint8_t value) {
    return command_data(value, 0, NULL);
  }

  bool command(uint8_t value, uint8_t data0) {
    return command_data(value, 1, &data0);
  }

  bool command(uint8_t value, uint8_t data0, uint8_t data1) {
    uint8_t data[2];
    data[0] = data0;
    data[1] = data1;
    return command_data(value, 2, data);
  }

  bool command(uint8_t value, uint8_t data0, uint8_t data1, uint8_t data2) {
    uint8_t data[3];
    data[0] = data0;
    data[1] = data1;
    data[2] = data2;
    return command_data(value, 3, data);
  }

  bool command_data(uint8_t value, size_t len = 0, const uint8_t *data = NULL) {
    Wire.beginTransmission(kI2cAddress);
    Wire.write(0x80);  // Co=1 (non-continunation), DC=0 (command)
    Wire.write(value);
    if (len > 0) {
      Wire.write(0x40);  // Co=0 (continunation), DC=1 (data)
      for (size_t i=0; i<len; i++) {
        Wire.write(*data);
        data++;
      }
    }
    return !Wire.endTransmission();
  }

  bool data(size_t len, const uint8_t *values) {
    Wire.beginTransmission(kI2cAddress);
    Wire.write(0x40);  // Co=0 (continunation), DC=1 (data)
      for (size_t i=0; i<len; i++) {
        Wire.write(*values);
        values++;
      }
    return !Wire.endTransmission();
  }

  bool drawPoint(uint8_t x, uint8_t y, uint8_t r, uint8_t g, uint8_t b) {
    bool success = true;
    success &= command(kCmdSetColAddr, y+32, y+32);
    success &= command(kCmdSetRowAddr, x, x);
    uint16_t values16 = (((uint16_t)r * 31 / 255) << 11) |
                      (((uint16_t)g * 63 / 255)) << 5 |
                      ((uint16_t)b * 31 / 255);
    uint8_t values[2];
    values[0] = (values16 >> 8) & 0xff;
    values[1] = values16 & 0xff;
    success &= command(kCmdWriteRam);
    success &= data(2, values);
    return success;
  }

protected:
  const uint8_t kI2cAddress = 0x3c;

  const uint8_t kCmdSetColAddr = 0x15;
  const uint8_t kCmdSetRowAddr = 0x75;
  const uint8_t kCmdWriteRam = 0x5c;

  const uint8_t kCmdSetDisplayStart = 0xa1;
  const uint8_t kCmdSetDisplayOffset = 0xa2;
  const uint8_t kCmdSetRemap = 0xa0;
  const uint8_t kCmdSetSleepModeOn = 0xae;
  const uint8_t kCmdSetSleepModeOff = 0xaf;
  const uint8_t kCmdSetResetPrecharge = 0xb1;
  const uint8_t kCmdSetClock = 0xb3;
  const uint8_t kCmdSetSecondPrecharge = 0xb6;
  const uint8_t kCmdSetPrechargeVoltage = 0xbb;
  const uint8_t kCmdSetVcomh = 0xbb;
  const uint8_t kCmdMasterContrastCurrent = 0xc7;
  const uint8_t kCmdSetContrastCurrent = 0xc1;
  const uint8_t kCmdSetMuxRatio = 0xca;
  const uint8_t kCmdSetCommandLock = 0xfd;
  const uint8_t kDataUnlockOledDriver = 0x12;

  const uint8_t kCmdDisplayAllOff = 0xa4;
  const uint8_t kCmdDisplayAllOn = 0xa5;
  const uint8_t kCmdDisplayNormal = 0xa6;
};

Ssd1357 Oled;


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

  Oled.begin();
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

    // if (Stm32.writeServoValue(0, servoValue)) {
    if (Oled.test()) {
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

  delay(100);
}
