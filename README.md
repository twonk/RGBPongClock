RGBPongClock
============
RGB Pong Clock - Andrew Holmes [@pongclock](http://twitter.com/PongClock)
Inspired by, and shamelessly derived from
[Nick's LED Projects](https://123led.wordpress.com/about/)
  
Videos of the clock in action:

* https://vine.co/v/hwML6OJrBPw
* https://vine.co/v/hgKWh1KzEU0
* https://vine.co/v/hgKz5V0jrFn
    
I run this on a Mega 2560, your milage on other chips may vary,can definately free up some memory if the bitmaps are shrunk down to size.
  
Uses an Adafruit 16x32 RGB matrix availble from here:
* http://www.phenoptix.com/collections/leds/products/16x32-rgb-led-matrix-panel-by-adafruit

This microphone:
* http://www.phenoptix.com/collections/adafruit/products/electret-microphone-amplifier-max4466-with-adjustable-gain-by-adafruit-1063

a DS1307 RTC chip (not sure where I got that from - was a spare)

and an Ethernet Shield
* http://hobbycomponents.com/index.php/dvbd/dvbd-ardu/ardu-shields/2012-ethernet-w5100-network-shield-for-arduino-uno-mega-2560-1280-328.html

Arduino requests weather data from a local VM running node.js which, in turn, requests the forecast from OpenWeatherMap.org The node script parses the feed and returns the relevant data to the arduino.
