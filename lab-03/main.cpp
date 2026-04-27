#include "DebouncedInterrupt.h"
#include "LCD_DISCO_F429ZI.h"
#include "mbed.h"
#include <time.h>

#define DATA_PIN PC_9
#define CLOCK_PIN PA_8
#define MEMORY_ADDR 0xA0

LCD_DISCO_F429ZI LCD;
I2C EEPROM(DATA_PIN, CLOCK_PIN);
InterruptIn button(BUTTON1);
DebouncedInterrupt showMemory(PA_6);
DebouncedInterrupt switchMode(PC_2);
DebouncedInterrupt adjustTime(PC_3);
Ticker tick; // for calling FSM every second

// Globals
static unsigned int mem_address = 0;
char timeString[20];
struct tm timeStruct; // for configuring RTC
time_t currentTime;
volatile int idleCounter = 0; // for automatically reverting to idle state
volatile int state = 0;
volatile bool initialInput = true;
volatile bool buttonPressed = false;
volatile bool showMemoryPress = false;
volatile bool hourSelect = false;
char firstEntry[10];           // for reading first time value
char secondEntry[10];          // for reading second time value
char modifyBuffer[20];         // for displaying setting time in edit mode
struct tm *timeInfo = nullptr; // for updating current time in edit mode

// Helper function prototypes
void WriteMemory(int addr, unsigned int mem_addr, char *data, int size);
void ReadMemory(int addr, unsigned int mem_addr, char *data, int size);

// Finite State Machine
void FSM() {
  currentTime++;
  switch (state) {
  case 0: // Idle State
    LCD.Clear(LCD_COLOR_BLACK);
    strftime(timeString, 20, "%H:%M:%S", localtime(&currentTime));
    LCD.SetBackColor(LCD_COLOR_BLACK);
    LCD.DisplayStringAt(0, 80, (uint8_t *)&timeString, CENTER_MODE);
    LCD.DisplayStringAt(0, 40, (uint8_t *)&"HH:MM:SS", CENTER_MODE);
    break;
  case 1: // Edit Time State
    timeInfo = localtime(&currentTime);
    strftime(modifyBuffer, 20, "%H:%M:%S", timeInfo);
    if (hourSelect) { // edit hour
      sprintf(modifyBuffer, "[%02d]:%02d:%02d", timeInfo->tm_hour,
              timeInfo->tm_min, timeInfo->tm_sec);
    } else { // edit minute
      sprintf(modifyBuffer, "%02d:[%02d]:%02d", timeInfo->tm_hour,
              timeInfo->tm_min, timeInfo->tm_sec);
    }
    LCD.Clear(LCD_COLOR_BLACK);
    LCD.SetBackColor(LCD_COLOR_BLACK);
    LCD.DisplayStringAt(0, 40, (uint8_t *)"Set Time", CENTER_MODE);
    LCD.DisplayStringAt(0, 80, (uint8_t *)modifyBuffer, CENTER_MODE);
    break;
  case 2: // Display Memory State
    LCD.Clear(LCD_COLOR_BLACK);
    LCD.DisplayStringAt(0, 45, (uint8_t *)&"First Time", CENTER_MODE);
    LCD.DisplayStringAt(0, 70, (uint8_t *)&firstEntry, CENTER_MODE);
    LCD.DisplayStringAt(0, 105, (uint8_t *)&"Second Time", CENTER_MODE);
    LCD.DisplayStringAt(0, 130, (uint8_t *)&secondEntry, CENTER_MODE);
    break;
  }
  idleCounter++;
  if (idleCounter >= 10) { // revert to idle state after 10 secs
    state = 0;
    LCD.Clear(LCD_COLOR_BLACK);
    idleCounter = 0;
  }
}

// Trigger time write to EEPROM
void writeTime() {
  idleCounter = 0;
  state = 0;
  buttonPressed = true;
  LCD.Clear(LCD_COLOR_BLACK);
}

// Trigger read from EEPROM
void toggleMemoryDisplay() {
  idleCounter = 0;
  switch (state) {
  case 2: // switch back to state 0
    state = 0;
    LCD.Clear(LCD_COLOR_BLACK);
    break;
  default: // enter display memory state from any state
    showMemoryPress = true;
    break;
  }
}

// Toggle between editting hours and minutes
void editTime() {
  idleCounter = 0;
  state = 1;
  hourSelect = !hourSelect;
  LCD.Clear(LCD_COLOR_BLACK);
}

// Increment minute or hour values
void updateClock() {
  idleCounter = 0;
  struct tm *timeInfo = nullptr;
  switch (state) {
  case 1: // Increment Time State
    timeInfo = localtime(&currentTime);
    if (hourSelect) {
      timeInfo->tm_hour = (timeInfo->tm_hour + 1) % 24;
    } else {
      timeInfo->tm_min = (timeInfo->tm_min + 1) % 60;
    }
    currentTime = mktime(timeInfo);
    LCD.Clear(LCD_COLOR_BLACK);
    break;
  default: // enter edit time mode from any state
    state = 1;
    LCD.Clear(LCD_COLOR_BLACK);
    break;
  }
}

int main() {
  button.fall(&writeTime);
  showMemory.attach(&toggleMemoryDisplay, IRQ_FALL, 100, false);
  switchMode.attach(&editTime, IRQ_FALL, 100, false);
  adjustTime.attach(&updateClock, IRQ_FALL, 100, false);

  tick.attach(&FSM, 1s);
  __enable_irq();

  // Set RTC to February 28, 2025 at 00:00:00 (HH:MM:SS)
  timeStruct.tm_year = 125;
  timeStruct.tm_mon = 1;
  timeStruct.tm_mday = 27;
  timeStruct.tm_hour = 0;
  timeStruct.tm_min = 0;
  timeStruct.tm_sec = 0;
  set_time(mktime(&timeStruct));

  // LCD Settings
  LCD.SetFont(&Font20);
  LCD.SetTextColor(LCD_COLOR_WHITE);
  LCD.Clear(LCD_COLOR_BLACK);

  while (1) {
    if (buttonPressed) {  // Write Time State
      if (initialInput) { // first write
        WriteMemory(MEMORY_ADDR, mem_address, timeString, 8);
        initialInput = false;
      } else { // second write
        WriteMemory(MEMORY_ADDR, mem_address + 8, timeString, 8);
        initialInput = true;
      }
      buttonPressed = false;
    }

    if (showMemoryPress) {
      ReadMemory(MEMORY_ADDR, mem_address, firstEntry, 8);
      ReadMemory(MEMORY_ADDR, mem_address + 8, secondEntry, 8);
      state = 2;
      showMemoryPress = false;
    }
  } // inf while
}

void WriteMemory(int addr, unsigned int mem_addr, char *data, int size) {
  char buffer[size + 2];
  buffer[0] = (unsigned char)(mem_addr >> 8);
  buffer[1] = (unsigned char)(mem_addr & 0xFF);
  for (int i = 0; i < size; i++) {
    buffer[i + 2] = data[i];
  }
  EEPROM.write(addr, buffer, size + 2, false);
  thread_sleep_for(6);
}

void ReadMemory(int addr, unsigned int mem_addr, char *data, int size) {
  char buffer[2];
  buffer[0] = (unsigned char)(mem_addr >> 8);
  buffer[1] = (unsigned char)(mem_addr & 0xFF);
  EEPROM.write(addr, buffer, 2, false);
  thread_sleep_for(6);
  EEPROM.read(addr, data, size);
  thread_sleep_for(6);
}