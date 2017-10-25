Simple Temperature and Humidity Reporter
============================================

This project combines several components in order to have an automatic reporting of temperature and humidity based on the (rather low-precision) DHT11.
The main board is a D1-Mini (ESP-8266 based) and screen is a basic 128x32 OLED.

Here is the main idea:
![Breadboard](sketch.png)

Principle
---------

The D1-Mini allows to interface with all 3 other devices:

 * The DHT-11 sensor does only need a pin, we selected D5;
 * The OLED screen is communicating with I2C, the board has a hardware I2C interface with pings D1&D2;
 * The pushbutton will go to ground so we connect it to the pullup input of D3

We didn't use D4 as it's plugged in the blue LED, we can use the D4 pin as a LED indicator in case the I2C screen does not work.

All peripherals are connected in 5V.

