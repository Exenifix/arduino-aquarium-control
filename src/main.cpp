#include <Arduino.h>
#include <GyverNTC.h>
#include <RTClib.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <TinyIRSender.hpp>
#include <Servo.h>

// Thermistor constants
#define THERM_PIN A0
#define THERM_RES 10000
#define THERM_BETA 3950
#define THERM_SAMPLES 50

// Display constants
#define DISPLAY_ADDRESS 0x3C

// Remote constants
#define IR_SENDER_PIN 2
#define IR_CMD_OFF 0x6
#define IR_CMD_ON 0x7
#define IR_CMD_SUNRISE 0xD
#define IR_CMD_NOON 0x15
#define IR_CMD_EVENING 0x11
#define IR_CMD_NEON 0x16

// Servo constants
#define SERVO_PIN 6
#define SERVO_CALIB (-3)
#define SERVO_DEGREE 30
#define SERVO_DELAY_MS 250

// Other constants
#define MIN_OPTIMAL_TEMP 23
#define MAX_OPTIMAL_TEMP 27
#define IR_SYNC_INTERVAL 10000
#define FISH_FEED_HOUR 17
#define FEED_BUTTON_PIN 4
static const int PROGMEM color_progress[][2] {
    {-2, 8},
    {8, 10},
    {10, 12},
    {12, 18},
    {18, 20},
    {20, 22},
    {22, 32}
};

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
Servo servo;

// Mutable variables
float temp;
float outside_temp;
bool temp_warn = false;
int hour;
int minute;
bool blink = false;
uint8_t current_color = IR_CMD_OFF;
unsigned long last_ir_update = 0;

[[noreturn]] void halt() {
    while (true) {
        delay(10);
    }
}

void send_ir_command(uint8_t cmd) {
    sendNEC(IR_SENDER_PIN, 0, cmd, 1);
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

uint8_t get_current_color() {
    if (hour < 8) {
        return IR_CMD_OFF;
    }
    if (hour < 10) {
        return IR_CMD_SUNRISE;
    }
    if (hour < 12) {
        return IR_CMD_NOON;
    }
    if (hour < 18) {
        return IR_CMD_OFF;
    }
    if (hour < 20) {
        return IR_CMD_EVENING;
    }
    if (hour < 22) {
        return IR_CMD_NEON;
    }
    return IR_CMD_OFF;
}

float get_current_color_progress() {
    int v1 = 0, v2 = 24;
    int a, b;
    for (unsigned int i = 0; i < sizeof color_progress / sizeof color_progress[0]; i++) {
        a = pgm_read_word(&color_progress[i][0]);
        b = pgm_read_word(&color_progress[i][1]);
        if (hour < b) {
            v1 = a;
            v2 = b;
            break;
        }
    }
    v1 *= 60;
    v2 *= 60;
    return constrain((float) (hour * 60 + minute - v1) / (v2 - v1), 0.f, 1.f);
}

const __FlashStringHelper *get_current_color_name() {
    switch (current_color) {
        case IR_CMD_OFF:
            return F("OFF");
        case IR_CMD_SUNRISE:
            return F("SUN");
        case IR_CMD_NOON:
            return F("LIT");
        case IR_CMD_EVENING:
            return F("EVE");
        case IR_CMD_NEON:
            return F("NEO");
        default: return F("NAN");
    }
}

void gather_data() {
    update_temp();
    update_outside_temp();
    update_datetime();
}

void set_servo_degree(const int degree) {
    servo.write(degree + SERVO_CALIB);
}

void update_ir() {
    if (millis() - last_ir_update < IR_SYNC_INTERVAL) {
        return;
    }
    last_ir_update = millis();

    auto new_color = get_current_color();
    if (current_color == IR_CMD_OFF && new_color != current_color) {
        send_ir_command(IR_CMD_ON);
        delay(300); // will mess up a blink for one time, but it's acceptable
    }
    send_ir_command(new_color);
    current_color = new_color;
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

    // current lights status
    display.setCursor(57, 53);
    display.print(get_current_color_name());

    // color progress
    display.drawFastHLine(0, 63, static_cast<int16_t>(get_current_color_progress() * 128), WHITE);

    display.display();
}

void feed_fish() {
    set_servo_degree(120 + SERVO_DEGREE);
    delay(SERVO_DELAY_MS);
    set_servo_degree(120);
}

void check_feeder() {
    if (rtc.alarmFired(1)) {
        rtc.clearAlarm(1);
        feed_fish();
    }
    if (digitalRead(FEED_BUTTON_PIN)) {
        feed_fish();
    }
}

void setup_rtc() {
    Serial.println(F("Initializing RTC"));
    if (!rtc.begin()) {
        Serial.println(F("Failed to initialize RTC"));
        Serial.flush();
        halt();
    }

    if (rtc.lostPower()) {
        Serial.println(F("RTC lost power, setting time to compile datetime"));
        rtc.disableAlarm(2);
        rtc.clearAlarm(1);
        rtc.setAlarm1(DateTime(2025, 1, 1, FISH_FEED_HOUR), DS3231_A1_Hour);
        rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
    }

    Serial.println(F("[OK] RTC initialized"));
}

void setup_display() {
    Serial.println(F("Initializing display"));
    if (!display.begin(SSD1306_SWITCHCAPVCC, DISPLAY_ADDRESS)) {
        Serial.println(F("Failed to initialize display!"));
        halt();
    }
    Serial.println(F("[OK] Display initialized"));
}

void setup_servo() {
    Serial.println(F("Initializing servo"));
    servo.attach(SERVO_PIN);
    if (!servo.attached()) {
        Serial.println(F("Failed to initialize servo"));
        halt();
    }
    set_servo_degree(120);
    Serial.println(F("[OK] Servo initialized"));
}

void setup() {
    Serial.begin(9600);
    pinMode(FEED_BUTTON_PIN, INPUT);
    setup_rtc();
    setup_display();
    setup_servo();
    last_ir_update = millis();
}

void loop() {
    gather_data();
    update_ir();
    update_display();
    check_feeder();
    delay(1000);
}