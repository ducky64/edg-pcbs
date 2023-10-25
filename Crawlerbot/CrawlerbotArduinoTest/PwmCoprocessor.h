#ifndef __PWM_COPROCESSOR_H__
#define __PWM_COPROCESSOR_H__

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

#endif
