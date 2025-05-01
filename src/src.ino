#include <Wire.h>
#include <RTClib.h>

RTC_DS1307 rtc; // DS1307 I2C address

void setup() {
  Serial.begin(9600);
  while (!Serial) {
    ; // Wait for serial port to connect (for safety)
  }
  if(!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  // Uncomment the following lines to set the time and date
  // rtc.adjust(DateTime(2025, 5, 1, 12, 0, 0)); // Set to May 1, 2025, 12:00:00
}

void loop() {
  DateTime now = rtc.now();

  Serial.print("Current Time: ");
  Serial.print(now.hour(), DEC);
  Serial.print(":");
  Serial.print(now.minute(), DEC);
  Serial.print(":");
  Serial.print(now.second(), DEC);
  Serial.print(" - ");

  Serial.print("Current Date: ");
  Serial.print(now.day(), DEC);
  Serial.print("/");
  Serial.print(now.month(), DEC);
  Serial.print("/");
  Serial.print(now.year(), DEC);
  Serial.println();

  delay(1000); // Wait 1 second
}
