#include <Arduino.h>
#include <GyverNTC.h>
#include <RTClib.h>
#include <IRSend.hpp>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Thermistor constants
#define THERM_PIN A0
#define THERM_RES 10000
#define THERM_BETA 3950
#define THERM_SAMPLES 50

// Display constants
#define DISPLAY_ADDRESS 0x3D


// Lib objects
GyverNTC therm(THERM_PIN, THERM_RES, THERM_BETA);
RTC_DS3231 rtc;
Adafruit_SSD1306 display(128, 64, &Wire);

// Mutable variables
float temp;
int hour;
int minute;

[[noreturn]] void halt() {
    while (true) {
        delay(10);
    }
}

void update_temp() {
    temp = therm.getTempAverage(50);
}

void update_datetime() {
    const DateTime now = rtc.now();
    hour = now.hour();
    minute = now.minute();
}

void gather_data() {
    update_temp();
    update_datetime();
}

void update_display() {

}

void setup_rtc() {
    Serial.println("Initializing RTC");
    if (!rtc.begin()) {
        Serial.println("Failed to initialize RTC");
        Serial.flush();
        halt();
    }

    if (rtc.lostPower()) {
        Serial.println("RTC lost power, setting time to compile datetime");
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    Serial.println("[OK] RTC initialized");
}

void setup_display() {
    Serial.println("Initializing display");
    display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS);
    Serial.println("[OK] Display initialized");
}

void setup() {
    Serial.begin(9600);
    setup_rtc();
    setup_display();
}

void loop() {
    gather_data();
    update_display();
    delay(1000);
}