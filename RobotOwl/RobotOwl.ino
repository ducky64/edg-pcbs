#include <Freenove_WS2812_Lib_for_ESP32.h>
#include <ESP32Servo.h>

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


#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

int kScreenWidth = 128;
int kScreenHeight = 64;
int kScreenAddress = 0x3c;
int kSccbScl = 5;
int kSccbSda = 4;
int kOledReset = 46;

// OLED code based on
// https://github.com/adafruit/Adafruit_SSD1306/blob/master/examples/ssd1306_128x64_i2c/ssd1306_128x64_i2c.ino
Adafruit_SSD1306 display(kScreenWidth, kScreenHeight, &Wire, kOledReset);


// #include "AudioFileSourcePROGMEM.h"
// #include "AudioGeneratorRTTTL.h"
// #include "AudioOutputI2S.h"

// const char testRtttl[] = "scale_up:d=32,o=5,b=100:c,c#,d#,e,f#,g#,a#,b";

// AudioGeneratorRTTTL *rtttl;
// AudioFileSourcePROGMEM *file;
// AudioOutputI2S *out;


// based on https://github.com/atomic14/esp32-i2s-mic-test/blob/main/i2s_mic_test/i2s_mic_test.ino
#include <driver/i2s.h>
#define SAMPLE_BUFFER_SIZE 512

int kPinMicClk = 3;
int kPinMicData = 14;

i2s_config_t i2s_config = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_PDM),
    .sample_rate = 8000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_I2S,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 1024,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};
i2s_pin_config_t i2s_mic_pins = {
    .ws_io_num = kPinMicClk,
    .data_in_num = kPinMicData};


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

  // Serial.printf("RTTTL start\n");
  // file = new AudioFileSourcePROGMEM(testRtttl, strlen_P(testRtttl));
  // out = new AudioOutputI2S();
  // out->SetPinout(kPinI2sSck, kPinI2sWs, kPinI2sSd);
  // rtttl = new AudioGeneratorRTTTL();
  // rtttl->begin(file, out);

  i2s_driver_install(I2S_NUM_0, &i2s_config, 0, NULL);
  i2s_set_pin(I2S_NUM_0, &i2s_mic_pins);
}

void loop() {
  // Serial.println(analogRead(kPhotodiodePin));

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

  // if (rtttl->isRunning()) {
  //   if (!rtttl->loop()) rtttl->stop();
  // } else {
  //   Serial.printf("RTTTL done\n");
  // }

  int32_t raw_samples[SAMPLE_BUFFER_SIZE];
  size_t bytes_read = 0;
  i2s_read(I2S_NUM_0, raw_samples, sizeof(int32_t) * SAMPLE_BUFFER_SIZE, &bytes_read, portMAX_DELAY);
  int samples_read = bytes_read / sizeof(int32_t);
  // dump the samples out to the serial channel.
  for (int i = 0; i < samples_read; i++)
  {
    Serial.printf("%ld\n", raw_samples[i]);
  }
}
