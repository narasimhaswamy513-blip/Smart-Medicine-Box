#include <Wire.h>
#include "RTClib.h"
#include <WiFi.h>
#include <IOXhop_FirebaseESP32.h>

// ---------- Firebase Config ----------
#define FIREBASE_HOST "..................."
#define FIREBASE_AUTH "..................."
#define WIFI_SSID "......................."
#define WIFI_PASSWORD "..................."

// ---------- RTC ----------
RTC_DS3231 rtc;

// ---------- Hardware Pins ----------
#define BUZZER_PIN 15
#define BUTTON_PIN 4
int ledPins[4] = { 13, 5, 18, 19 };

// ---------- Tablet Data ----------
int tabletCount[4] = { 9, 6, 10, 10 };

// Tablet times (HH, MM)
int tabletHours[4] = { 12, 12, 12, 12 };
int tabletMinutes[4] = { 40, 41, 42, 50 };

bool ledStatus[4] = { false, false, false, false };
int lastAlertDay[4] = { -1, -1, -1, -1 };
bool universalAlertSent = false;

void setup() {
  Serial.begin(115200);
  Wire.begin();
  pinMode(BUZZER_PIN, OUTPUT);
  pinMode(BUTTON_PIN, INPUT_PULLUP);

  for (int i = 0; i < 4; i++) {
    pinMode(ledPins[i], OUTPUT);
    digitalWrite(ledPins[i], LOW);
  }

  // RTC setup
  if (!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    while (1)
      ;
  }
  if (rtc.lostPower()) {
    Serial.println("RTC lost power, setting to compile time...");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  // WiFi setup
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Connecting to Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nConnected!");

  // Firebase setup
  Firebase.begin(FIREBASE_HOST, FIREBASE_AUTH);
  Firebase.setString("/status", "TabletDispenserReady");
}

void loop() {
  DateTime now = rtc.now();
  bool anyAlertTriggered = false;

  // ----------- Check tablet schedule -----------
  for (int i = 0; i < 4; i++) {
    if (now.hour() == tabletHours[i] && now.minute() == tabletMinutes[i] && lastAlertDay[i] != now.day()) {

      digitalWrite(ledPins[i], HIGH);  // Turn on LED
      digitalWrite(BUZZER_PIN, HIGH);  // Turn on buzzer
      ledStatus[i] = true;
      lastAlertDay[i] = now.day();
      anyAlertTriggered = true;
    }
  }

  // ----------- Send universal alert if needed -----------
  if (anyAlertTriggered && !universalAlertSent) {
    Firebase.setString("/alert", "TabletTime!PleaseTake.");
    universalAlertSent = true;
  }

  // ----------- Button press handling -----------
  if (digitalRead(BUTTON_PIN) == LOW) {
    delay(300);  // debounce

    for (int i = 0; i < 4; i++) {
      if (ledStatus[i]) {
        digitalWrite(ledPins[i], LOW);
        ledStatus[i] = false;

        // Update tablet count
        if (tabletCount[i] > 0) {
          tabletCount[i]--;
          Serial.printf("Compartment %d: Tablet taken. Remaining: %d\n", i + 1, tabletCount[i]);

          // Update count in Firebase
          String countPath = "/count" + String(i + 1);
          Firebase.setInt(countPath, tabletCount[i]);
        }
      }
    }

    // Check if all compartments are cleared
    bool allCleared = true;
    for (int i = 0; i < 4; i++) {
      if (ledStatus[i]) {
        allCleared = false;
        break;
      }
    }

    if (allCleared) {
      digitalWrite(BUZZER_PIN, LOW);  // Turn off buzzer
      if (universalAlertSent) {
        Firebase.setString("/alert", "");  // Clear alert
        universalAlertSent = false;
      }
    }
  }

  // ----------- Print current time -----------
  int displayHour = now.hour() % 12;
  if (displayHour == 0) displayHour = 12;
  String ampm = (now.hour() >= 12) ? "PM" : "AM";

  char timeString[20];
  sprintf(timeString, "%02d:%02d:%02d %s", displayHour, now.minute(), now.second(), ampm.c_str());
  Serial.printf("Time: %s\n", timeString);

  delay(1000);
}
