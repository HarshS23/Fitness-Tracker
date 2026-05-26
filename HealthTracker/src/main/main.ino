#include <Arduino.h>
#include <lvgl.h>
#include <Arduino_GFX_Library.h>
#include <Wire.h>
#include "MAX30105.h"
#include "heartRate.h"

// Pin definitions for Waveshare ESP32-S3-LCD-1.47
#define TFT_MOSI  45
#define TFT_SCLK  40
#define TFT_CS    42
#define TFT_DC    41
#define TFT_RST   39
#define TFT_BL    48

#define SCREEN_WIDTH  172
#define SCREEN_HEIGHT 320


// MAX30102 globals
MAX30105 particleSensor;
const byte RATE_SIZE = 4;
byte rates[RATE_SIZE];
byte rateSpot = 0;
long lastBeat = 0;
float beatsPerMinute;
int beatAvg;


// Display driver
Arduino_DataBus *bus = new Arduino_ESP32SPI(TFT_DC, TFT_CS, TFT_SCLK, TFT_MOSI);
Arduino_GFX *gfx = new Arduino_ST7789(bus, TFT_RST, 0, false,
  SCREEN_WIDTH, SCREEN_HEIGHT, 34, 0, 34, 0);

// LVGL draw buffer
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[SCREEN_WIDTH * 10];

// LVGL flush callback
void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  gfx->draw16bitRGBBitmap(area->x1, area->y1, (uint16_t *)color_p, w, h);
  lv_disp_flush_ready(disp);
}

// Include your SquareLine exported UI header here
//#include "ui/ui.h"

void setup() {
  Serial.begin(115200);

  // ADD THIS BLOCK - I2C Scanner
  // Wire.begin(5, 6); // SDA=42, SCL=41
  // Serial.println("Scanning I2C...");
  // for (byte i = 8; i < 120; i++) {
  //   Wire.beginTransmission(i);
  //   if (Wire.endTransmission() == 0) {
  //     Serial.print("Sensor found at address: 0x");
  //     Serial.println(i, HEX);
  //   }
  // }
  // Serial.println("I2C scan done.");
  // END OF ADDED BLOCK

  // New code 

  // After the I2C scan, add this check:
  Wire.begin(5, 6);
  Serial.println("Scanning I2C...");
  bool sensorFound = false;
  for (byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);
    if (Wire.endTransmission() == 0) {
      Serial.print("Sensor found at address: 0x");
      Serial.println(i, HEX);
      sensorFound = true;
    }
  }
  Serial.println("I2C scan done.");

  // STOP here if nothing found - don't even try to init
  if (!sensorFound) {
    Serial.println("NO I2C DEVICES FOUND - CHECK WIRING!");
    while(1); // halt
  }




  // Init MAX30102
  if (!particleSensor.begin(Wire, I2C_SPEED_FAST, 0x57)) { // its either 0x63 or 0x57
    Serial.println("MAX30102 not found! Check wiring.");
    while (1);
  }
  Serial.println("MAX30102 ready!");
  // particleSensor.setup();
  // particleSensor.setPulseAmplitudeRed(0xFF);
  // particleSensor.setPulseAmplitudeIR(0xFF);
  // particleSensor.setPulseAmplitudeGreen(0);

  particleSensor.setup(
  0x0F,  // LED brightness (start lower, not max)
  4,     // sample average
  2,     // LED mode: 2 = Red + IR
  400,   // sample rate
  411,   // pulse width
  4096   // ADC range
  );

  particleSensor.setPulseAmplitudeRed(0x0F);  
  particleSensor.setPulseAmplitudeIR(0x0F);
  particleSensor.setPulseAmplitudeGreen(0);



  // Backlight
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);

  // Init display
  gfx->begin();
  gfx->fillScreen(0x0000);

  // Init LVGL
  lv_init();
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, SCREEN_WIDTH * 10);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = SCREEN_WIDTH;
  disp_drv.ver_res = SCREEN_HEIGHT;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  // Init your SquareLine UI
  //ui_init();
}


void loop() {
  // ADD THIS - populates the library's internal buffer
  particleSensor.check();

  // Then read from FIFO properly
  while (particleSensor.available()) {
    long irValue = particleSensor.getFIFOIR(); // use getFIFOIR() not getIR()

    if (checkForBeat(irValue) == true) {
      long delta = millis() - lastBeat;
      lastBeat = millis();
      beatsPerMinute = 60 / (delta / 1000.0);

      if (beatsPerMinute < 255 && beatsPerMinute > 20) {
        rates[rateSpot++] = (byte)beatsPerMinute;
        rateSpot %= RATE_SIZE;
        beatAvg = 0;
        for (byte x = 0; x < RATE_SIZE; x++)
          beatAvg += rates[x];
        beatAvg /= RATE_SIZE;
      }
    }

    static long lastPrint = 0;
    if (millis() - lastPrint > 1000) {
      lastPrint = millis();
      Serial.print("IR="); Serial.print(irValue);
      Serial.print(" BPM="); Serial.print(beatsPerMinute);
      Serial.print(" Avg BPM="); Serial.println(beatAvg);
      if (irValue < 50000)
        Serial.println(" -- No finger detected");
    }

    particleSensor.nextSample(); // move to next FIFO sample
  }

  lv_timer_handler();
  delay(5);
}

// void loop() {

//   particleSensor.check();

//   // Read heart rate
//   long irValue = particleSensor.getIR();

//   if (checkForBeat(irValue) == true) {
//     long delta = millis() - lastBeat;
//     lastBeat = millis();

//     beatsPerMinute = 60 / (delta / 1000.0);

//     if (beatsPerMinute < 255 && beatsPerMinute > 20) {
//       rates[rateSpot++] = (byte)beatsPerMinute;
//       rateSpot %= RATE_SIZE;

//       beatAvg = 0;
//       for (byte x = 0; x < RATE_SIZE; x++)
//         beatAvg += rates[x];
//       beatAvg /= RATE_SIZE;
//     }
//   }

//   // Print to serial every second
//   static long lastPrint = 0;
//   if (millis() - lastPrint > 1000) {
//     lastPrint = millis();
//     Serial.print("IR="); Serial.print(irValue);
//     Serial.print(" BPM="); Serial.print(beatsPerMinute);
//     Serial.print(" Avg BPM="); Serial.print(beatAvg);

//     if (irValue < 50000)
//       Serial.print(" -- No finger detected");

//     Serial.println();
//   }

//   lv_timer_handler();
//   delay(5);

// }
