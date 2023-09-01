// "i2c=I2C0_T",
// "i2c.scl=GPIO25, 37",
// "i2c.sda=GPIO24, 36",
// "led_0=GPIO2, 4",
// "led_1=GPIO9, 12",
// "led_2=GPIO11, 14",
// "led_3=GPIO13, 16",
// "swd_swo=GPIO16, 27",
// "swd=SWD",
// "swd.swdio=25",
// "swd.swclk=24"

#include <Arduino.h>

const int led0 = 2;
const int led1 = 9;
const int led2 = 11;
const int led3 = 13;

void setup() {
  pinMode(led0, OUTPUT);
  pinMode(led1, OUTPUT);
  pinMode(led2, OUTPUT);
  pinMode(led3, OUTPUT);
  digitalWrite(led0, 0);
  digitalWrite(led1, 0);
  digitalWrite(led2, 1);
  digitalWrite(led3, 1);
}

void loop() {
  digitalWrite(led0, !digitalRead(led0));
  digitalWrite(led1, !digitalRead(led0));
  
  delay(100);
}
