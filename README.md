# UniversalUI

This library is for common tasks of a user interface for IoT devices on ESP8266/ESP32.
It supports a status LED, storing log (depending on buffer size) and integrates basic functions like WiFi connect, OTA and optional NTP integration.


## supported controllers

Tested with Espressif's ESP32 and ESP8266.


## First steps & Dependencies

* add universalUI as lib into your project. Easiest way is to add git-submodule in `/lib`from https://github.com/makerMcl/universalUi.git (your should have found this file there)
* provide Streaming as dependency
	** Note: I use Streaming v6 for `_WIDTH`-feature: in platformio lib registry is still version 5
	** Workaround: add https://github.com/janelia-arduino/Streaming.git[`lib from the current maintainer`] as git-submodule in `lib/` directory
	** after v6 has been uploaded to platformio lib registry, in `platformio.ini` via `libs = Streaming` should suffice
* provide blinkLed.h as dependency: clone it as git-submodule into your `/lib`-folder, from https://github.com/makerMcl/blinkled.git
* copy `universalUIsettings.h_sample` into `universalUIsettings.h` and provide WLAN and OTA settings


## Using NTP time

* in `platformio.ini`: `libs = NTPClient`
* 


## Licence
Licenced under LGPL v3

For contact, create a github issue please.



