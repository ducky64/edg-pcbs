#include <mbed.h>

DigitalOut LedR(A1);
DigitalOut LedG(A2);
DigitalOut LedB(A3);


int main() {
  LedR = 0;
  LedG = 0;
  LedB = 0;

  while (1) {
    LedB = 1;
    LedR = 0;
    wait_us(1000*100);
    LedR = 1;
    LedG = 0;
    wait_us(1000*100);
    LedG = 1;
    LedB = 0;
    wait_us(1000*100);
  }

  return 0;
}
