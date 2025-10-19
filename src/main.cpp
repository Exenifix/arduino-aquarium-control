#include <Arduino.h>
#include <GyverNTC.h>
#include <RTClib.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// Thermistor constants
#define THERM_PIN A0
#define THERM_RES 10000
#define THERM_BETA 3950
#define THERM_SAMPLES 50

// Display constants
#define DISPLAY_ADDRESS 0x3C

// Other constants
#define MIN_OPTIMAL_TEMP 23
#define MAX_OPTIMAL_TEMP 27

// Bitmaps
#define WARNING_BITMAP_H 16
#define WARNING_BITMAP_W 16
static const unsigned char PROGMEM warning_bitmap[] = {
    0x01, 0x80, 0x03, 0xc0, 0x06, 0x60, 0x04, 0x20, 0x0d, 0xb0, 0x09, 0x90, 0x19, 0x98, 0x11, 0x88,
0x31, 0x8c, 0x21, 0x84, 0x61, 0x86, 0x40, 0x02, 0xc0, 0x03, 0x81, 0x81, 0x80, 0x01, 0xff, 0xff
};

// Lib objects
GyverNTC therm(THERM_PIN, THERM_RES, THERM_BETA);
RTC_DS3231 rtc;
Adafruit_SSD1306 display(128, 64, &Wire, -1);

// Mutable variables
float temp;
float outside_temp;
bool temp_warn = false;
int hour;
int minute;
bool blink = false;

[[noreturn]] void halt() {
    while (true) {
        delay(10);
    }
}

void update_temp() {
    temp = therm.getTempAverage(50);
    temp_warn = temp <= MIN_OPTIMAL_TEMP || temp >= MAX_OPTIMAL_TEMP;
}

void update_outside_temp() {
    outside_temp = rtc.getTemperature();
}

void update_datetime() {
    const DateTime now = rtc.now();
    hour = now.hour();
    minute = now.minute();
}

void gather_data() {
    update_temp();
    update_outside_temp();
    update_datetime();
}

void update_display() {
    blink = !blink;
    display.clearDisplay();
    display.setTextWrap(false);
    display.setTextColor(WHITE);

    // Time
    display.drawCircle(64, -5, 50 + (blink ? 2 : 0), WHITE);
    display.setTextSize(2);
    display.setCursor(35, 10);
    if (hour < 10) {
        display.print(0);
    }
    display.print(hour);
    display.print(blink ? F(":") : F(" "));
    if (minute < 10) {
        display.print(0);
    }
    display.print(minute);

    // inside temp
    display.setTextSize(1);
    display.setCursor(10, 50);
    display.print(temp, 1);
    display.print(F("C"));
    if (temp_warn && blink) {
        display.drawBitmap(56, 27, warning_bitmap, WARNING_BITMAP_W, WARNING_BITMAP_H, WHITE);
    }

    // outside temp
    display.setCursor(90, 50);
    display.print(outside_temp, 1);
    display.print(F("C"));

    display.display();
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
    if (!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS)) {
        Serial.println("Failed to initialize display!");
        halt();
    }
    Serial.println("[OK] Display initialized");
}

void scanI2C() {
    Serial.println("Scanning I2C devices...");
    byte error, address;
    int devices = 0;

    for(address = 1; address < 127; address++ ) {
        Wire.beginTransmission(address);
        error = Wire.endTransmission();

        if (error == 0) {
            Serial.print("I2C device found at address 0x");
            if (address < 16) Serial.print("0");
            Serial.print(address, HEX);
            Serial.println();
            devices++;
        }
    }

    if (devices == 0) {
        Serial.println("No I2C devices found");
    }
}

void setup() {
    Serial.begin(9600);
    scanI2C();
    setup_rtc();
    setup_display();
}

void loop() {
    gather_data();
    update_display();
    delay(1000);
}