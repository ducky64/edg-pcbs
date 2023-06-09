// "spi=SPI2",
// "spi.sck=GPIO40, 33",
// "spi.mosi=GPIO41, 34",
// "ledr=GPIO7, 7",
// "ledg=GPIO15, 8",
// "ledb=GPIO16, 9",
// "sw0=GPIO8, 12",
// "sw1=GPIO18, 11",
// "sw2=GPIO17, 10",
// "oled_rst=GPIO2, 38",
// "oled_dc=GPIO39, 32",
// "oled_cs=GPIO38, 31",
// "epd_busy=GPIO42, 35",
// "0=USB",
// "0.dp=GPIO20, 14",
// "0.dm=GPIO19, 13"


int kLedR = 7;
int kLedG = 15;
int kLedB = 16;

void setup() {
  // put your setup code here, to run once:
  pinMode(kLedR, OUTPUT);
  pinMode(kLedG, OUTPUT);
  pinMode(kLedB, OUTPUT);

  digitalWrite(kLedR, 0);
  digitalWrite(kLedG, 0);
  digitalWrite(kLedB, 0);
}

void loop() {
  // put your main code here, to run repeatedly:
  digitalWrite(kLedG, 0);
  delay(100);
  digitalWrite(kLedG, 1);
  delay(100);
}
