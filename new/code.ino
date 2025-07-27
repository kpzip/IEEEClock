#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>

// Constants
#define TOP_LEN 14
#define BOTTOM_LEN 14
#define DIFF_LEN 10

#define SHIFT_DELAY_US 1

// Settings
#define USE_FULLER_6
#define LEADING_ZEROS

// Pin numbers
#define SEGMENT_CLK 8
#define DIGIT_CLK 9
#define OUTPUT_ENABLE 10
#define PUSH 11
#define SERIAL_DATA 12

#define INC 0
#define DEC 1
#define UP 2
#define DOWN 3
#define LEFT 4
#define RIGHT 5
#define SET_ENABLE 6
#define MODE 7

#define BRIGHTNESS_ADJ PIN_A0

// Values
// You would think i'm the secretary of state with how much state this simple clock has
const DateTime compile_time(__DATE__, __TIME__);
static DateTime now;
static DateTime travel_time = compile_time;
static TimeSpan difference;

static char top_line[TOP_LEN + 1] = { ' ' };
static char bottom_line[BOTTOM_LEN + 1] = { ' ' };
static char diff_line[DIFF_LEN + 1] = { ' ' };

// State
static bool is_setting_time = false;
static bool is_12_hr = false;
static uint8_t cursor_line = 0;
// 0 is top row, 1 is middle row, 2 is difference row
static uint8_t cursor_row = 0;
// 0 is rightmost side
static uint8_t cursor_column = 0;

static bool was_pressed = false;


RTC_DS1307 rtc; // DS1307 I2C address

void write_date_to_eeprom(DateTime date) {
  // TODO Redundancy
  EEPROM.put <uint32_t> (0, date.unixtime());
}

DateTime read_date_from_eeprom() {
  uint32_t utime;
  EEPROM.get <uint32_t> (0, utime);
  if (utime == 0) {
    return DateTime(compile_time);
  }
  return DateTime(utime);
}

