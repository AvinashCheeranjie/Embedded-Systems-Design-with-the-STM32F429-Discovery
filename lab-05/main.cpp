#include "DebouncedInterrupt.h"
#include "LCD_DISCO_F429ZI.h"
#include "mbed.h"
#include <chrono>

/*
Stepper motor has 48 steps and is bipolar.
Angular Resolution = 360 / 48 = 7.5 degrees per step

Student 1:
Name: Abdullah Abusalout
Student Number: 400533459
Last 2 digits = 59
Seconds per revolution: 59 seconds

Full Stepping:
59 / 48 = 1.229 seconds per step
Half Stepping:
59 / 96 = 0.6146 seconds per step

Student 2:
Name: Avinash Cheeranjie
Student Number: 400530770
Last 2 digits = 70 → 70 - 33 = 37
Seconds per revolution: 37 seconds

Full Stepping:
37 / 48 = 0.7708 seconds per step
Half Stepping:
37 / 96 = 0.3854 seconds per step
*/

LCD_DISCO_F429ZI LCD;

// Stepper motor pins
DigitalOut s_red(PF_15);
DigitalOut s_grey(PG_0);
DigitalOut s_yellow(PG_1);
DigitalOut s_black(PE_7);

// Buttons
DebouncedInterrupt direction(PA_8);
DebouncedInterrupt cycle(PA_9);
InterruptIn user_button(BUTTON1);
DebouncedInterrupt increase(PA_14);
DebouncedInterrupt decrease(PA_12);

Ticker stepperTick;
int speed = 1229; // Student 1 full-step (1229 ms per step)
int num_step = 4;
int num_half_step = 8;
int step = 0;
bool CW = true; // clockwise
bool start = false;
bool student1 = true;
bool full = true;

// Full-step sequence functions (4 steps)
void step1() {
  s_red = 1;
  s_grey = 0;
  s_yellow = 1;
  s_black = 0;
}
void step2() {
  s_red = 1;
  s_grey = 0;
  s_yellow = 0;
  s_black = 1;
}
void step3() {
  s_red = 0;
  s_grey = 1;
  s_yellow = 0;
  s_black = 1;
}
void step4() {
  s_red = 0;
  s_grey = 1;
  s_yellow = 1;
  s_black = 0;
}

void (*state_table[])() = {step1, step2, step3, step4};

// Half-step sequence functions (8 steps)
void halfStep1() {
  s_red = 1;
  s_grey = 0;
  s_yellow = 0;
  s_black = 0;
}
void halfStep2() {
  s_red = 1;
  s_grey = 0;
  s_yellow = 1;
  s_black = 0;
}
void halfStep3() {
  s_red = 0;
  s_grey = 0;
  s_yellow = 1;
  s_black = 0;
}
void halfStep4() {
  s_red = 0;
  s_grey = 1;
  s_yellow = 1;
  s_black = 0;
}
void halfStep5() {
  s_red = 0;
  s_grey = 1;
  s_yellow = 0;
  s_black = 0;
}
void halfStep6() {
  s_red = 0;
  s_grey = 1;
  s_yellow = 0;
  s_black = 1;
}
void halfStep7() {
  s_red = 0;
  s_grey = 0;
  s_yellow = 0;
  s_black = 1;
}
void halfStep8() {
  s_red = 1;
  s_grey = 0;
  s_yellow = 0;
  s_black = 1;
}

void (*half_state_table[])() = {halfStep1, halfStep8, halfStep7, halfStep6,
                                halfStep5, halfStep4, halfStep3, halfStep2};

// Stepper motor ISR
void stepperISR() {
  if (start) {
    if (full) {
      if (CW) {
        step = (step + 1) % num_step;
        state_table[step]();
      } else {
        step = (step - 1 + num_step) % num_step;
        state_table[step]();
      }
    } else {
      if (CW) {
        step = (step + 1) % num_half_step;
        half_state_table[step]();
      } else {
        step = (step - 1 + num_half_step) % num_half_step;
        half_state_table[step]();
      }
    }
  }
}

// Toggle direction of rotation
void changeDirection() {
  CW = !CW;
  // Immediately update motor output for a faster response:
  stepperISR();
  LCD.Clear(LCD_COLOR_WHITE);
}

// Toggle between half and full step
void cycleHalf() {
  full = !full;
  LCD.Clear(LCD_COLOR_WHITE);
  // Update speed based on current student and step mode
  if (student1) {
    speed = full ? 1229 : 614; // 59 sec/rev: 1229ms for full, 614ms for half
  } else {
    speed = full ? 770 : 385; // 37 sec/rev: 770ms for full, 385ms for half
  }
  stepperTick.detach();
  stepperTick.attach(&stepperISR, std::chrono::milliseconds(speed));
}

// Toggle between student 1 & 2
void userButtonISR() {
  if (!start) {
    start = true;
  } else {
    CW = true;            // reset direction to clockwise
    student1 = !student1; 
    speed = student1 ? (full ? 1229 : 614) : (full ? 770 : 385);
  }
  stepperTick.detach();
  stepperTick.attach(&stepperISR, std::chrono::milliseconds(speed));
  LCD.Clear(LCD_COLOR_WHITE);
}

void decreaseSpeed() {
  speed = std::max(50, speed - 50); // prevent speed from going too low
  LCD.Clear(LCD_COLOR_WHITE);
  stepperTick.detach();
  stepperTick.attach(&stepperISR, std::chrono::milliseconds(speed));
}

void increaseSpeed() {
  speed += 50;
  LCD.Clear(LCD_COLOR_WHITE);
  stepperTick.detach();
  stepperTick.attach(&stepperISR, std::chrono::milliseconds(speed));
}

int main() {
  __enable_irq();
  // LCD Settings
  LCD.Clear(LCD_COLOR_WHITE);
  LCD.SetFont(&Font16);
  LCD.SetTextColor(LCD_COLOR_BLACK);

  // Button ISRs
  user_button.fall(&userButtonISR);
  direction.attach(&changeDirection, IRQ_FALL, 100, false);
  cycle.attach(&cycleHalf, IRQ_FALL, 100, false);
  increase.attach(&increaseSpeed, IRQ_FALL, 100, false);
  decrease.attach(&decreaseSpeed, IRQ_FALL, 100, false);

  stepperTick.attach(&stepperISR, std::chrono::milliseconds(speed));

  while (true) {
    if (start) {
      LCD.Clear(LCD_COLOR_WHITE);
      if (student1) {
        LCD.DisplayStringAt(0, 40, (uint8_t *)"Number: 400533459", CENTER_MODE);
        LCD.DisplayStringAt(0, 60, (uint8_t *)"Secs/Rev: 59s", CENTER_MODE);
        LCD.DisplayStringAt(0, 80, (uint8_t *)"Abdullah Abusalout",
                            CENTER_MODE);
      } else {
        LCD.DisplayStringAt(0, 40, (uint8_t *)"Number: 400530770", CENTER_MODE);
        LCD.DisplayStringAt(0, 60, (uint8_t *)"Secs/Rev: 37s", CENTER_MODE);
        LCD.DisplayStringAt(0, 80, (uint8_t *)"Avinash Cheeranjie",
                            CENTER_MODE);
      }
      if (full) {
        LCD.DisplayStringAt(0, 100, (uint8_t *)"Full Step", CENTER_MODE);
      } else {
        LCD.DisplayStringAt(0, 100, (uint8_t *)"Half Step", CENTER_MODE);
      }
    }
    ThisThread::sleep_for(chrono::milliseconds(250));
  }
}
