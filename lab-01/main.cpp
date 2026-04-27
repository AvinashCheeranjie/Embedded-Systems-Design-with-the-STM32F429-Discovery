#include "mbed.h"

// Initialize user button and LEDs
InterruptIn userButton(BUTTON1);
DigitalOut led3(PG_13); // green
DigitalOut led4(PG_14); // red

volatile int state = 0; // default state on startup
Ticker ticker;

// Function to change the state when the button is pressed
void SetState() { state = (state != 1) ? 1 : 2; }

// Function to toggle LEDs based on the state
void ToggleLEDs() {
  static int clock = 0; // keeps track of the current clock cycle

  led3 = 0;
  led4 = 0;

  switch (state) {
  // toggle led4 only
  case 0:
    led4 = (clock % 2 == 0) ? !led4 : led4;
    break;
  // alternate between LEDs
  case 1:
    led4 = (clock % 2 == 0) ? !led4 : led3 = !led3;
    break;
  // alternate blinking the LEDs
  case 2:
    led4 = (clock % 4 == 0) ? !led4 : led4;
    led3 = (clock % 4 == 2) ? !led3 : led3;
    break;
  }

  clock++;
}

int main() {
  userButton.fall(&SetState);
  ticker.attach(&ToggleLEDs, 1s);
  __enable_irq();

  while (true) {
  }
}