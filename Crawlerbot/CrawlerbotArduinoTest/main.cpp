#include <Arduino.h>

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
int kPinStmReset = 8;
int kPinNeoPixel = 39;
int kPinI2cScl = 17;
int kPinI2cSda = 16;

#include <Wire.h>


#include <NeoPixelBrightnessBus.h>
const int kNeoPixelCount = 10;
NeoPixelBrightnessBus<NeoGrbFeature, NeoEsp32Rmt0Ws2812xMethod> NeoPixels(kNeoPixelCount, kPinNeoPixel);


const int kOledReset = 15;

#include "Ssd1357.h"
Ssd1357 Oled;

#include "PwmCoprocessor.h"
PwmCoprocessor Stm32;


const size_t kNumServos = 14;


#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
const char *kWifiSsid = "RobotDriverAP";
// const char *password = "password";
WiFiServer Server(80);
// note, send rate over network determined by kFbReadMs
WiFiClient Client;

const int kCmdDurationMs = 1000;
const size_t kNumRemoteServos = 10;
uint16_t remoteServoVaues[kNumRemoteServos] = {0};  // remote servos
int cmdExpireMillis = 0;


void setup() {
  delay(100);
  Serial.begin(115200);
  Serial.println("\r\n\n\nStart\r\n");

  pinMode(kPinStmReset, OUTPUT);
  digitalWrite(kPinStmReset, 0);
  delay(100);
  pinMode(kPinStmReset, INPUT_PULLUP);
  delay(100);

  Wire.begin(kPinI2cSda, kPinI2cScl);

  NeoPixels.Begin();
  NeoPixels.SetBrightness(32);

  pinMode(kPinLed, OUTPUT);
  digitalWrite(kPinLed, 1);
  Serial.println("Setup complete\r\n");

  // Oled.begin();

  // WiFi.softAP(kWifiSsid);
  // Serial.println(WiFi.softAPIP());

  WiFi.mode(WIFI_STA);
  WiFi.begin("lemur2", "nope");
  while(WiFi.status() != WL_CONNECTED){
    Serial.print(".");
    delay(100);
  }
  Serial.println(WiFi.localIP());

  Server.begin();
  Client = Server.available();  // initialize persistent client object
}


int servoUpdateIndex = 0;
int nextRemoteServoMillis = 0;
const int kServoUpdateMs = 100;
const int kServoNeutral = 114;

int nextFbReadMillis = 0;
const int kFbReadMs = 50;


void loop() {
  int thisMillis = millis();

  if (!Client) {
    Client = Server.available();
    if (Client) {
      Serial.println("New client");
    }
  }

  if (Client) {
    uint16_t cmdValues[kNumServos] = {0};
    if (Client.available() >= sizeof(cmdValues)) {
      Client.read((uint8_t*)cmdValues, sizeof(cmdValues));
      for (size_t i=0; i<kNumRemoteServos; i++) {
        remoteServoVaues[i] = cmdValues[i+4];  // first 4 are on ESP32 itself
      }
      cmdExpireMillis = thisMillis + kCmdDurationMs;  // always based off current millis
    }    
  }

  if (thisMillis >= nextRemoteServoMillis) {
    digitalWrite(kPinLed, 1);

    for (int i=0; i<10; i++) {
      if (thisMillis < cmdExpireMillis) {
        Stm32.writeServoValue(i, remoteServoVaues[i]);
      } else {  // expired, write neutral
        Stm32.writeServoValue(i, kServoNeutral);
      }
    }

    digitalWrite(kPinLed, 0);

    if (thisMillis < cmdExpireMillis) {
      NeoPixels.SetPixelColor(4, RgbColor(0, 127, 0));
    } else {
      NeoPixels.SetPixelColor(4, RgbColor(127, 0, 0));
    }
    NeoPixels.Show();

    nextRemoteServoMillis += kServoUpdateMs;  // increment for long term frequency stability
  }

  if (thisMillis >= nextFbReadMillis) {
    uint16_t fbValues[kNumServos] = {0};
    for (size_t i=0; i<kNumServos; i++) {
      if (!Stm32.readServoFeedback(i, fbValues + i)) {
        fbValues[i] = 0;
      }
    }

    if (Client) {
      Client.write((uint8_t*)fbValues, sizeof(fbValues));
    }

    nextFbReadMillis += kFbReadMs;  // increment for long term frequency stability
  }
}
