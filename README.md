

Adapted by Bodmer to make use of the new Graphics library 
(Forum: https://forum.arduino.cc/index.php?topic=443787.120) and made it suit 480*320. (Have to change a few things here and there)

Thanx to Bodmer for whipping up a working touchsketch. I incorporated that code in the current version and also included the original sketch as seperate sketch. You need https://github.com/PaulStoffregen/XPT2046_Touchscreen for it to work.
The Dev branch has code for touch but that's a WIP. It's hashed out at the moment even though parts are working. (Zoom is working)   

The menustructure is (shortpress) 0 is ZoomAndPan, (Longer press) 1 MainMenu, (Even longer press) 2 manual update 
The menu's are defined in planespotter.cpp
The drawing of menu's is working.
The commands are defined in the sketch but don't work (yet)


Original adapted by Bodmer for use with latest TFT_ILI9341_ESP and JPEGDecoder library from his repository.

These are the connections assumed for this forked copy of the sketch:

  The typical setup for a NodeMCU1.0 (ESP-12 Module) is :
  
  * Display SDO/MISO      to NodeMCU pin D6 <<<<<< This is not used by this sketch
  * Display LED           to NodeMCU pin  5V or 3.3V via a 56 Ohm resistor
  * Display SCK           to NodeMCU pin D5
  * Display SDI/MOSI      to NodeMCU pin D7
  * Display DC/RS (or AO) to NodeMCU pin D3
  * Display RESET         to NodeMCU pin D4 <<<<<< Or connect to NodeMCU RST pin
  * Display CS            to NodeMCU pin D8
  * Display GND           to NodeMCU pin GND (0V)
  * Display VCC           to NodeMCU pin 5V or 3.3V
  
Note: only some versions of the NodeMCU provide the USB 5V on the VIN pin, To confuse
matters some NodeMCU clone board pins are mislabeled and D4 and D5 are swapped!

There are some variants of the common 2.2" and 2.4" ILI9341 based displays:

  * Some are fitted with a transistor switch so the LED can be PWM controlled
  * Some have no transistor and no current/brightness limit resistor, so use 56 Ohms
  between the LED pin and 5V (or 3.3V)

If 5V is not available at a pin you can use 3.3V but backlight brightness
will be lower.
  
If the TFT RESET signal is connected to the NodeMCU RST line then define the pin
in the TFT library User_Config.h file as negative so the library ignores it,
e.g. TFT_RST -1

The following images are screenshots grabbed from an ILI9341 320x240 pixel display:

<a href="http://imgur.com/tAfLJSf"><img src="http://i.imgur.com/tAfLJSf.png" title="source: imgur.com" /></a>

<a href="http://imgur.com/Kh3NMid"><img src="http://i.imgur.com/Kh3NMid.png" title="source: imgur.com" /></a>



##Original master repository text follows:

#ESP8266 Plane Spotter Color

This is the repository of the ESP8266 Plane Spotter Color. It downloads data from web APIs and displays aircrafts close
to your location on a map.



## The related blog post
https://blog.squix.org/2017/01/esp8266-planespotter-color.html

## Video
[![ESP8266 Plane Spotter Color](http://img.youtube.com/vi/4pTkoMsl1H4/0.jpg)](http://www.youtube.com/watch?v=4pTkoMsl1H4 "Plane Spotter Color")

## Features
* Beautiful startup splash screen
* Automatic geo location by using WiFi scanning. List of visible SSIDs identifies your location
* Automatic download of JPEGs from MapQuest
* Detailed information about the nearest aircraft
* Flight track: last 20 waypoints per aircraft displayed

## Planed Features
* Enable touch screen
  * Zoom in/out by button
  * shift map center
  * call location service again
  * select aircraft of interest
* only download map if center of map, scale or map type changed

## Known Issues
* Flickering with every update: not enough memory for frame/ double buffering
* Sometimes waypoints get lost
* Encoding problems when displaying airport names containing non-ASCII characters (e.g. Zürich)


## Hardware Requirements

This project was built for the following hardware:
* ESP8266 Wifi chip, especially with the Wemos D1 Mini, but all other ESP8266 modules should work as well. You can *support my blog* and buy it on my shop: https://blog.squix.org/product/d1-mini-nodemcu-lua-wifi-esp8266-development-board
* ILI9341/ XPT2046 TFT display with touch screen. At the moment the touch screen part is not used but I hope to extend it at a later time.
https://www.aliexpress.com/item/240x320-2-4-SPI-TFT-LCD-Touch-Panel-Serial-Port-Module-with-PBC-ILI9341-5-3/2031268807.html

Optionally you can get the connector PCB in the kicad sub directory. This allows for a easy soldering

## Wiring/ Schema

If you are currently prototyping this shows how to setup the connections for the above mentioned ILI9341 display

![Wiring](images/PlaneSpotterWiring.png)

![Schema](images/PlaneSpotterSchema.png)


## Libraries

Install the following libraries:

### WifiManager by tzapu

![WifiManager](images/WifiManagerLib.png)

### Json Streaming Parser by Daniel Eichhorn

![Json Streaming Parser] (images/JsonStreamingParserLib.png)

### JPEGDecoder, fork by Frederic Plante

This is (not yet?) available through the library manager. You have to download it from here and add it to the Arduino IDE
https://github.com/fredericplante/JPEGDecoder

*Attention:* You'll also have to open User_config.h in Arduino/libraries/JPEGDecoder-master and change
```
#define USE_SD_CARD
//#define USE_SPIFFS
```
into
```
//#define USE_SD_CARD
#define USE_SPIFFS
```
### Adafruit GFX by Adafruit

![Adafruit GFX Lib](images/AdafruitGFXLib.png)

## Credits

This project wouldn't be possible if not for many open source contributors. Here are some I'd like to mention:
* Frédéric Plante for his adaptations of the JPEGDecoder library
* tzapu for the WifiManager library
* Rene Nyfenegger for the base64 encoder I got from here: http://www.adp-gmbh.ch/cpp/common/base64.html
* Adafruit for the ILI9341 driver and potentially also for the original designs of the TFT display