void setup() {
  Serial.begin(9600);

  // Clock Data Pins
  pinMode(SEGMENT_CLK, OUTPUT);
  pinMode(DIGIT_CLK, OUTPUT);
  pinMode(OUTPUT_ENABLE, OUTPUT);
  pinMode(PUSH, OUTPUT);
  pinMode(SERIAL_DATA, OUTPUT);

  // Settings pins
  pinMode(INC, INPUT_PULLUP);
  pinMode(DEC, INPUT_PULLUP);
  pinMode(UP, INPUT_PULLUP);
  pinMode(DOWN, INPUT_PULLUP);
  pinMode(LEFT, INPUT_PULLUP);
  pinMode(RIGHT, INPUT_PULLUP);
  pinMode(SET_ENABLE, INPUT_PULLUP);
  pinMode(MODE, INPUT_PULLUP);

  digitalWrite(SEGMENT_CLK, LOW);
  digitalWrite(DIGIT_CLK, LOW);
  //digitalWrite(OUTPUT_ENABLE, HIGH);
  digitalWrite(PUSH, LOW);
  digitalWrite(SERIAL_DATA, LOW);

  auto second_time = travel_time;

  uint16_t bottom_year = second_time.year();
  uint8_t bottom_month = second_time.month();
  uint8_t bottom_day = second_time.day();
  uint8_t bottom_hour = second_time.hour();
  uint8_t bottom_minute = second_time.minute();
  uint8_t bottom_second = second_time.second();

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

  // Read the set time from EEPROM. If it is not set, this function returns the compile time
  travel_time = read_date_from_eeprom();

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

const bool ZERO_SEGMENTS[7] PROGMEM = {true, true, true, true, true, true, false};
const bool ONE_SEGMENTS[7] PROGMEM = {false, true, true, false, false, false, false};
const bool TWO_SEGMENTS[7] PROGMEM = {true, true, false, true, true, false, true};
const bool THREE_SEGMENTS[7] PROGMEM = {true, true, true, true, false, false, true};
const bool FOUR_SEGMENTS[7] PROGMEM = {false, true, true, false, false, true, true};
const bool FIVE_SEGMENTS[7] PROGMEM = {true, false, true, true, false, true, true};
const bool SIX_SEGMENTS[7] PROGMEM = {false, false, true, true, true, true, true};
const bool SEVEN_SEGMENTS[7] PROGMEM = {true, true, true, false, false, false, false};
const bool EIGHT_SEGMENTS[7] PROGMEM = {true, true, true, true, true, true, true};
const bool NINE_SEGMENTS[7] PROGMEM = {true, true, true, false, false, true, true};

bool c_has_segment(char c, char index) {

  if (index >= 7) {
    return false;
  }

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

const uint8_t daysInMonth[] PROGMEM = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

void change_row_by(int8_t change) {
  DateTime *row;
  uint16_t newyear;
  switch(cursor_row) {
  case 0:
    row = &now;
    break;
  case 1:
    row = &travel_time;
    break;
  case 2:
    row = &now;
    break;
  default:
    return;
  }
  switch(cursor_column) {
  case 0:
    // Seconds ones place
    *row = *row + TimeSpan(0, 0, 0, change);
    break;
  case 1:
    // Seconds tens place
    *row = *row + TimeSpan(0, 0, 0, 10 * change);
    break;
  case 2:
    // Minutes ones place
    *row = *row + TimeSpan(0, 0, change, 0);
    break;
  case 3:
    // Minutes tens place
    *row = *row + TimeSpan(0, 0, 10 * change, 0);
    break;
  case 4:
    // Hours ones place
    *row = *row + TimeSpan(0, change, 0, 0);
    break;
  case 5:
    // Hours tens place
    *row = *row + TimeSpan(0, 10 * change, 0, 0);
    break;
  case 6:
    // Days ones place
    *row = *row + TimeSpan(change, 0, 0, 0);
    break;
  case 7:
    // Days tens place
    *row = *row + TimeSpan(10 * change, 0, 0, 0);
    break;
  case 8:
    if (cursor_row == 2) {
      // Days hundreds place
      *row = *row + TimeSpan(100 * (int16_t)change, 0, 0, 0);
      break;
    }
    else {
      // Months ones place
      // TODO Leap years may change feb 29
      uint8_t day_adjust = row->day();
      uint8_t newmonth = row->month() + change;
      if (newmonth > 12) {
        newmonth = 1;
      }
      if (newmonth < 1) {
        newmonth = 12;
      }
      if (day_adjust > daysInMonth[newmonth - 1]) {
        day_adjust = daysInMonth[newmonth - 1];
      }
      *row = DateTime(row->year(), newmonth, day_adjust, row->hour(), row->minute(), row->second());
      return;
    }
    break;
  case 9:
    if (cursor_row == 2) {
      // Days thousands place
      *row = *row + TimeSpan(1000 * (int16_t)change, 0, 0, 0);
      break;
    }
    else {
      // Months tens place
      // Dont need this for now
      return;
    }
    break;
  case 10:
    // Years ones place
    // TODO Leap years may change feb 29
    newyear = row->year() + change;
    if (newyear > 2099) {
      newyear = 2099;
    }
    if (newyear < 2000) {
      newyear = 2000;
    }
    *row = DateTime(newyear, row->month(), row->day(), row->hour(), row->minute(), row->second());
    return;
  case 11:
    // Years tens place
    // TODO Leap years may change feb 29
    newyear = row->year() + 10 * change;
    if (newyear > 2099) {
      newyear = 2099;
    }
    if (newyear < 2000) {
      newyear = 2000;
    }
    *row = DateTime(newyear, row->month(), row->day(), row->hour(), row->minute(), row->second());
    return;
  case 12:
    // Years hundreds place
    // Unsupported by the library
  case 13:
    // Years thousands place
    // Unsupported by the library
  default:
    // years > 9999 are not supported
    return;
  }

  // Update saved values
  if (cursor_row == 1) {
    // Constant row, write to EEPROM
    write_date_to_eeprom(*row);
  }
  else {
    // Write to RTC
    rtc.adjust(*row);
  }
}

void loop() {
  // Read in pin data
  int brightness = 1023 - analogRead(BRIGHTNESS_ADJ);

  analogWrite(OUTPUT_ENABLE, brightness/4);

  
  is_12_hr = digitalRead(MODE) == LOW;
  is_setting_time = digitalRead(SET_ENABLE) == HIGH;
  if (is_setting_time) {
    // TODO Button holding & debounce

    bool inc = digitalRead(INC) == LOW;
    bool dec = digitalRead(DEC) == LOW;
    bool up = digitalRead(UP) == LOW;
    bool down = digitalRead(DOWN) == LOW;
    bool left = digitalRead(LEFT) == LOW;
    bool right = digitalRead(RIGHT) == LOW;
    
    if (!was_pressed) {
      if (up && cursor_row != 0) {
        cursor_row--;
      }
      else if (down && cursor_row != 2) {
        cursor_row++;
        if (cursor_row == 2 && cursor_column >= DIFF_LEN) {
          cursor_column = DIFF_LEN - 1;
        }
      }
      else if ((left && (cursor_column < BOTTOM_LEN - 1 && cursor_row < 2) || (cursor_column < DIFF_LEN - 1 && cursor_row == 2))) {
        cursor_column++;
      }
      else if (right && cursor_column < 0) {
        cursor_column--;
      }

      else if (inc) {
       change_row_by(1);
      }
      else if (dec) {
        change_row_by(-1);
      }
    }

    if (inc || dec || up || down || left || right ) {
      was_pressed = true;
    }
    else {
      was_pressed = false;
    }


  }
  
  bool is_12hr = digitalRead(MODE);
  bool is_top_pm = false;
  bool is_bottom_pm = false;
  
  now = rtc.now();
  auto second_time = travel_time;
  difference = now - second_time;

  // Top row led values
  uint16_t top_year = now.year();
  uint8_t top_month = now.month();
  uint8_t top_day = now.day();
  uint8_t top_hour = now.hour();
  if (is_12_hr && top_hour < 24 && top_hour > 11) {
    is_top_pm = true;
  }
  if (is_12_hr && top_hour > 12) {
    top_hour -= 12;
  }
  uint8_t top_minute = now.minute();
  uint8_t top_second = now.second();

  // Bottom row led values
  uint16_t bottom_year = second_time.year();
  uint8_t bottom_month = second_time.month();
  uint8_t bottom_day = second_time.day();
  uint8_t bottom_hour = second_time.hour();
  if (is_12_hr && bottom_hour < 24 && bottom_hour > 11) {
    is_bottom_pm = true;
  }
  if (is_12_hr && bottom_hour > 12) {
    bottom_hour -= 12;
  }
  uint8_t bottom_minute = second_time.minute();
  uint8_t bottom_second = second_time.second();

  // Difference
  uint16_t difference_days = difference.days();
  uint8_t difference_hours = difference.hours();
  uint8_t difference_minutes = difference.minutes();
  uint8_t difference_seconds = difference.seconds();


  sprintf(top_line, "%4d%2d%2d%2d%02d%02d", top_year, top_month, top_day, top_hour, top_minute, top_second);
  sprintf(bottom_line, "%4d%2d%2d%2d%02d%02d", bottom_year, bottom_month, bottom_day, bottom_hour, bottom_minute, bottom_second);
  sprintf(diff_line, "%4d%2d%02d%02d", difference_days, difference_hours, difference_minutes, difference_seconds);

  // For each segment
  for (int i = 0; i < 7; i++) {
    write_segment(i);
    // Top light and bottom light
    write_bit(is_bottom_pm, DIGIT_CLK);
    write_bit(is_top_pm, DIGIT_CLK);
    // diff row first
    for (int j = 0; j < DIFF_LEN; j++) {
      char c = diff_line[j];
      bool is_lit = c_has_segment(c, i);
      if (cursor_line == 2 && (DIFF_LEN - 1) - j == cursor_column && is_setting_time && i == 7) {
        is_lit = true;
      }
      write_bit(is_lit, DIGIT_CLK);
    }
    for (int j = 0; j < BOTTOM_LEN; j++) {
      char c = bottom_line[j];
      bool is_lit = c_has_segment(c, i);
      if (cursor_line == 1 && (BOTTOM_LEN - 1) - j == cursor_column && is_setting_time && i == 7) {
        is_lit = true;
      }
      write_bit(is_lit, DIGIT_CLK);
    }
    for (int j = 0; j < TOP_LEN; j++) {
      char c = top_line[j];
      bool is_lit = c_has_segment(c, i);
      if (cursor_line == 0 && (TOP_LEN - 1) - j == cursor_column && is_setting_time && i == 7) {
        is_lit = true;
      }
      write_bit(is_lit, DIGIT_CLK);
    }

    // Output the data on the shift registers
    digitalWrite(PUSH, HIGH);
    delayMicroseconds(SHIFT_DELAY_US);
    digitalWrite(PUSH, LOW);
    delayMicroseconds(SHIFT_DELAY_US);

    // Pwm is 500 Hz, so make sure we get a few cycles for each segment
    if (i != 7) delay(2);
  }
}
