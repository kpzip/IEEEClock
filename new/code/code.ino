#include <Wire.h>
#include <RTClib.h>

#define TOP_LEN 14
#define BOTTOM_LEN 14
#define DIFF_LEN 10

#define SEGMENT_CLK 8
#define DIGIT_CLK 9
#define OUTPUT_ENABLE 10
#define RESET 11
#define SERIAL_DATA 12

#define BRIGHTNESS_ADJ PIN_A0

#define SHIFT_DELAY_US 1

const DateTime compile_time(__DATE__, __TIME__);
static DateTime now;
static TimeSpan difference;

static char top_line[TOP_LEN + 1] = { 0 };
static char bottom_line[BOTTOM_LEN + 1] = { 0 };
static char diff_line[DIFF_LEN + 1] = { 0 };

RTC_DS1307 rtc; // DS1307 I2C address

void setup() {
  Serial.begin(9600);
  pinMode(SEGMENT_CLK, OUTPUT);
  pinMode(DIGIT_CLK, OUTPUT);
  pinMode(OUTPUT_ENABLE, OUTPUT);
  pinMode(RESET, OUTPUT);
  pinMode(SERIAL_DATA, OUTPUT);

  digitalWrite(SEGMENT_CLK, LOW);
  digitalWrite(DIGIT_CLK, LOW);
  digitalWrite(OUTPUT_ENABLE, HIGH);
  digitalWrite(RESET, LOW);
  digitalWrite(SERIAL_DATA, LOW);

  while (!Serial) {
    ; // Wait for serial port to connect (for safety)
  }
  if(!rtc.begin()) {
    Serial.println("Couldn't find RTC");
    Serial.flush();
    abort();
  }

  // Init
  now = rtc.now();
  difference = now - compile_time;

  // Uncomment the following lines to set the time and date
  //rtc.adjust(compile_time);
}

void write_bit(uint8_t bit, uint8_t clock_pin) {
  digitalWrite(SERIAL_DATA, bit);
  digitalWrite(clock_pin, HIGH);
  delayMicroseconds(SHIFT_DELAY_US);
  digitalWrite(clock_pin, LOW);
  delayMicroseconds(SHIFT_DELAY_US);
}

void write_segment(uint8_t segment_num) {
  for (int i = 0; i < 8; i++) {
    // Subtract 8 since we are writing to shift register
    // High and low are inverted because pnp
    if (i == (7 - segment_num)) {
      write_bit(0, SEGMENT_CLK);
    } else {
      write_bit(1, SEGMENT_CLK);
    }
  }
}

const bool ZERO_SEGMENTS[7] = {true, true, true, true, true, true, false};
const bool ONE_SEGMENTS[7] = {false, true, true, false, false, false, false};
const bool TWO_SEGMENTS[7] = {true, true, false, true, true, false, true};
const bool THREE_SEGMENTS[7] = {true, true, true, true, false, false, true};
const bool FOUR_SEGMENTS[7] = {false, true, true, false, false, true, true};
const bool FIVE_SEGMENTS[7] = {true, false, true, true, false, true, true};
const bool SIX_SEGMENTS[7] = {false, false, true, true, true, true, true};
const bool SEVEN_SEGMENTS[7] = {true, true, true, false, false, false, false};
const bool EIGHT_SEGMENTS[7] = {true, true, true, true, true, true, true};
const bool NINE_SEGMENTS[7] = {true, true, true, false, false, true, true};

bool c_has_segment(char c, char index) {

  if (c == ' ' || c < 48) {
    return false;
  }
  // ASCII offset
  uint8_t digit = c - 48;
  // Put these all in one array maybe?
  switch(digit) {
  case 0:
    return ZERO_SEGMENTS[index];
  case 1:
    return ONE_SEGMENTS[index];
  case 2:
    return TWO_SEGMENTS[index];
  case 3:
    return THREE_SEGMENTS[index];
  case 4:
    return FOUR_SEGMENTS[index];
  case 5:
    return FIVE_SEGMENTS[index];
  case 6:
    return SIX_SEGMENTS[index];
  case 7:
    return SEVEN_SEGMENTS[index];
  case 8:
    return EIGHT_SEGMENTS[index];
  case 9:
    return NINE_SEGMENTS[index];
  }
  return false;
}



void loop() {
  int brightness = 1024 - analogRead(BRIGHTNESS_ADJ);
  //analogWrite(OUTPUT_ENABLE, brightness/4);
  now = rtc.now();
  difference = now - compile_time;
  sprintf(top_line, "%4d%2d%2d%2d%2d%2d", now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
  sprintf(bottom_line, "%4d%2d%2d%2d%2d%2d", compile_time.year(), compile_time.month(), compile_time.day(), compile_time.hour(), compile_time.minute(), compile_time.second());
  sprintf(diff_line, "%4d%2d%2d%2d", difference.days(), difference.hours(), difference.minutes(), difference.seconds());

  // For each segment
  for (int i = 0; i < 7; i++) {
    write_segment(i);
    // Top light and bottom light
    write_bit(0, DIGIT_CLK);
    write_bit(1, DIGIT_CLK);
    // diff row first
    for (int j = 0; j < DIFF_LEN; j++) {
      char c = diff_line[j];
      bool is_lit = c_has_segment(c, i);
      write_bit(is_lit, DIGIT_CLK);
    }
    for (int j = 0; j < BOTTOM_LEN; j++) {
      char c = bottom_line[j];
      bool is_lit = c_has_segment(c, i);
      write_bit(is_lit, DIGIT_CLK);
    }
    for (int j = 0; j < TOP_LEN; j++) {
      char c = top_line[j];
      bool is_lit = c_has_segment(c, i);
      write_bit(is_lit, DIGIT_CLK);
    }

    //delayMicroseconds(2000);
    // Wait for output to go low in order to avoid the lights blinking. REALLY shitty way of doing this but it works with PWM.
    // int counter = 0;
    // while (digitalRead(OUTPUT_ENABLE)) {
    //   delayMicroseconds(1);
    //   if (counter > 1000) break;
    //   counter += 1;
    // }
    long time_on = brightness;
    //long clocks_on = brightness / 4;
    long time_off = 1024 - time_on;
    if (time_off >= 100) {
      time_off -= 100;
    } else {
      time_off = 0;
    }
    digitalWrite(OUTPUT_ENABLE, LOW);
    delayMicroseconds(time_on);
    digitalWrite(OUTPUT_ENABLE, HIGH);
    delayMicroseconds(time_off);

    // Reset the shift registers
    digitalWrite(RESET, HIGH);
    delayMicroseconds(SHIFT_DELAY_US);
    digitalWrite(RESET, LOW);
    delayMicroseconds(SHIFT_DELAY_US);
  }
}
