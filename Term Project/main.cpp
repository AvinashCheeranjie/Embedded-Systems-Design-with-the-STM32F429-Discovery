#include "LCD_DISCO_F429ZI.h"
#include "mbed.h"
#include <string.h>

#define DOT_LENGTH 250
#define DASH_LENGTH DOT_LENGTH * 3

// MBed Objects
InterruptIn userbtn(BUTTON1);   // onboard user button
InterruptIn btn2(PD_7, PullUp); // external button
LCD_DISCO_F429ZI LCD;
static UnbufferedSerial pc(USBTX, USBRX); // USB serial port
DigitalOut led1(PC_14);                   // RGB LED for flashing Morse
DigitalOut buzzer(PC_15);                 // buzzer for Morse beeps
Ticker idleTimer;                         // for reverting to idle state

// Globals
volatile int index = 0;  // for Morse encoding algorithm
char received_text[100]; // buffer for storing input text
char morseSymbol[1000];  // buffer for storing encoded Morse
static const char space[] =
    "000";                    // standardised space between letters in Morse
volatile int SpaceDetect = 0; // space detection flag for encoding algorithm
size_t i = 0;
volatile bool playMorse = false;
volatile bool showText = false;
volatile bool newWord = false;
char display1[100]; // buffer for displaying morse on LCD
char display2[100];
volatile int state = 0; // FSM state
volatile int idleCount = 0;

// Function Prototypes
static const char *EncodeMorse(char character);
void UpdateLCD();
void UARTRxISR();
void TranslateISR();
void ToggleMode();
void IdleTick();

int main() {
  buzzer = 0;
  led1 = 1;
  // LCD Settings
  LCD.SetFont(&Font20);
  LCD.SetTextColor(LCD_COLOR_WHITE);
  LCD.SetBackColor(LCD_COLOR_BLACK);
  UpdateLCD();

  pc.set_blocking(false);
  pc.attach(&UARTRxISR);

  userbtn.fall(&TranslateISR);
  btn2.fall(&ToggleMode);
  idleTimer.attach(&IdleTick, 1s);
  __enable_irq();

  while (true) {
    if (playMorse) { // text-to-morse
      UpdateLCD();
      int spaceCount = 0;
      int wordCount = 0;
      bool newLine = false;

      for (int i = 0; i < strlen(morseSymbol); i++) {
        if (spaceCount == 3 && !newLine)
          strcat(display1, " ");
        if (spaceCount == 3 && newLine)
          strcat(display2, " ");

        if (spaceCount == 7) {
          wordCount++;
          newLine = true;
        }

        LCD.DisplayStringAt(0, 150, (uint8_t *)&display1, LEFT_MODE);
        LCD.DisplayStringAt(0, 180, (uint8_t *)&display2, LEFT_MODE);

        char c = morseSymbol[i];
        switch (c) {
        case '1': // dot
          led1 = 0;
          buzzer = 1;
          if (!newLine)
            strcat(display1, ".");
          if (newLine)
            strcat(display2, ".");
          spaceCount = 0;
          thread_sleep_for(DOT_LENGTH);
          break;
        case '0':
          led1 = 1; // turn off for dot length
          buzzer = 0;
          spaceCount++;
          thread_sleep_for(DOT_LENGTH);
          break;
        case '3': // dash
          led1 = 0;
          buzzer = 1;
          if (!newLine)
            strcat(display1, "-");
          if (newLine)
            strcat(display2, "-");
          spaceCount = 0;
          thread_sleep_for(DASH_LENGTH);
          break;
        default:
          break;
        }
      }

      LCD.DisplayStringAt(0, 150, (uint8_t *)&display1, LEFT_MODE);
      LCD.DisplayStringAt(0, 180, (uint8_t *)&display2, LEFT_MODE);

      led1 = 1;
      buzzer = 0;
      playMorse = false;
    }

    if (showText) { // morse-to-text
      UpdateLCD();
      showText = false;
    }
  } // inf while
}

