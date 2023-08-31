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

I2CSlave i2c(B11, B10);  // sda, scl

DigitalOut* Servos[] = {
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

AnalogIn* ServoFbs[] = {
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

uint16_t kServoTimeMinUs = 1000;
uint16_t kServoTimeMaxUs = 2000;
uint16_t kServoPeriodUs = 2100;  // each servo allocated this time period
uint16_t kServosScanTimeUs = 25000;  // time between scanning all servos

// updated by host processor
uint8_t ServoValues[kServosCount] = {0};
uint16_t ServoFbValues[kServosCount] = {0};

RawSerial SwoSerial(B6, A10, 115200);  // need to give it a dummy RX, internally mbed_asserts RX isn't NC
Timer SysTimer;
Timer ServoTimer;

int main() {
  Led = 1;
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
        ((uint32_t)ServoValues[servoIndex] * (kServoTimeMaxUs - kServoTimeMinUs) / 255);
      
      ServoFbValues[servoIndex] = ServoFbs[servoIndex]->read_u16();

      while (ServoTimer.read_us() < timerTargetUs);
      servo = 0;

      while (ServoTimer.read_us() < kServoPeriodUs);
    }
    while (SysTimer.read_us() < kServosScanTimeUs);
    SysTimer.reset();

    Led = !Led;
  }
}
