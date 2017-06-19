#include <XPT2046_Touchscreen.h>
#include <SPI.h>

// These are the pins for all ESP8266 boards
#define PIN_D0 16
#define PIN_D1  5
#define PIN_D2  4
#define PIN_D3  0
#define PIN_D4  2
#define PIN_D5 14 // SCLK
#define PIN_D6 12 // MISO
#define PIN_D7 13 // MOSI
#define PIN_D8 15
#define PIN_D9  3
#define PIN_D10 1

#define CS_PIN  PIN_D2 // XPT2046 chip select
#define TFT_CS  PIN_D8 // TFT chip select

XPT2046_Touchscreen ts(CS_PIN);

void setup() {
  Serial.begin(38400);
  digitalWrite(TFT_CS, 1); // Disable TFT for test only
  ts.begin();
  while (!Serial && (millis() <= 1000));
}

void loopB() {
  TS_Point p = ts.getPoint();
  Serial.print("Pressure = ");
  Serial.print(p.z);
  if (ts.touched()) {
    Serial.print(", x = ");
    Serial.print(p.x);
    Serial.print(", y = ");
    Serial.print(p.y);
  }
  Serial.println();
  //  delay(100);
  delay(30);
}

void loop() {
  if (ts.touched()) {
    TS_Point p = ts.getPoint();
    Serial.print("Pressure = ");
    Serial.print(p.z);
    Serial.print(", x = ");
    Serial.print(p.x);
    Serial.print(", y = ");
    Serial.print(p.y);
    delay(30);
    Serial.println();
  }
}


