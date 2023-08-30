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


RawSerial serial(B6, A10);  // need to give it a dummy RX, internally mbed_asserts RX isn't NC
Timer timer;

int main() {
  Led = 1;
  serial.baud(115200);
  serial.printf("\r\n\n\nStart\r\n");
  timer.start();

  while (1) {
    while (timer.read_ms() < 250);  // clock is fast by 1.5x
    timer.reset();

    Led = !Led;
  }
}
