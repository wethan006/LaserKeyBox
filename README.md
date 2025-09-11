# LaserKeyBox
Contains all the information for my laser key box. Its a simple lock box that holds a key. Made using what I had around: a Raspberry pi 1, Arduino Uno R3, Servo Motor, Limit Switch, LEDs, Active Buzzer, Magnets, Card Reader, RFID reader, and PLA.

# Raspberry Pi
I had to install the evdev library for this, and you'll have to change your text file name and log file name. Note that I set up the start script and logging using crontab on the pi.

# Design
How it all gets wired is up to you if you were to make it, such as changing which pins are used, but here I'll explain what is connected to what. The card reader and arduino are connected to the raspberry pi using usb cables, the buzzer+ is connected to a 220 ohm resistor then the arduino, all three LEDs are the same. The limit switch, RFID, and servo motor are connected to the arduino. The fan+ is connected to arduino 5v, fan- is connected to an NPN collector then a 220 ohm resistor connected to the NPN base and the resistor to arduino pin. I am using a pwm pin for the fan so I can control the speed. I have a small bread board inside of the box making all of these connections, if you use the same method be sure to use cables over 3inches.
