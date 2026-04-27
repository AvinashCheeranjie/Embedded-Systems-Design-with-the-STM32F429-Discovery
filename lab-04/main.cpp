#include "LCD_DISCO_F429ZI.h"
#include "TS_DISCO_F429ZI.h"
#include "mbed.h"

// Screen constants
#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 320
#define BUTTON_HEIGHT 50
#define BUTTON_WIDTH 100
#define BUTTON_Y (SCREEN_HEIGHT - BUTTON_HEIGHT - 10)

// MBed Objects
LCD_DISCO_F429ZI LCD;
TS_DISCO_F429ZI TS;
AnalogIn tempSensor(PA_0);
PwmOut fan(PD_14);
Ticker periodicTicker;
Ticker fanTicker;

// FSM states
typedef enum { IDLE_STATE, FAN_ON, INCREMENT_THRESH, DECREMENT_THRESH } State;

// Globals
volatile float currentTemp;
volatile float threshTemp;
volatile bool updateDisplay = false;
volatile float duty = 0.0; // pwm duty cycle
State state;

// Function prototypes
float ReadTemperature();
void DrawButtons();
void UpdateLCD();
void PeriodicUpdate();
void StartMotor();
void InitialiseFSM();
void HandleIdleState();
void HandleFanOn();
void IncrementThreshold();
void DecrementThreshold();
void PeriodicUpdate() { updateDisplay = true; }

void StartMotor() {
  if (duty == 1)
    return;
  duty += 0.1;
  fan.write(duty);
}

int main() {
  // LCD Settings
  LCD.Clear(LCD_COLOR_WHITE);
  LCD.SetFont(&Font20);
  LCD.SetTextColor(LCD_COLOR_BLACK);

  fan.period_ms(100);
  InitialiseFSM();
  periodicTicker.attach(&PeriodicUpdate, 50ms);

  while (true) {
    switch (state) {
    case IDLE_STATE:
      HandleIdleState();
      break;

    case FAN_ON:
      HandleFanOn();
      break;

    case INCREMENT_THRESH:
      IncrementThreshold();
      break;

    case DECREMENT_THRESH:
      DecrementThreshold();
      break;
    }

    ThisThread::sleep_for(20ms);
  }
}

void InitialiseFSM() {
  currentTemp = ReadTemperature();
  threshTemp = ceil(currentTemp) + 1.0;
  DrawButtons();
  state = IDLE_STATE;
}

void HandleIdleState() {
  currentTemp = ReadTemperature();
  if (currentTemp > threshTemp) { // transition to fan on state
    duty = 0.0;
    fanTicker.attach(&StartMotor, 1000ms); // gradually ramp up fan speed
    state = FAN_ON;
    return;
  }

  // Check for button presses
  TS_StateTypeDef ts_state;
  TS.GetState(&ts_state);

  if (ts_state.TouchDetected && ts_state.Y > 250) {
    if (ts_state.X < SCREEN_WIDTH / 2) { // increment button
      state = INCREMENT_THRESH;
      return;
    }

    if (ts_state.X >= SCREEN_WIDTH / 2) { // decrement button
      state = DECREMENT_THRESH;
      return;
    }
  }

  if (updateDisplay) {
    UpdateLCD();
    updateDisplay = false;
  }
}

void HandleFanOn() {
  currentTemp = ReadTemperature();
  if (currentTemp <= threshTemp) {
    duty = 0.0;
    fan.write(0.0f);
    fanTicker.detach();
    state = IDLE_STATE;
    return;
  }

  // Check for button presses
  TS_StateTypeDef ts_state;
  TS.GetState(&ts_state);

  if (ts_state.TouchDetected && ts_state.Y > 250) {
    if (ts_state.X < SCREEN_WIDTH / 2) { // increment button
      state = INCREMENT_THRESH;
      return;
    }

    if (ts_state.X >= SCREEN_WIDTH / 2) { // decrement button
      state = DECREMENT_THRESH;
      return;
    }
  }

  if (updateDisplay) {
    UpdateLCD();
    updateDisplay = false;
  }
}

