Notes on the sub-directories:

DIY_Receiver_test: 
Test app, that displays packet information on TFT display
This is currently running on a Teensy 3.1 board on my Teensy breakout board with Arduino headers:
https://github.com/KurtE/Teensy3.1-Breakout-Boards/tree/master/Teensy%20Shield%20with%20Arduino%20Headers
I am using an XBee shield from Seeedstudio as well as a 1.8" tft display shield from Adafruit.

DIY_Remote_Tenensy:
This is my updated Arduino DIY remote control software.  I am currently using my other Teensy breakout board
that has XBee connectors: 
https://github.com/KurtE/Teensy3.1-Breakout-Boards/tree/master/Teensy%20Shield%20with%20XBee

SQ3_DIY_Teensy: 
This is using the Arduino Phoenix Parts project Quad support branch: https://github.com/KurtE/Arduino_Phoenix_Parts/tree/Quad-Support
It is for a Lynxmotion SQ3 quad robot, that is only using the Teensy 3.1 XBee breakout board.  The Teensy is driving the 
servos in this version. 