#include <Adafruit_NeoPixel.h>
#include <EEPROM.h>
#include <Wire.h>
#include <RTClib.h>
#include <SoftwareSerial.h> // For Bluetooth module

#define LDR_PIN A0
#define LED_PIN 6

#define Blue 170
#define Yellow 70
#define Green 90
#define Orange 50
#define Cyan 150
#define Pink 180
#define Red 0

// Bluetooth setup
#define BT_TX 11 // Bluetooth TX pin
#define BT_RX 10 // Bluetooth RX pin
SoftwareSerial Bluetooth(BT_TX, BT_RX); // RX, TX

Adafruit_NeoPixel strip = Adafruit_NeoPixel(24, LED_PIN, NEO_GRB + NEO_KHZ800);
RTC_DS3231 rtc;

enum State {
    WAITING,
    SETTINGS_MENU,
    ALARM_MENU,
    SET_ALARM_HOUR,
    SET_ALARM_MINUTE,
    SET_ALARM_COLOR,
    SET_SLEEP_HOUR,
    SET_SLEEP_MINUTE,
    SET_SLEEP_COLOR,
    CONTROL_LAMP,
    SELECT_COLOR_COMBO,
    SELECT_DURATION,
    SET_DATE_YEAR,
    SET_DATE_MONTH,
    SET_DATE_DAY,
    SET_DATE_HOUR,
    SET_DATE_MINUTE
};

State currentState = WAITING;

int alarmHour = -1;
int alarmMinute = -1;
byte alarmColor = Yellow;
unsigned long alarmTime = 0;

bool alarmActive = false;
bool sleepingModeActive = false;
int sleepHour = -1;
int sleepMinute = -1;
byte sleepColor = Yellow;
DateTime sleepEndTime = DateTime(0, 0, 0, 0, 0, 0); // Initialize with zeroes

bool lampControlActive = false;
bool lampOn = true;  // New variable to track lamp state
byte colorCombo[7] = {Yellow, Orange, Blue}; // Default color combo
int comboLength = 3;
int duration = 0;

int setYear = -1;
int setMonth = -1;
int setDay = -1;
int setHour = -1;
int setMinute = -1;

void setup() {
    strip.begin();
    strip.show();
    Serial.begin(9600);

    if (!rtc.begin()) {
        Serial.println("Couldn't find RTC");
        while (1);
    }

    if (rtc.lostPower()) {
        Serial.println("RTC lost power, setting the time!");
    }

    // Bluetooth setup
    Bluetooth.begin(9600);
    Serial.println("Bluetooth Initialized");
    Serial.println("Welcome to Mood Lamp Setting. Please enter 'g'.");
}

void loop() {
    handleBluetoothInput();
    checkAlarm();
    checkSleepingMode();

    if (!alarmActive && !sleepingModeActive && !lampControlActive) {
        int lightvalue = analogRead(LDR_PIN);
        Serial.print("Light value: ");
        Serial.println(lightvalue);

        DateTime now = rtc.now();
        Serial.print(now.year(), DEC);
        Serial.print('/');
        Serial.print(now.month(), DEC);
        Serial.print('/');
        Serial.print(now.day(), DEC);
        Serial.print(" ");
        Serial.print(now.hour(), DEC);
        Serial.print(':');
        Serial.print(now.minute(), DEC);
        Serial.print(':');
        Serial.print(now.second(), DEC);
        Serial.println();

        if (lampOn) {  // Only set color if the lamp is on
            if (lightvalue < 350) {
                setColor(Pink);
            } else if (lightvalue < 400) {
                setColor(Yellow);
            } else if (lightvalue < 450) {
                setColor(Orange);
            } else if (lightvalue < 500) {
                setColor(Cyan);
            } else if (lightvalue < 550) {
                setColor(Blue);
            } else if (lightvalue < 600) {
                setColor(Green);
            } else {
                setColor(Red);
            }
        } else {
            strip.clear();  // Turn off the LEDs
            strip.show();
        }

        delay(1000);
    } else if (lampControlActive) {
        unsigned long startMillis = millis();
        while (millis() - startMillis < duration * 60000) { // Run for the selected duration
            for (int i = 0; i < comboLength; i++) {
                setColor(colorCombo[i]);
                delay(3000); // Show each color for 3 seconds
            }
        }
        lampControlActive = false;
    }
}

