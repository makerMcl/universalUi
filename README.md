# UniversalUI

This library is for common tasks of a user interface for IoT devices, mainly beneficial on ESP8266/ESP32.
It supports a status LED, storing log (depending on buffer size) and integrates basic functions like WiFi (re-)connect, OTA and optional NTP integration.


## Supported controllers

Tested with Espressif's ESP32 and ESP8266 using Arduino framework to compile.


## Usage

### Statically available from anywhere

* include [`universalUIglobal.h`](universalUIglobal.h) and use instance variable `ui` where you need it
* see example usage in [`webUiGenericPlaceHolder.h`](webUiGenericPlaceHolder.h)

### Using NTP time

Uses NTP time if available, if not fall back to timestamps using `millis()`.

* dependency in `platformio.ini`: `libs = NTPClient`
* create instance in your main sketch `NTPClient *timeClient = new NTPClient(ntpUDP, "europe.pool.ntp.org", 3600, 60000);`
* inject instance into universalUi in method *setup()*: `ui.setNtpClient(timeClient);`

### Avoid repeated placeholders for AsyncWebServer

* include [`webUiGenericPlaceHolder.h`](webUiGenericPlaceHolder.h)
* see example usage in projects [calibrationServer](https://github.com/makerMcl/calibrationServer) and [espEnviServer](https://github.com/makerMcl/espEnviServer)



## Installation / Dependencies

* add universalUI as lib into your project. There are several options:
	* reference as library dependency from github: 

		`lib_deps = https://github.com/makerMcl/universalUi.git`

	* reference as separate directory, i.e. reuse among several projects: 
	
		`lib_extra_dirs = ../zCommon-libs`

	* add as git-submodule in `/lib` from https://github.com/makerMcl/universalUi.git (you should have found this file there)
* same goes for `blinkLed.h` as dependency, from https://github.com/makerMcl/blinkled.git
* provide Streaming as dependency
	* Note: I use Streaming v6 for `_WIDTH`-feature: in platformio lib registry is still version 5
	* Workaround: add [`lib from the current maintainer`](https://github.com/janelia-arduino/Streaming.git) as git-submodule in `lib/` directory
	* after v6 has been uploaded to platformio lib registry, in `platformio.ini` via `libs = Streaming` should suffice
* copy `universalUIsettings.h_sample` into `universalUIsettings.h` and provide WLAN and OTA settings



## Design considerations

* converted universalUi to header-only lib to avoid issue with different effective settings in archived compile artifacts of it (got an overflow/reboot bug due to that effect)
* universalUi is used as static instance on heap, to allow use via external reference in sub-libs of project
* SPIFFS is deprecated now, but necessary used by AsyncWebServer - thus the temporary deprecation disabling in method `serverSetup()`
* I prefer default baudrate of bootloader, thus using 74800 for Serial on ESP8266
* I prefer HTML template content (index.html, log.html) on SPIFFS not only to save flash but first because you can read that code more easily, including the template variables



## Licence
Licenced under GPL v3

For contact, create a github issue please.



## TODOs
* get unit tests running (see feature-file for more test specifications)