void UpdateLCD() {
  switch (state) {
  case 0: // Idle State (Text-to-Morse)
    LCD.Clear(LCD_COLOR_BLACK);
    LCD.SetFont(&Font20);
    LCD.DisplayStringAt(0, 40, (uint8_t *)"Morse Code", CENTER_MODE);
    LCD.DisplayStringAt(0, 60, (uint8_t *)"Translator", CENTER_MODE);
    LCD.DisplayStringAt(0, 100, (uint8_t *)"Current Mode", CENTER_MODE);
    LCD.DisplayStringAt(0, 130, (uint8_t *)"Text-To-Morse", CENTER_MODE);
    break;

  case 1: // Playing Morse State
    memset(display1, 0, sizeof(display1));
    memset(display2, 0, sizeof(display2));
    LCD.Clear(LCD_COLOR_BLACK);
    if (strlen(received_text) == 0) { // userbtn pressed without sending text
      LCD.DisplayStringAt(0, 40, (uint8_t *)"No Input Text", CENTER_MODE);
      LCD.DisplayStringAt(0, 60, (uint8_t *)"To Translate", CENTER_MODE);
      idleCount = 43;
    } else {
      LCD.DisplayStringAt(0, 40, (uint8_t *)"Playing", CENTER_MODE);
      LCD.DisplayStringAt(0, 60, (uint8_t *)"Translation", CENTER_MODE);
      LCD.SetFont(&Font16);
    }
    break;

  case 2: // Idle State (Morse-to-Text)
    LCD.Clear(LCD_COLOR_BLACK);
    LCD.SetFont(&Font20);
    LCD.DisplayStringAt(0, 40, (uint8_t *)"Morse Code", CENTER_MODE);
    LCD.DisplayStringAt(0, 60, (uint8_t *)"Translator", CENTER_MODE);
    LCD.DisplayStringAt(0, 100, (uint8_t *)"Current Mode", CENTER_MODE);
    LCD.DisplayStringAt(0, 130, (uint8_t *)"Morse-To-Text", CENTER_MODE);
    break;

  case 3: // Translating Morse State
    memset(display1, 0, sizeof(display1));
    memset(display2, 0, sizeof(display2));
    LCD.Clear(LCD_COLOR_BLACK);
    if (strlen(received_text) == 0) { // userbtn pressed without sending text
      LCD.DisplayStringAt(0, 40, (uint8_t *)"No Translated", CENTER_MODE);
      LCD.DisplayStringAt(0, 60, (uint8_t *)"Text Received", CENTER_MODE);
      idleCount = 43;
    } else {
      LCD.DisplayStringAt(0, 40, (uint8_t *)"Morse", CENTER_MODE);
      LCD.DisplayStringAt(0, 60, (uint8_t *)"Translation", CENTER_MODE);
      LCD.DisplayStringAt(1, 150, (uint8_t *)&received_text, LEFT_MODE);
      LCD.DisplayStringAt(0, 180, (uint8_t *)&display2, LEFT_MODE);
    }
    break;
  }
}

void UARTRxISR() {
  if (newWord) { // clear buffers for new text
    memset(received_text, 0, sizeof(received_text));
    memset(morseSymbol, 0, sizeof(morseSymbol));
    newWord = false;
  }
  char c;
  if (pc.readable()) {
    pc.read(&c, 1);               // read character from serial
    if (c == '\r' || c == '\n') { // if [Enter] key is pressed
      received_text[index] = '\0';
      index = 0;
    } else if (index < sizeof(received_text) - 1) {
      received_text[index] = c;
      index++;
    }
  }
}

void TranslateISR() {
  idleCount = 0;
  switch (state) {
  case 0:
    // Translate Input Text to Encoded Morse. Adapted from:
    // https://os.mbed.com/users/jkhan/code/MorseCode//file/277b4be8e03c/main.cpp/
    for (int j = 0; j < index; j++) {
      char c = received_text[j];
      const char *code = EncodeMorse(c);

      if (j > 0) {
        if (c != 32) {                  // is not [Space]
          if (SpaceDetect == 0) {       // is not a letter after  [Space]
            strcat(morseSymbol, space); // add spacing before letter
            strcat(morseSymbol, code);
          } else {                     // is a letter after [Space]
            strcat(morseSymbol, code); // don't add spacing before letter
            SpaceDetect = 0;           // reset "space detected" flag
          }
        } else {                     // is [Space]
          strcat(morseSymbol, code); // don't add spacing
          SpaceDetect = 1;           // dont add spacing to the following letter
        }
      } else {                     // is the first letter in the code
        strcat(morseSymbol, code); // don't add spacing before first word
      }
    }
    index = 0;
    playMorse = true;
    newWord = true;
    state = 1;
    break;

  case 1:
    playMorse = true;
    break;

  case 2:
    showText = true;
    newWord = true;
    state = 3;
    break;

  case 3: // display translated text
    showText = true;
    break;
  }
}

