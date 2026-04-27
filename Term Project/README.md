# Project Overview
The cybernetic system is a two-way Morse Code translator. It can switch between modes by pressing a connected external button.

## Text-to-Morse Mode
The microcontroller receives text input from the connected laptop via UART. The program then encodes the text to Morse code and plays it by blinking an RGB LED, buzzing an active buzzer, and displaying the Morse code on the onboard LCD upon pressing the onboard user button. 

## Morse-to-Text Mode
An external python script running on the connected laptop uses computer vision to interpret specific hand signals as Morse code and translates it to English. The microcontroller receives the translated text via UART and displys it on the onboard LCD upon pressing the onboard user button. 