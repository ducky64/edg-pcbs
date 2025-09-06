#include <Arduino.h>

// pin assigns from HDL
// [ax1=ADC1_CH4, 3, 
// ax2=ADC1_CH3, 15, 
// trig=ADC1_CH1, 17, 
// vbat_sense=ADC1_CH0, 18, 
// i2c=I2C, 
// i2c.scl=GPIO5, 4, 
// i2c.sda=GPIO19, 14, 
// sw=GPIO6, 5, 
// sw0=GPIO10, 10, 
// sw1=GPIO18, 13, 
// sw2=GPIO7, 6]

const int kPinLed = 9;
const int kPinI2cSda = 19;
const int kPinI2cScl = 5;

#include <Wire.h>


class Mp2722 {
public:
  bool writeRegister(uint8_t addr, uint8_t value) {
    Wire.beginTransmission(kI2cAddress);
    Wire.write(addr);
    Wire.write(value);
    return !Wire.endTransmission();
  }

  bool readRegister(uint8_t addr, uint8_t* valueOut) {
    Wire.beginTransmission(kI2cAddress);
    Wire.write(addr);  // set address
    if (Wire.endTransmission()) {  // if failed
      return false;
    }

    Wire.requestFrom(kI2cAddress, 2);
    uint8_t result = Wire.read();
    if (Wire.endTransmission()) {
      return false;
    }
    *valueOut = result;
    return true;
  }

protected:
const int kI2cAddress = 0x3F;
};


Mp2722 conv;

void setup() {
  Serial.begin(115200);
  Serial.println("\r\n\n\nStart\r\n");

  pinMode(kPinLed, OUTPUT);
  digitalWrite(kPinLed, 1);

  delay(100);
  Wire.begin(kPinI2cSda, kPinI2cScl);

  if (!conv.writeRegister(0x02, 0xc2)) {  // 160mA charging current
    Serial.println("Icc set fail\r\n");
  }

  // if (!conv.writeRegister(0x08, 0x64)) {  // 5v out, 500mA limit
  if (!conv.writeRegister(0x08, 0x67)) {  // 5.15v out, 500mA limit
    Serial.println("boost set fail\r\n");
  }
  if (!conv.writeRegister(0x09, 0x3f)) {  // DRP Try.SNK, boost en
    Serial.println("boost en fail\r\n");
  }

  uint8_t reg1val;
  bool success = conv.readRegister(0x01, &reg1val);
  Serial.printf("0x01 => %d %02x\r\n", success, reg1val);

  Serial.println("Setup complete\r\n");
}


void loop() {
  int thisMillis = millis();

  digitalWrite(kPinLed, !digitalRead(kPinLed));
  delay(250);
}
