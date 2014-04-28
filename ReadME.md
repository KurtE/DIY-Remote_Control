Warning
=======

This is a Work In Progress and also a place holder for several projects that a few of us have done with building our own Remote control that uses XBees to communicate. These projects are discussed up on the Lynxmotion forums, including the threads:
http://www.lynxmotion.net/viewtopic.php?f=21&t=9220
http://www.lynxmotion.net/viewtopic.php?f=21&t=5447
http://www.lynxmotion.net/viewtopic.php?f=21&t=7707

As well as several others. 

Again this is a Work In Progress!  There are no warranties or Guarantees of any type that this code is usable for anything. But I hope it is.

Communication Protocol
===
My DIY remote controls use XBee Series 1 to communicate.  I use the XBees in packet mode. I am currently in the process of simplifying the communication protocol between the remote control and robots.  Before the code had it where the Remote detects that it has some new data (joystick moved, button pressed...) and sends a packet to the robot, who when ready sends a packet saying, I want your current data at which point the Controller responds with the data.  This was working well but was a lot of overhead and I was probably only getting 5-10 data packets per second max.  Now the remote simply sends the data at a predefined rate, currently set to 30 packets per second.  

Will fill in more later...

Directories
===

Arduino
---
Contains the Arduino versions of the software. Again WIP there will be a few directories.  One for my current transmitter that is currently using a Teensy 3.1 processor board, plus I have an 4d systems OLED display, 2 4 way joysticks, 3 sliders and a 16 key keypad. 

Basic Atom Pro
---
Code for the Original XBee remote control that uses a Basic Atom Pro 28.  The transmitter version here is based off of the version that Kåre (Zenta) added two more analog inputs to.  

Older Protocol
---
Copies of stuff that used the earlier version of the protocol with the extra handshaking. 

WIP
===
Again this is a WIP