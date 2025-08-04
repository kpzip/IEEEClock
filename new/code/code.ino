#include <Wire.h>
#include <RTClib.h>
#include <EEPROM.h>

// Constants
#define TOP_LEN 14
#define BOTTOM_LEN 14
#define DIFF_LEN 10

//#define SHIFT_DELAY_US 1

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
static volatile DateTime now;
static volatile DateTime new_time;
static volatile DateTime travel_time = compile_time;
static volatile TimeSpan difference;

static volatile char top_line[TOP_LEN + 1] = { 0 };
static volatile char bottom_line[BOTTOM_LEN + 1] = { 0 };
static volatile char diff_line[DIFF_LEN + 1] = { 0 };

static volatile int display_counter = 0;
static volatile uint8_t display_current_segment = 0;

// State
static volatile bool is_setting_time = false;
static volatile bool is_12_hr = false;
// 0 is top row, 1 is middle row, 2 is difference row
static volatile uint8_t cursor_row = 0;
// 0 is rightmost side
static volatile uint8_t cursor_column = 0;

static volatile bool is_top_pm = false;
static volatile bool is_bottom_pm = false;

static volatile bool rtc_needs_adjusting = false;
static volatile bool eeprom_needs_writing = false;

// Blinking
static volatile bool cursor_is_lit = true;
static volatile int cursor_blink_counter = 0;

static volatile bool inc_is_pressed = false;
static volatile bool inc_press_handled = false;
static volatile int inc_pressed_time = 0;

static volatile bool dec_is_pressed = false;
static volatile bool dec_press_handled = false;
static volatile int dec_pressed_time = 0;

static volatile bool up_is_pressed = false;
static volatile bool up_press_handled = false;
static volatile int up_pressed_time = 0;

static volatile bool down_is_pressed = false;
static volatile bool down_press_handled = false;
static volatile int down_pressed_time = 0;

static volatile bool left_is_pressed = false;
static volatile bool left_press_handled = false;
static volatile int left_pressed_time = 0;

static volatile bool right_is_pressed = false;
static volatile bool right_press_handled = false;
static volatile int right_pressed_time = 0;

static volatile RTC_DS1307 rtc; // DS1307 I2C address

// Bruh
// volatile DateTime operator=(const DateTime& other) {
//   return *this;
// }

// volatile DateTime operator=(const volatile DateTime& other) {
//   return *this;
// }

void write_date_to_eeprom(DateTime& date) {
  // TODO Redundancy
  EEPROM.put <uint32_t> (0, date.unixtime());
}

DateTime read_date_from_eeprom() {
  // uint32_t utime;
  // EEPROM.get <uint32_t> (0, utime);
  // if (utime == 0) {
  //   return compile_time;
  // }
  // return DateTime(utime);
  return compile_time;
}

const uint8_t daysInMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };

void change_row_by(int8_t change) {
  DateTime *row;
  uint16_t newyear;
  switch(cursor_row) {
  case 0:
  case 2:
    new_time = DateTime(&now);
    row = &new_time;
    break;
  case 1:
    row = &travel_time;
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
    eeprom_needs_writing = true;
  }
  else {
    // Write to RTC
    // Maybe bad to use I2C inside an ISR
    //rtc.adjust(*row);
    rtc_needs_adjusting = true;
  }
}

void inc() {
  change_row_by(1);
}

void dec() {
  change_row_by(-1);
}

void up() {
  if (cursor_row > 0) {
    cursor_row--;
  }
}

void down() {
  if (cursor_row < 2) {
    cursor_row++;
  }
  if (cursor_row == 2 && cursor_column >= DIFF_LEN) {
    cursor_column = DIFF_LEN - 1;
  }
}

void left() {
  if ((cursor_row == 0 && cursor_column < TOP_LEN - 1) || (cursor_row == 1 && cursor_column < BOTTOM_LEN - 1) || (cursor_row == 2 && cursor_column < DIFF_LEN - 1)) {
    cursor_column++;
  }
}

