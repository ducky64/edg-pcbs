#include <mbed.h>

DigitalOut Led(A12);

AnalogIn Servo4Fb(A0);  // ADC12_IN0
AnalogIn Servo5Fb(A1);  // ADC12_IN1
AnalogIn Servo6Fb(A2);  // ADC12_IN2
AnalogIn Servo7Fb(A4);  // ADC12_IN4
AnalogIn Servo8Fb(B1);  // ADC12_IN9
AnalogIn Servo9Fb(B0);  // ADC12_IN8
AnalogIn Servo10Fb(A7);  // ADC12_IN7
AnalogIn Servo11Fb(A5);  // ADC12_IN5
AnalogIn ServoCam0Fb(A3);  // ADC12_IN3
AnalogIn ServoCam1Fb(A6);  // ADC12_IN6

DigitalOut Servo4(B5);
DigitalOut Servo5(B7);
DigitalOut Servo6(B8);
DigitalOut Servo7(B13);
DigitalOut Servo8(A11);
DigitalOut Servo9(A10);
DigitalOut Servo10(A9);
DigitalOut Servo11(B15);
DigitalOut ServoCam0(B9);
DigitalOut ServoCam1(A8);


#include "stm32f1xx_hal_i2c.h"
class I2CSlaveAsync: public I2CSlave {
public:
  I2CSlaveAsync(PinName sda, PinName scl): I2CSlave(sda, scl) {
  };

  int receive() {
    if (!asyncReadRunning_ && !asyncWriteRunning_) {
      return I2CSlave::receive();
    } else {
      return I2CSlave::NoData;
    }
  }

  // same as read(...), except nonblocking (returns immediately)
  // returns true if successfully started
  // based on i2c_slave_read in i2c_api.c
  bool readAsyncStart(char *data, int length) {
    struct i2c_s *obj_s = (struct i2c_s *) (&((&_i2c)->i2c));
    I2C_HandleTypeDef *handle = &(obj_s->handle);
    int ret = 0;

    ret = HAL_I2C_Slave_Sequential_Receive_IT(handle, (uint8_t *) data, length, I2C_NEXT_FRAME);
    if (ret == HAL_OK) {
      asyncReadRunning_ = true;
      return true;
    } else {
      return false;
    }
  }

  // after readAsyncStart, returns the same value as read(), -1 if not yet finished, or -2 if was not running
  // TODO: support timeouts?
  int readAsyncPoll() {
    struct i2c_s *obj_s = (struct i2c_s *) (&((&_i2c)->i2c));
    if (!asyncReadRunning_) {
      return -2;
    } else if (obj_s->pending_slave_rx_maxter_tx) {
      return -1;
    } else {
      asyncReadRunning_ = false;
      return 0;
    }
  }

protected:
  bool asyncReadRunning_ = false, asyncWriteRunning_ = false;
};

I2CSlaveAsync I2cTarget(B11, B10);  // sda, scl
const int kI2cAddress = 0x42;

DigitalOut * const Servos[] = {
  &Servo4,
  &Servo5,
  &Servo6,
  &Servo7,
  &Servo8,
  &Servo9,
  &Servo10,
  &Servo11,
  &ServoCam0,
  &ServoCam1,
};
constexpr int kServosCount = sizeof(Servos) / sizeof(Servos[0]);

AnalogIn * const ServoFbs[] = {
  &Servo4Fb,
  &Servo5Fb,
  &Servo6Fb,
  &Servo7Fb,
  &Servo8Fb,
  &Servo9Fb,
  &Servo10Fb,
  &Servo11Fb,
  &ServoCam0Fb,
  &ServoCam1Fb,
};
static_assert(sizeof(ServoFbs) / sizeof(ServoFbs[0]) == kServosCount);

const uint16_t kServoTimeMinUs = 1000;
const uint16_t kServoTimeMaxUs = 2000;
const uint16_t kServoPeriodUs = 2100;  // each servo allocated this time period
const uint16_t kServosScanTimeUs = 25000;  // time between scanning all servos

// updated by host processor
const uint8_t kIndexSetReadIndex = 0x80;  // when this is the servo index, the next byte is the feedback index to dead
uint8_t I2cServoReadIndex = 0;
uint8_t I2cServoValues[kServosCount] = {0};
uint16_t I2cServoFbValues[kServosCount] = {0};

RawSerial SwoSerial(B6, A10, 115200);  // need to give it a dummy RX, internally mbed_asserts RX isn't NC
Timer SysTimer;
Timer ServoTimer;


// TODO encapsulate into a class
uint8_t I2cBuffer[16];
void processI2c() {
  switch (I2cTarget.receive()) {
    case I2CSlave::ReadAddressed: break;  // ignored
    case I2CSlave::WriteGeneral: break;  // ignored
    case I2CSlave::WriteAddressed:
      I2cTarget.readAsyncStart((char*)I2cBuffer, 2);  // address, data bytes
      Led = 1;
      break;
  }
  if (I2cTarget.readAsyncPoll() == 0) {
    uint8_t index = I2cBuffer[0];
    if (index < kServosCount) {
      I2cServoValues[index] = I2cBuffer[1];
    } else if (index == kIndexSetReadIndex) {
      I2cServoReadIndex = I2cBuffer[1];
    }
    Led = 0;
  }
}

int main() {
  I2cTarget.address(kI2cAddress << 1);
  SwoSerial.printf("\r\n\n\nStart\r\n");
  SysTimer.start();
  ServoTimer.start();

  while (1) {
    for (int servoIndex=0; servoIndex<kServosCount; servoIndex++) {
      DigitalOut* servo = Servos[servoIndex];

      ServoTimer.reset();
      *servo = 1;

      // during the minimum PWM time, calculate the target time and read the ADC
      uint16_t timerTargetUs = kServoTimeMinUs + 
        ((uint32_t)I2cServoValues[servoIndex] * (kServoTimeMaxUs - kServoTimeMinUs) / 255);
      
      I2cServoFbValues[servoIndex] = ServoFbs[servoIndex]->read_u16();

      while (ServoTimer.read_us() < timerTargetUs) {
        processI2c();
      }
      *servo = 0;

      while (ServoTimer.read_us() < kServoPeriodUs) {
        processI2c();
      }
    }
    while (SysTimer.read_us() < kServosScanTimeUs) {
      processI2c();
    }
    SysTimer.reset();
  }

  return 0;
}