void ToggleMode() {
  idleCount = 0;
  switch (state) {
  case 0:
  case 1:
    state = 2;
    UpdateLCD();
    break;

  case 2:
  case 3:
    state = 0;
    UpdateLCD();
    break;
  }
}

void IdleTick() {
  idleCount++;
  if (idleCount > 45) {
    if (state == 1) {
      state = 0;
      UpdateLCD();
    }

    if (state == 3) {
      state = 2;
      UpdateLCD();
    }
  }
}

// Source:
// https://os.mbed.com/users/jkhan/code/MorseCode//file/277b4be8e03c/EncodeMorse.cpp/
static const char *EncodeMorse(char character) {
  int ascii = character; // char to int

  switch (ascii) {
  case 8:
    return "101010101010101"; // [Backspace] (8 dots)
  case 32:
    return "0000000"; // [Space]
  case 34:
    return "10301010301"; // ["] (quotation mark)
  case 39:
    return "10303030301"; // ['] (apostrophe)
  case 40:
    return "301030301"; // [(]
  case 41:
    return "30103030103"; // [)]
  case 43:
    return "103010301"; // [+]
  case 44:
    return "30301010303"; // [,] (comma)
  case 45:
    return "30101010103"; // [-]
  case 46:
    return "10301030103"; // [.] (period)
  case 47:
    return "301010301"; // [/]
  case 48:
    return "303030303"; // 0
  case 49:
    return "103030303"; // 1
  case 50:
    return "101030303"; // 2
  case 51:
    return "101010303"; // 3
  case 52:
    return "101010103"; // 4
  case 53:
    return "101010101"; // 5
  case 54:
    return "301010101"; // 6
  case 55:
    return "303010101"; // 7
  case 56:
    return "303030101"; // 8
  case 57:
    return "303030301"; // 9
  case 58:
    return "30303010101"; // [:] (colon)
  case 61:
    return "301010103"; // [=]
  case 63:
    return "10103030101"; // [?]
  case 64:
    return "10303010301"; // [@]
  case 65:
  case 97:
    return "103"; // A,a
  case 66:
  case 98:
    return "3010101"; // B,b
  case 67:
  case 99:
    return "3010301"; // C,c
  case 68:
  case 100:
    return "30101"; // D,d
  case 69:
  case 101:
    return "1"; // E,e
  case 70:
  case 102:
    return "1010301"; // F,f
  case 71:
  case 103:
    return "30301"; // G,g
  case 72:
  case 104:
    return "1010101"; // H,h
  case 73:
  case 105:
    return "101"; // I,i
  case 74:
  case 106:
    return "1030303"; // J,j
  case 75:
  case 107:
    return "30103"; // K,k
  case 76:
  case 108:
    return "1030101"; // L,l
  case 77:
  case 109:
    return "303"; // M,m
  case 78:
  case 110:
    return "301"; // N,n
  case 79:
  case 111:
    return "30303"; // O,o
  case 80:
  case 112:
    return "1030301"; // P,p
  case 81:
  case 113:
    return "3030103"; // Q,q
  case 82:
  case 114:
    return "10301"; // R,r
  case 83:
  case 115:
    return "10101"; // S,s
  case 84:
  case 116:
    return "3"; // T,t
  case 85:
  case 117:
    return "10103"; // U,u
  case 86:
  case 118:
    return "1010103"; // V,v
  case 87:
  case 119:
    return "10303"; // W,w
  case 88:
  case 120:
    return "3010103"; // X,x (also use for multiplication sign)
  case 89:
  case 121:
    return "3010303"; // Y,y
  case 90:
  case 122:
    return "3030101"; // Z,z

  case 0:
    return "-1"; // null (for debugging)
  case 10:
    return "-1"; // newline (for debugging)

  default:
    return "";
  }
}