void handleBluetoothInput() {
    if (Bluetooth.available()) {
        char input = Bluetooth.read();
        switch (currentState) {
            case WAITING:
                if (input == 'g') {
                    currentState = SETTINGS_MENU;
                    Bluetooth.println("1-> Set Alarm");
                    Bluetooth.println("2-> Control Lamp");
                    Bluetooth.println("3-> Set Date and Time");
                }
                break;
            case SETTINGS_MENU:
                if (input == '1') {
                    currentState = ALARM_MENU;
                    Bluetooth.println("Setting Alarm");
                    Bluetooth.println("1-> Wake Up Alarm");
                    Bluetooth.println("2-> Sleeping Mood");
                    Bluetooth.println("x-> Return to Main Menu");
                } else if (input == '2') {
                    currentState = CONTROL_LAMP;
                    Bluetooth.println("Control Lamp");
                    Bluetooth.println("1-> Select Color Combo");
                    Bluetooth.println("2-> On/Off the Lamp");  // New option for lamp control
                    Bluetooth.println("x-> Return to Main Menu");
                } else if (input == '3') {
                    currentState = SET_DATE_YEAR;
                    Bluetooth.println("Enter Year (4 digits):");
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case CONTROL_LAMP:
                if (input == '1') {
                    currentState = SELECT_COLOR_COMBO;
                    Bluetooth.println("Select Color Combo:");
                    Bluetooth.println("1-> Pink, Orange, Blue");
                    Bluetooth.println("2-> Cyan, Yellow, Bluel");
                    Bluetooth.println("3-> Red, Pink, Green");
                    Bluetooth.println("4-> Cyan, Orange, Green");
                    Bluetooth.println("5-> Rainbow");
                    Bluetooth.println("x-> Return to Main Menu");
                } else if (input == '2') {  // Toggle lamp on/off
                    lampOn = !lampOn;  // Toggle the lamp state
                    if (lampOn) {
                        Bluetooth.println("Lamp turned ON");
                    } else {
                        Bluetooth.println("Lamp turned OFF");
                        strip.clear();
                        strip.show();  // Turn off LEDs immediately
                    }
                    currentState = WAITING;
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case SELECT_COLOR_COMBO:
                if (input >= '1' && input <= '5') {
                    switch (input) {
                        case '1':
                            colorCombo[0] = Pink;
                            colorCombo[1] = Orange;
                            colorCombo[2] = Blue;
                            comboLength = 3;
                            break;
                        case '2':
                            colorCombo[0] = Cyan;
                            colorCombo[1] = Yellow;
                            colorCombo[2] = Blue;
                            comboLength = 3;
                            break;
                        case '3':
                            colorCombo[0] = Red;
                            colorCombo[1] = Pink;
                            colorCombo[2] = Green;
                            comboLength = 3;
                            break;
                        case '4':
                            colorCombo[0] = Cyan;
                            colorCombo[1] = Orange;
                            colorCombo[2] = Green;
                            comboLength = 3;
                            break;
                        case '5':
                            colorCombo[0] = Yellow;
                            colorCombo[1] = Orange;
                            colorCombo[2] = Blue;
                            colorCombo[3] = Green;
                            colorCombo[4] = Pink;
                            colorCombo[5] = Cyan;
                            colorCombo[6] = Red;
                            comboLength = 7;
                            break;
                    }
                    currentState = SELECT_DURATION;
                    Bluetooth.println("Select Duration:");
                    Bluetooth.println("1-> 5 minutes");
                    Bluetooth.println("2-> 15 minutes");
                    Bluetooth.println("3-> 30 minutes");
                    Bluetooth.println("4-> 45 minutes");
                    Bluetooth.println("5-> 60 minutes");
                    Bluetooth.println("x-> Return to Main Menu");
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case SELECT_DURATION:
                if (input >= '1' && input <= '5') {
                    switch (input) {
                        case '1':
                            duration = 5;
                            break;
                        case '2':
                            duration = 15;
                            break;
                        case '3':
                            duration = 30;
                            break;
                        case '4':
                            duration = 45;
                            break;
                        case '5':
                            duration = 60;
                            break;
                    }
                    Bluetooth.println("Color Combo and Duration Set");
                    lampControlActive = true; // Set lamp control mode active
                    currentState = WAITING;
                    Bluetooth.println("x-> Return to Main Menu.");
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case ALARM_MENU:
                if (input == '1') {
                    currentState = SET_ALARM_HOUR;
                    Bluetooth.println("Waking Up Alarm");
                    Bluetooth.println("Enter Hour: ");
                } else if (input == '2') {
                    currentState = SET_SLEEP_HOUR;
                    Bluetooth.println("Sleeping Mood");
                    Bluetooth.println("Enter Hour: ");
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case SET_ALARM_HOUR:
                if (input >= '0' && input <= '9') {
                    int value = input - '0';
                    if (alarmHour == -1) {
                        alarmHour = value;
                    } else {
                        alarmHour = alarmHour * 10 + value;
                        currentState = SET_ALARM_MINUTE;
                        Bluetooth.println("Enter Minute: ");
                    }
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case SET_ALARM_MINUTE:
                if (input >= '0' && input <= '9') {
                    int value = input - '0';
                    if (alarmMinute == -1) {
                        alarmMinute = value;
                    } else {
                        alarmMinute = alarmMinute * 10 + value;
                        currentState = SET_ALARM_COLOR;
                        Bluetooth.println("Set Color: ");
                        Bluetooth.println("1-> Red");
                        Bluetooth.println("2-> Orange");
                        Bluetooth.println("3-> Yellow");
                        Bluetooth.println("4-> Green");
                        Bluetooth.println("5-> Cyan");
                        Bluetooth.println("6-> Blue");
                        Bluetooth.println("7-> Pink");
                    }
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case SET_ALARM_COLOR:
                if (input >= '1' && input <= '7') {
                    alarmColor = (input - '1') * 30 + 1;
                    Bluetooth.println("Alarm Set");
                    currentState = WAITING;
                    alarmActive = true; // Set the alarm as active
                    // Show return to main menu message
                    Bluetooth.println("x-> Return to Main Menu");
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case SET_SLEEP_HOUR:
                if (input >= '0' && input <= '9') {
                    int value = input - '0';
                    if (sleepHour == -1) {
                        sleepHour = value;
                    } else {
                        sleepHour = sleepHour * 10 + value;
                        currentState = SET_SLEEP_MINUTE;
                        Bluetooth.println("Enter Minute: ");
                    }
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case SET_SLEEP_MINUTE:
                if (input >= '0' && input <= '9') {
                    int value = input - '0';
                    if (sleepMinute == -1) {
                        sleepMinute = value;
                    } else {
                        sleepMinute = sleepMinute * 10 + value;
                        currentState = SET_SLEEP_COLOR;
                         Bluetooth.println("Set Color: ");
                        Bluetooth.println("1-> Red");
                        Bluetooth.println("2-> Orange");
                        Bluetooth.println("3-> Yellow");
                        Bluetooth.println("4-> Green");
                        Bluetooth.println("5-> Cyan");
                        Bluetooth.println("6-> Blue");
                        Bluetooth.println("7-> Pink");
                    }
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case SET_SLEEP_COLOR:
                if (input >= '1' && input <= '7') {
                    sleepColor = (input - '1') * 30 + 1;
                    sleepEndTime = rtc.now();
                    sleepEndTime = DateTime(sleepEndTime.year(), sleepEndTime.month(), sleepEndTime.day(), sleepHour, sleepMinute, 0);
                    Bluetooth.println("Sleeping Mood Set");
                    currentState = WAITING;
                    sleepingModeActive = true; // Set sleeping mode as active
                    // Set the LED to the selected color
                    setColor(sleepColor);
                    // Show return to main menu message
                    Bluetooth.println("x-> Return to Main Menu.");
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case SET_DATE_YEAR:
                if (input >= '0' && input <= '9') {
                    int value = input - '0';
                    if (setYear == -1) {
                        setYear = value;
                    } else {
                        setYear = setYear * 10 + value;
                        if (setYear >= 2000 && setYear <= 2099) { // Assuming a year range for validation
                            currentState = SET_DATE_MONTH;
                            Bluetooth.println("Enter Month (2 digits):");
                        }
                    }
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case SET_DATE_MONTH:
                if (input >= '0' && input <= '9') {
                    int value = input - '0';
                    if (setMonth == -1) {
                        setMonth = value;
                    } else {
                        setMonth = setMonth * 10 + value;
                        if (setMonth >= 1 && setMonth <= 12) { // Valid month range
                            currentState = SET_DATE_DAY;
                            Bluetooth.println("Enter Day (2 digits):");
                        }
                    }
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case SET_DATE_DAY:
                if (input >= '0' && input <= '9') {
                    int value = input - '0';
                    if (setDay == -1) {
                        setDay = value;
                    } else {
                        setDay = setDay * 10 + value;
                        if (setDay >= 1 && setDay <= 31) { // Simplified validation, should handle month-specific day limits
                            currentState = SET_DATE_HOUR;
                            Bluetooth.println("Enter Hour (24 format, 2 digits):");
                        }
                    }
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case SET_DATE_HOUR:
                if (input >= '0' && input <= '9') {
                    int value = input - '0';
                    if (setHour == -1) {
                        setHour = value;
                    } else {
                        setHour = setHour * 10 + value;
                        if (setHour >= 0 && setHour < 24) { // Valid hour range
                            currentState = SET_DATE_MINUTE;
                            Bluetooth.println("Enter Minute (2 digits):");
                        }
                    }
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
            case SET_DATE_MINUTE:
                if (input >= '0' && input <= '9') {
                    int value = input - '0';
                    if (setMinute == -1) {
                        setMinute = value;
                    } else {
                        setMinute = setMinute * 10 + value;
                        if (setMinute >= 0 && setMinute < 60) { // Valid minute range
                            rtc.adjust(DateTime(setYear, setMonth, setDay, setHour, setMinute, 0));
                            Bluetooth.println("Date set, Enter x");
                            setYear = -1;
                            setMonth = -1;
                            setDay = -1;
                            setHour = -1;
                            setMinute = -1;
                            currentState = WAITING;
                        }
                    }
                } else if (input == 'x') {
                    currentState = WAITING;
                    Bluetooth.println("Returned to Main Menu.");
                }
                break;
        }
    }
}

void checkAlarm() {
    if (alarmHour != -1 && alarmMinute != -1 && alarmActive) {
        DateTime now = rtc.now();
        if (now.hour() == alarmHour && now.minute() == alarmMinute) {
            setColor(alarmColor);
            delay(60000); // Keep the color for 1 minute
            alarmHour = -1;
            alarmMinute = -1;
            alarmActive = false;
            Bluetooth.println("Alarm triggered");
        }
    }
}

void checkSleepingMode() {
    if (sleepingModeActive) {
        DateTime now = rtc.now();
        if (now >= sleepEndTime) {
            sleepingModeActive = false;
            setColor(Red); // Reset to default color
            Bluetooth.println("Sleeping Mode Ended");
        }
    }
}

void setColor(byte color) {
    uint32_t colorValue = Wheel(color);
    for (int i = 0; i < strip.numPixels(); i++) {
        strip.setPixelColor(i, colorValue);
    }
    strip.show();
}

uint32_t Wheel(byte WheelPos) {
    WheelPos = 255 - WheelPos;
    if (WheelPos < 85) {
        return strip.Color((255 - WheelPos * 3), 0, (WheelPos * 3));
    }
    if (WheelPos < 170) {
        WheelPos -= 85;
        return strip.Color(0, (WheelPos * 3), (255 - WheelPos * 3));
    }
    WheelPos -= 170;
    return strip.Color((WheelPos * 3), (255 - WheelPos * 3), 0);
}