void IncrementThreshold() {
  // Highlight "+" button
  LCD.SetTextColor(LCD_COLOR_GREEN);
  LCD.SetBackColor(LCD_COLOR_GREEN);
  LCD.FillCircle((SCREEN_WIDTH / 2) - BUTTON_WIDTH - 5 + (BUTTON_WIDTH / 2),
                 BUTTON_Y + (BUTTON_HEIGHT / 2) - 5, 30);
  LCD.SetTextColor(LCD_COLOR_BLUE);
  LCD.DisplayChar((SCREEN_WIDTH / 2) - BUTTON_WIDTH - 10 + (BUTTON_WIDTH / 2),
                  BUTTON_Y + (BUTTON_HEIGHT / 2) - 12, '+');

  threshTemp += 0.5;
  ThisThread::sleep_for(200ms);
  DrawButtons();

  // return to previous state
  if (currentTemp > threshTemp) {
    state = FAN_ON;
  } else {
    state = IDLE_STATE;
  }
}

void DecrementThreshold() {
  // Highlight "-" button
  LCD.SetTextColor(LCD_COLOR_GREEN);
  LCD.SetBackColor(LCD_COLOR_GREEN);
  LCD.FillCircle((SCREEN_WIDTH / 2) + 15 + (BUTTON_WIDTH / 2),
                 BUTTON_Y + (BUTTON_HEIGHT / 2) - 5, 30);
  LCD.SetTextColor(LCD_COLOR_BLUE);
  LCD.DisplayChar((SCREEN_WIDTH / 2) + 10 + (BUTTON_WIDTH / 2),
                  BUTTON_Y + (BUTTON_HEIGHT / 2) - 12, '-');

  threshTemp -= 0.5;
  ThisThread::sleep_for(200ms);
  DrawButtons();

  // return to previous state
  if (currentTemp > threshTemp) {
    state = FAN_ON;
  } else {
    state = IDLE_STATE;
  }
}

// Read temperature from LM35 sensor
float ReadTemperature() {
  float voltage = tempSensor.read();
  return voltage * 100.0; // convert voltage to C (10 mV/°C)
}

//  Draw increment and decrement buttons
void DrawButtons() {
  // Draw "+" button with blue background
  LCD.SetTextColor(LCD_COLOR_BLUE);
  LCD.SetBackColor(LCD_COLOR_BLUE);
  LCD.FillCircle((SCREEN_WIDTH / 2) - BUTTON_WIDTH - 5 + (BUTTON_WIDTH / 2),
                 BUTTON_Y + (BUTTON_HEIGHT / 2) - 5, 30);
  LCD.SetTextColor(LCD_COLOR_BLACK);
  LCD.DisplayChar((SCREEN_WIDTH / 2) - BUTTON_WIDTH - 10 + (BUTTON_WIDTH / 2),
                  BUTTON_Y + (BUTTON_HEIGHT / 2) - 12, '+');

  // Draw "-" button with red background
  LCD.SetTextColor(LCD_COLOR_RED);
  LCD.SetBackColor(LCD_COLOR_RED);
  LCD.FillCircle((SCREEN_WIDTH / 2) + 15 + (BUTTON_WIDTH / 2),
                 BUTTON_Y + (BUTTON_HEIGHT / 2) - 5, 30);
  LCD.SetTextColor(LCD_COLOR_BLACK);
  LCD.DisplayChar((SCREEN_WIDTH / 2) + 10 + (BUTTON_WIDTH / 2),
                  BUTTON_Y + (BUTTON_HEIGHT / 2) - 12, '-');
}

void UpdateLCD() {
  LCD.SetBackColor(LCD_COLOR_WHITE);

  // Display sensor and threshold temperatures
  LCD.DisplayStringAt(0, 40, (uint8_t *)"Smart Fan", CENTER_MODE);
  char display[20];
  sprintf((char *)display, "Sensor: %.1f C", currentTemp);
  LCD.DisplayStringAt(0, 80, (uint8_t *)&display, CENTER_MODE);
  sprintf((char *)display, "Thresh: %.1f C", threshTemp);
  LCD.DisplayStringAt(0, 100, (uint8_t *)&display, CENTER_MODE);
}