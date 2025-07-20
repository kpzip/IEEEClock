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
const volatile DateTime compile_time(__DATE__, __TIME__);
static volatile DateTime now;
static volatile DateTime travel_time = compile_time;
static volatile TimeSpan difference;

static volatile char top_line[TOP_LEN + 1] = { ' ' };
static volatile char bottom_line[BOTTOM_LEN + 1] = { ' ' };
static volatile char diff_line[DIFF_LEN + 1] = { ' ' };

// State
static volatile bool is_setting_time = false;
static volatile bool is_12_hr = false;
// 0 is top row, 1 is middle row, 2 is difference row
static volatile uint8_t cursor_row = 0;
// 0 is rightmost side
static volatile uint8_t cursor_column = 0;

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


}