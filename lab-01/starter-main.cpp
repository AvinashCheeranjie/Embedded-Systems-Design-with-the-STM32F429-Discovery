#include "mbed.h"

InterruptIn userButton(BUTTON1);

DigitalOut led3(PG_13);
DigitalOut led4(PG_14);

int state = 0; // default state on startup

void SetState() {
  if (state != 1) {
    state = 1;
  } else {
    state = 2;
  }
}

int main() {
  userButton.fall(&SetState);
  __enable_irq();

  while (true) {
    // start with LEDs off to switch between phases cleanly  
    led3 = 0;
    led4 = 0;
    if (state == 0) {
      led4 = 1;
      thread_sleep_for(1000);
      led4 = 0;
      thread_sleep_for(1000);
    } else if (state == 1) {
      led3 = 0;
      led4 = 1;
      thread_sleep_for(1000);
      led3 = 1;
      led4 = 0;
      thread_sleep_for(1000);
    } else {
      led4 = 1;
      thread_sleep_for(1000);
      led4 = 0;
      thread_sleep_for(1000);
      led3 = 1;
      thread_sleep_for(1000);
      led3 = 0;
      thread_sleep_for(1000);
    }
  }
}

