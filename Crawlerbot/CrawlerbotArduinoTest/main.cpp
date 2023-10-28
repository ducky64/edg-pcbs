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


#include <WiFi.h>
#include <WiFiClient.h>
#include <WiFiAP.h>
const char *kWifiSsid = "RobotDriverAP";
// const char *password = "password";
WiFiServer Server(80);


const int kCmdDurationMs = 1000;
int cmdServos[10] = {0};  // remote servos
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

  WiFi.softAP(kWifiSsid);
  Serial.println(WiFi.softAPIP());
  Server.begin();
}


int servoUpdateIndex = 0;
int lastServoValueMillis = 0;
const int kServoUpdateMs = 100;
const int kServoNeutral = 114;

int lastFbReadMillis = 0;
const int kFbReadMs = 50;


void loop() {
  Serial.println("Loop\r\n");

  WiFiClient client = Server.available();   // listen for incoming clients
  if (client) {
    Serial.println("New client");
    String currentLine = "";                // make a String to hold incoming data from the client
    while (client.connected()) {            // loop while the client's connected
      if (client.available()) {             // if there's bytes to read from the client,
        char c = client.read();             // read a byte, then
        if (c == '\n') {                    // if the byte is a newline character
          // if the current line is blank, you got two newline characters in a row.
          // that's the end of the client HTTP request, so send a response:
          if (currentLine.length() == 0) {
            // HTTP headers always start with a response code (e.g. HTTP/1.1 200 OK)
            // and a content-type so the client knows what's coming, then a blank line:
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println();

            // the content of the HTTP response follows the header:
            client.print("<a href=\"/fwd\">FWD</a><br>");
            client.print("<a href=\"/left\">LEFT</a><br>");
            client.print("<a href=\"/right\">RIGHT</a><br>");
            client.print("<a href=\"/back\">BACK</a><br>");

            // The HTTP response ends with another blank line:
            client.println();
            // break out of the while loop:
            break;
          } else {    // if you got a newline, then clear currentLine:
            currentLine = "";
          }
        } else if (c != '\r') {  // if you got anything else but a carriage return character,
          currentLine += c;      // add it to the end of the currentLine
        }

        int thisMillis = millis();
        if (currentLine.endsWith("GET /fwd")) {
          cmdServos[2] = 255;  // front left
          cmdServos[6] = 0;  // front right
          cmdServos[8] = 255;  // back left
          cmdServos[9] = 0;  // back right
          cmdExpireMillis = thisMillis + kCmdDurationMs;
        } else if (currentLine.endsWith("GET /left")) {
          cmdServos[2] = kServoNeutral;
          cmdServos[6] = 0;
          cmdServos[8] = kServoNeutral;
          cmdServos[9] = 0;
          cmdExpireMillis = thisMillis + kCmdDurationMs;
        } else if (currentLine.endsWith("GET /right")) {
          cmdServos[2] = 255;
          cmdServos[6] = kServoNeutral;
          cmdServos[8] = 255;
          cmdServos[9] = kServoNeutral;
          cmdExpireMillis = thisMillis + kCmdDurationMs;
        } else if (currentLine.endsWith("GET /back")) {
          cmdServos[2] = 0;
          cmdServos[6] = 255;
          cmdServos[8] = 0;
          cmdServos[9] = 255;
          cmdExpireMillis = thisMillis + kCmdDurationMs;
        }
      }
    }
    // close the connection:
    client.stop();
  }

  int thisMillis = millis();
  if (thisMillis - lastServoValueMillis >= kServoUpdateMs) {
    digitalWrite(kPinLed, 1);
    for (int i=0; i<10; i++) {
      if (thisMillis < cmdExpireMillis) {
        Stm32.writeServoValue(i, cmdServos[i]);
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

    // servoUpdateIndex++;
    lastServoValueMillis = lastServoValueMillis + kServoUpdateMs;
  }

  // if (thisMillis - lastFbReadMillis >= kFbReadMs) {
  //   uint16_t fbValue;
  //   if (Stm32.readServoFeedback(0, &fbValue)) {
  //     uint8_t ledValue = fbValue / 256;
  //     NeoPixels.SetPixelColor(4, RgbColor(ledValue, ledValue, ledValue));
  //   } else {
  //     NeoPixels.SetPixelColor(4, RgbColor(0, 0, 0));
  //   }
  //   NeoPixels.Show();
  // }
}
