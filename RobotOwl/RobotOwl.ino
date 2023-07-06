#include <Freenove_WS2812_Lib_for_ESP32.h>
#include <ESP32Servo.h>

#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "AudioFileSourcePROGMEM.h"
#include "AudioGeneratorRTTTL.h"
#include "AudioOutputI2S.h"

const char testRtttl[] = "scale_up:d=32,o=5,b=100:c,c#,d#,e,f#,g#,a#,b";

AudioGeneratorRTTTL *rtttl;
AudioFileSourcePROGMEM *file;
AudioOutputI2S *out;

// "speaker=I2S0",
// "speaker.sck=GPIO41, 35",
// "speaker.ws=GPIO2, 37",
// "speaker.sd=GPIO42, 36",
// "photodiode=ADC1_CH0, 38",
// "oled=CAM_SCCB",
// "oled.scl=4",
// "oled.sda=3",
// "mic_clk=GPIO3, 12",
// "mic_data=GPIO14, 19",
// "servo0=GPIO47, 25",
// "servo1=GPIO21, 24",
// "ws2812=GPIO48, 26"

int kPinOnboardLed = 2;
int kPinNeopixel = 48;
int kLedsCount = 12;
int kPhotodiodePin = 1;

int kPinI2sSck = 41;
int kPinI2sWs = 2;
int kPinI2sSd = 42;

Servo Servo0;
Servo Servo1;

Freenove_ESP32_WS2812 Neopixels(kLedsCount, kPinNeopixel, 0, TYPE_GRB);

int kScreenWidth = 128;
int kScreenHeight = 64;
int kScreenAddress = 0x3c;
int kSccbScl = 5;
int kSccbSda = 4;

// OLED code based on
// https://github.com/adafruit/Adafruit_SSD1306/blob/master/examples/ssd1306_128x64_i2c/ssd1306_128x64_i2c.ino
Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, -1);


void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);
  Serial.println("start");

  pinMode(kPinOnboardLed, OUTPUT);
  digitalWrite(kPinOnboardLed, 0);

	ESP32PWM::allocateTimer(0);
	ESP32PWM::allocateTimer(1);
	ESP32PWM::allocateTimer(2);
	ESP32PWM::allocateTimer(3);
	Servo0.setPeriodHertz(50);    // standard 50 hz servo
  Servo0.attach(47, 1000, 2000);
	Servo1.setPeriodHertz(50);    // standard 50 hz servo
  Servo1.attach(21, 1000, 2000);
  // Servo1.attach(21, 600, 2300);

  Neopixels.begin();
  Neopixels.setBrightness(15);

  // start up with all lights white
  for (int i = 0; i < kLedsCount; i++) {
    Neopixels.setLedColorData(i, 255, 255, 255);
  }
  Neopixels.show();

  Wire.begin(kSccbSda, kSccbScl);

  display.ssd1306_command(0xe4);
  if(!display.begin(SSD1306_EXTERNALVCC, kScreenAddress)) {
    Serial.println("SSD1306 allocation failed");
    for(;;); // Don't proceed, loop forever
  }
  display.display();

  Serial.printf("RTTTL start\n");

  file = new AudioFileSourcePROGMEM(testRtttl, strlen_P(testRtttl));
  out = new AudioOutputI2S();
  out->SetPinout(kPinI2sSck, kPinI2sWs, kPinI2sSd);
  rtttl = new AudioGeneratorRTTTL();
  rtttl->begin(file, out);
}

void loop() {
  if (rtttl->isRunning()) {
    if (!rtttl->loop()) rtttl->stop();
  } else {
    Serial.printf("RTTTL done\n");
  }

  // put your main code here, to run repeatedly:
  Serial.println(analogRead(kPhotodiodePin));

  // digitalWrite(kPinOnboardLed, 1);
  // delay(100);
  // digitalWrite(kPinOnboardLed, 0);
  // delay(100);

  // Neopixels.setLedColorData(0, 255, 0, 0);
  // Neopixels.setLedColorData(11, 255, 0, 0);
  // Neopixels.show();
  // Servo0.write(45);
  // delay(500);

  // Neopixels.setLedColorData(0, 0, 255, 0);
  // Neopixels.setLedColorData(11, 0, 255, 0);
  // Neopixels.show();
  // Servo0.write(135);
  // delay(500);

  // display.ssd1306_command(SSD1306_DISPLAYOFF);
}