void right() {
  if (cursor_column > 0) {
    cursor_column--;
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

bool c_has_segment(char c, uint8_t index) {

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

#define DISPLAY_TIMER 1

void write_bit(uint8_t bit, uint8_t clock_pin) {
  digitalWrite(SERIAL_DATA, bit);
  digitalWrite(clock_pin, HIGH);
  digitalWrite(clock_pin, LOW);
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

void display_logic() {

  write_segment(display_current_segment);
  // Top light and bottom light
  write_bit(is_bottom_pm, DIGIT_CLK);
  write_bit(is_top_pm, DIGIT_CLK);
  // diff row first
  for (int j = 0; j < DIFF_LEN; j++) {
    char c = diff_line[j];
    bool is_lit = c_has_segment(c, display_current_segment);
    if (cursor_row == 2 && (DIFF_LEN - 1) - j == cursor_column && is_setting_time) {
      if (c == ' ' && display_current_segment == 7) {
        is_lit = true;
      }
      if (!cursor_is_lit) {
        is_lit = false;
      }
    }
    write_bit(is_lit, DIGIT_CLK);
  }
  for (int j = 0; j < BOTTOM_LEN; j++) {
    char c = bottom_line[j];
    bool is_lit = c_has_segment(c, display_current_segment);
    if (cursor_row == 1 && (BOTTOM_LEN - 1) - j == cursor_column && is_setting_time) {
      if (c == ' ' && display_current_segment == 7) {
        is_lit = true;
      }
      if (!cursor_is_lit) {
        is_lit = false;
      }
    }
    write_bit(is_lit, DIGIT_CLK);
  }
  for (int j = 0; j < TOP_LEN; j++) {
    char c = top_line[j];
    bool is_lit = c_has_segment(c, display_current_segment);
    if (cursor_row == 0 && (TOP_LEN - 1) - j == cursor_column && is_setting_time) {
      if (c == ' ' && display_current_segment == 7) {
        is_lit = true;
      }
      if (!cursor_is_lit) {
        is_lit = false;
      }
    }
    write_bit(is_lit, DIGIT_CLK);
  }

  // Output the data on the shift registers
  digitalWrite(PUSH, HIGH);
  digitalWrite(PUSH, LOW);

  display_current_segment++;
  if (display_current_segment > 7) {
    display_current_segment = 0;
  }
}

// Timer 2 used for button shenanigans
ISR(TIMER2_OVF_vect) {
  if (display_counter < DISPLAY_TIMER) {
    display_counter++;
  }
  else {
    display_counter = 0;
    display_logic();
  }
  //display_logic();
}

// number of 1ms intervals
const int BUTTON_GRACE_PERIOD = 2 * 1000; // 2 seconds
const int BUTTON_REPRESS_PERIOD = 1000 / 4; // 0.5 seconds

// number of 1ms intervals
const int BUTTON_BLINK_PERIOD = 1000 / 4; // 0.5 seconds (really this is half of the period)

bool counter_logic(const volatile bool *is_pressed, volatile int *counter) {
  if (*is_pressed) {
    if (*counter < (BUTTON_GRACE_PERIOD + BUTTON_REPRESS_PERIOD)) {
      *counter += 1;
    }
    else {
      *counter = BUTTON_GRACE_PERIOD;
      return true;
    }
  }
  else {
    *counter = 0;
  }
  return false;
}

ISR(TIMER2_COMPA_vect) {
  OCR2A += 250; // Increment COMPA to keep uniform time
  // Increment counters
  if (is_setting_time) {
    if (counter_logic(&inc_is_pressed, &inc_pressed_time)) {
      // Increment
      inc();
    }
    if (counter_logic(&dec_is_pressed, &dec_pressed_time)) {
      // Decrement
      dec();
    }
    if (counter_logic(&up_is_pressed, &up_pressed_time)) {
      // Up
      up();
    }
    if (counter_logic(&down_is_pressed, &down_pressed_time)) {
      // Down
      down();
    }
    if (counter_logic(&left_is_pressed, &left_pressed_time)) {
      // Left
      left();
    }
    if (counter_logic(&right_is_pressed, &right_pressed_time)) {
      // Right
      right();
    }
  }

  if (cursor_blink_counter < BUTTON_BLINK_PERIOD) {
    cursor_blink_counter++;
  }
  else {
    cursor_blink_counter = 0;
    cursor_is_lit = !cursor_is_lit;
  }

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

  // Init Hardware Timers
  TCCR2A = 0;           // Init Timer2A
  TCCR2B = 0;           // Init Timer2B
  TCCR2B |= B00000100;  // Prescaler = 64
  OCR2A = 250;          // Timer Compare2A Register
  TIMSK2 |= B00000011;  // Enable Timer COMPA and OVF Interrupt

  // TCCR0A = 0;           // Init Timer0A
  // TCCR0B = 0;           // Init Timer0B
  // TCCR0B |= B00000100;  // Prescaler = 256
  // TCNT0 = 0;            // Timer Preloading
  // TIMSK0 |= B00000001;  // Enable Timer Overflow Interrupt

  // const DateTime& second_time = &travel_time;

  // uint16_t bottom_year = second_time.year();
  // uint8_t bottom_month = second_time.month();
  // uint8_t bottom_day = second_time.day();
  // uint8_t bottom_hour = second_time.hour();
  // uint8_t bottom_minute = second_time.minute();
  // uint8_t bottom_second = second_time.second();

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

bool should_activate(const volatile bool *is_pressed, volatile bool *is_press_handled, volatile int *counter) {
  if (*is_pressed && !*is_press_handled) {
    *is_press_handled = true;
    return true;
  }
  else if (!*is_pressed) {
    *counter = 0;
    *is_press_handled = false;
  }
  return false;
}

void handle_button_presses() {
  if (should_activate(&inc_is_pressed, &inc_press_handled, &inc_pressed_time)) {
    // Increment
    inc();
  }
  if (should_activate(&dec_is_pressed, &dec_press_handled, &dec_pressed_time)) {
    // Decrement
    dec();
  }
  if (should_activate(&up_is_pressed, &up_press_handled, &up_pressed_time)) {
    // Move up
    up();
  }
  if (should_activate(&down_is_pressed, &down_press_handled, &down_pressed_time)) {
    // Move Down
    down();
  }
  if (should_activate(&left_is_pressed, &left_press_handled, &left_pressed_time)) {
    // Move Left
    left();
  }
  if (should_activate(&right_is_pressed, &right_press_handled, &right_pressed_time)) {
    // Move Right
    right();
  }
}

void loop() {
  // Read in pin data
  int brightness = 1023 - analogRead(BRIGHTNESS_ADJ);
  analogWrite(OUTPUT_ENABLE, brightness/4);

  inc_is_pressed = digitalRead(INC) == LOW;
  dec_is_pressed = digitalRead(DEC) == LOW;
  up_is_pressed = digitalRead(UP) == LOW;
  down_is_pressed = digitalRead(DOWN) == LOW;
  left_is_pressed = digitalRead(LEFT) == LOW;
  right_is_pressed = digitalRead(RIGHT) == LOW;

  is_setting_time = digitalRead(SET_ENABLE) == HIGH;
  is_12_hr = digitalRead(MODE) == LOW;

  // Handle single button presses
  if (is_setting_time) {
    handle_button_presses();
  }

  // Refresh time data

  if (eeprom_needs_writing) {
    write_date_to_eeprom(&travel_time);
    eeprom_needs_writing = false;
  }
  
  if (rtc_needs_adjusting) {
    rtc.adjust(&new_time);
    rtc_needs_adjusting = false;
  }
  now = rtc.now();
  const DateTime& second_time = &travel_time;
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

  // add 48 to get the right ascii char
  // maybe make this not use ascii in the future
  char top_year_thousands = ((top_year / 1000) % 10) + 48;
  char top_year_hundreds = ((top_year / 100) % 10) + 48;
  char top_year_tens = ((top_year / 10) % 10) + 48;
  char top_year_ones = (top_year % 10) + 48;

  char top_month_tens = top_month < 10 ? ' ' : ((top_month / 10) % 10) + 48;
  char top_month_ones = (top_month % 10) + 48;

  char top_day_tens = top_day < 10 ? ' ' : ((top_day / 10) % 10) + 48;
  char top_day_ones = (top_day % 10) + 48;

  char top_hour_tens = top_hour < 10 ? ' ' : ((top_hour / 10) % 10) + 48;
  char top_hour_ones = (top_hour % 10) + 48;

  char top_minute_tens = ((top_minute / 10) % 10) + 48;
  char top_minute_ones = (top_minute % 10) + 48;

  char top_second_tens = ((top_second / 10) % 10) + 48;
  char top_second_ones = (top_second % 10) + 48;

  top_line[0] = top_year_thousands;
  top_line[1] = top_year_hundreds;
  top_line[2] = top_year_tens;
  top_line[3] = top_year_ones;

  top_line[4] = top_month_tens;
  top_line[5] = top_month_ones;

  top_line[6] = top_day_tens;
  top_line[7] = top_day_ones;

  top_line[8] = top_hour_tens;
  top_line[9] = top_hour_ones;

  top_line[10] = top_minute_tens;
  top_line[11] = top_minute_ones;

  top_line[12] = top_second_tens;
  top_line[13] = top_second_ones;

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

  // add 48 to get the right ascii char
  // maybe make this not use ascii in the future
  char bottom_year_thousands = ((bottom_year / 1000) % 10) + 48;
  char bottom_year_hundreds = ((bottom_year / 100) % 10) + 48;
  char bottom_year_tens = ((bottom_year / 10) % 10) + 48;
  char bottom_year_ones = (bottom_year % 10) + 48;

  char bottom_month_tens = bottom_month < 10 ? ' ' : ((bottom_month / 10) % 10) + 48;
  char bottom_month_ones = (bottom_month % 10) + 48;

  char bottom_day_tens = bottom_day < 10 ? ' ' : ((bottom_day / 10) % 10) + 48;
  char bottom_day_ones = (bottom_day % 10) + 48;

  char bottom_hour_tens = bottom_hour < 10 ? ' ' : ((bottom_hour / 10) % 10) + 48;
  char bottom_hour_ones = (bottom_hour % 10) + 48;

  char bottom_minute_tens = ((bottom_minute / 10) % 10) + 48;
  char bottom_minute_ones = (bottom_minute % 10) + 48;

  char bottom_second_tens = ((bottom_second / 10) % 10) + 48;
  char bottom_second_ones = (bottom_second % 10) + 48;

  bottom_line[0] = bottom_year_thousands;
  bottom_line[1] = bottom_year_hundreds;
  bottom_line[2] = bottom_year_tens;
  bottom_line[3] = bottom_year_ones;

  bottom_line[4] = bottom_month_tens;
  bottom_line[5] = bottom_month_ones;

  bottom_line[6] = bottom_day_tens;
  bottom_line[7] = bottom_day_ones;

  bottom_line[8] = bottom_hour_tens;
  bottom_line[9] = bottom_hour_ones;

  bottom_line[10] = bottom_minute_tens;
  bottom_line[11] = bottom_minute_ones;

  bottom_line[12] = bottom_second_tens;
  bottom_line[13] = bottom_second_ones;

  // Difference
  uint16_t difference_days = difference.days();
  uint8_t difference_hours = difference.hours();
  uint8_t difference_minutes = difference.minutes();
  uint8_t difference_seconds = difference.seconds();

  // add 48 to get the right ascii char
  // maybe make this not use ascii in the future
  char difference_day_thousands = difference_days < 1000 ? ' ' : ((difference_days / 1000) % 10) + 48;
  char difference_day_hundreds = difference_days < 100 ? ' ' : ((difference_days / 100) % 10) + 48;
  char difference_day_tens = difference_days < 10 ? ' ' : ((difference_days / 10) % 10) + 48;
  char difference_day_ones = (difference_days % 10) + 48;

  char difference_hour_tens = difference_hours < 10 ? ' ' : ((difference_hours / 10) % 10) + 48;
  char difference_hour_ones = (difference_hours % 10) + 48;

  char difference_minute_tens = ((difference_minutes / 10) % 10) + 48;
  char difference_minute_ones = (difference_minutes % 10) + 48;

  char difference_second_tens = ((difference_seconds / 10) % 10) + 48;
  char difference_second_ones = (difference_seconds % 10) + 48;

  diff_line[0] = difference_day_thousands;
  diff_line[1] = difference_day_hundreds;
  diff_line[2] = difference_day_tens;
  diff_line[3] = difference_day_ones;

  diff_line[4] = difference_hour_tens;
  diff_line[5] = difference_hour_ones;

  diff_line[6] = difference_minute_tens;
  diff_line[7] = difference_minute_ones;

  diff_line[8] = difference_second_tens;
  diff_line[9] = difference_second_ones;
}