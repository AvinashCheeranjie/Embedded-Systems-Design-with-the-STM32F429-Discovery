# Code adapted from https://github.com/HassanNiaz/PoseVerter
# Original authors: Hassan Niaz, Anas Ramzan
# This script is an adaptation for detecting Morse code from hand gestures using the HandTrackingModule.

import cv2
from HandTrackingModule import HandDetector
import serial

# Morse code lookup dictionary
morse_dict = {
    '.-': 'A', '-...': 'B', '-.-.': 'C', '-..': 'D', '.': 'E', '..-.': 'F',
    '--.': 'G', '....': 'H', '..': 'I', '.---': 'J', '-.-': 'K', '.-..': 'L',
    '--': 'M', '-.': 'N', '---': 'O', '.--.': 'P', '--.-': 'Q', '.-.': 'R',
    '...': 'S', '-': 'T', '..-': 'U', '...-': 'V', '.--': 'W', '-..-': 'X',
    '-.--': 'Y', '--..': 'Z', '-----': '0', '.----': '1', '..---': '2',
    '...--': '3', '....-': '4', '.....': '5', '-....': '6', '--...': '7',
    '---..': '8', '----.': '9'
}

# Initialize serial communication
try:
    ser = serial.Serial('COM3', baudrate=9600)
except serial.SerialException as e:
    print(f"Error initializing serial communication: {e}")
    ser = None 

# Initialize camera
cap = cv2.VideoCapture(0)
detector = HandDetector(detectionCon=0.5, maxHands=1)

# Variables to track states
checkThumb = False
checkX = False
checkY = False
words = ''
full_text = ''  # to store the entire decoded Morse code message

# Function to translate Morse code to English
def morse_to_text(morse_code):
    words = morse_code.strip().split('   ')  # split into words (3 spaces between words)
    decoded_message = ''
    for word in words:
        for symbol in word.split():  # split the word into symbols (separated by a single space)
            decoded_message += morse_dict.get(symbol, '')  # decode the symbol
        decoded_message += ' '  # space between words
    return decoded_message.strip()

# Main function to capture image and process hand gestures
def select_img():
    global checkThumb, checkX, checkY, words, cap, detector, full_text

    success, img = cap.read()
    if not success:
        cap = cv2.VideoCapture(0)
        select_img()

    img = cv2.flip(img, 1)  # Flip the image horizontally
    hands, img = detector.findHands(img, flipType=False)
    img = cv2.resize(img, (640, 480))  # Resize to fit the window

    if hands:
        hand1 = hands[0]
        lmList1 = hand1["lmList"]  # List of 21 Landmark points

        # Extract landmarks for relevant fingers
        Index_tipY = lmList1[8][1]  # Y-coordinate of the tip of index finger
        Index_dipY = lmList1[7][1]  # Y-coordinate of the dip of index finger
        
        Middle_tipY = lmList1[12][1]  # Y-coordinate of the tip of middle finger
        Middle_dipY = lmList1[11][1]  # Y-coordinate of the dip of middle finger
        
        Thumb_tipX = lmList1[4][0]  # X-coordinate of the tip of the thumb
        Thumb_dipX = lmList1[3][0]   # X-coordinate of the thumb dip (base)

        # Detect if the index finger is down (for dash)
        if (Index_tipY < Index_dipY):
            checkY = True
        if Index_tipY > Index_dipY and checkY:
            words += '-'
            checkY = False

        # Detect if the middle finger is down (for dot)
        if (Middle_tipY < Middle_dipY):
            checkX = True
        if Middle_tipY > Middle_dipY and checkX:
            words += '.'
            checkX = False

        # Detect thumb down for confirmation and space
        if(Thumb_tipX < Thumb_dipX):
            checkThumb = True
        if (Thumb_tipX > Thumb_dipX and checkThumb):
            # Confirm the current Morse sequence and append it to the full_text
            decoded_text = morse_to_text(words)  # Decode Morse code to text
            
            # print(f"Confirmed Morse Code: {words}")
            # print(f"Decoded Text: {decoded_text}")
            full_text += decoded_text  
            if (words == ''): # If the sequence is empty, add a space
                full_text += ' '
            words = ''  # Reset the sequence after confirmation
            checkThumb = False
            print(f"Decoded Text: {full_text}")

    # Display the camera feed with the detected hand landmarks
    cv2.imshow("Hand Tracking for Morse Code", img)

    # Print the full decoded text (real-time feedback)
    # print(f"Decoded Text: {full_text}")

    # print(f"Decoded Text: {decoded_text}")
    
    # Call the function again to capture the next frame
    if cv2.waitKey(1) & 0xFF == ord('q'):  # Press 'q' to quit
        cap.release()
        cv2.destroyAllWindows()
        return

    select_img()

def main():
    select_img()
    ser.write(full_text.encode())   

if __name__ == "__main__":
    main()
