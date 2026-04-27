#include <cstdint>
#include "LCD_DISCO_F429ZI.h"
#include "mbed.h"
#include "stm32f429i_discovery_lcd.h"

#define INF 100000000  // for initial best time

// Initialise mbed objects
LCD_DISCO_F429ZI LCD;
DigitalOut led3(PG_13);  // green
InterruptIn externalButton(PA_6, PullUp);
InterruptIn userButton(BUTTON1);  // on-board button
Ticker ticker1;
Timeout timeout;
Timer timer;

// Globals
volatile int state = 0;  // current state of FSM
int bestTime;            // fastest reaction time
uint32_t elapsedMs;      // elapsed time from timer
uint8_t text[40];        // for LCD text display

void ToggleLED() { led3 = !led3; }

// turn on LED after a random time and start timer
void ReactionDelay() {
  led3 = 1;
  timer.start();
}

// state 0
void initialiseFSM() {
  LCD.Clear(LCD_COLOR_WHITE);
  ticker1.attach(&ToggleLED, 50ms);  // flash at 10 Hz
  LCD.DisplayStringAt(0, 40, (uint8_t *)"Press Button", CENTER_MODE);
  LCD.DisplayStringAt(0, 60, (uint8_t *)"to Start", CENTER_MODE);
  bestTime = INF;
  elapsedMs = 0;
  state = 1;
}

void FSM() {
  // generate random number for reaction test
  int rng = (rand() % 5 + 1);  // random number (1,5)
  chrono::seconds random_duration(rng);

  switch (state) {
    // start reaction test
    case 1:
      LCD.Clear(LCD_COLOR_WHITE);
      ticker1.detach();
      led3 = 0;
      timeout.attach(&ReactionDelay, random_duration.count());
      state = 2;
      break;
    // compute and display reaction time
    case 2:
      timer.stop();
      elapsedMs =
          chrono::duration_cast<chrono::milliseconds>(timer.elapsed_time())
              .count();

      // check if button pressed at t = 0 (before led toggles)
      if (elapsedMs == 0) {
        timer.stop();
        timer.reset();
        initialiseFSM();
        break;
      }

      bestTime = (elapsedMs < bestTime) ? elapsedMs : bestTime;

      // display latest and best reaction times
      LCD.DisplayStringAt(0, 40, (uint8_t *)"Reaction Times", CENTER_MODE);
      sprintf((char *)text, "Latest: %d ms", elapsedMs);
      LCD.DisplayStringAt(0, 100, (uint8_t *)&text, CENTER_MODE);
      sprintf((char *)text, "Best: %d ms", bestTime);
      LCD.DisplayStringAt(0, 130, (uint8_t *)&text, CENTER_MODE);
      state = 1;
      timer.reset();
      break;
  }
}

// reset FSM at any time/state
void ResetISR() {
  timer.stop();
  timer.reset();
  initialiseFSM();
}

int main() {
  srand(static_cast<unsigned>(std::time(nullptr)));  // seed for rand()
  LCD.SetFont(&Font20);
  LCD.SetTextColor(LCD_COLOR_DARKBLUE);
  initialiseFSM();
  userButton.rise(&FSM);
  externalButton.rise(&ResetISR);
  __enable_irq();

  while (1) {
  }
}
