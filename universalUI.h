/*
UniversalUI - often-used user interface functionality

Copyright (C) 2020  Matthias Clauß

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#include <Arduino.h>
#include <Streaming.h>
#include <NTPClient.h>

#ifndef UNIVERSALUI_H
#define UNIVERSALUI_H

#include "universalUIsettings.h"
#include "logBuffer.h"
#include "blinkLed.h"

// configuration section, to be modified via earlier #define's
#ifndef NTP_UPDATE_INTERVAL
#define NTP_UPDATE_INTERVAL 3600000 // every hour, in [ms]
#define NTP_RETRY_INTERVAL 60000    // once a minute, in [ms]
#endif
#ifndef NTP_INITIAL_TRIES
#define NTP_INITIAL_TRIES 3
#endif

#ifndef UNIVERSALUI_WIFI_MAX_CONNECT_TRIES
#define UNIVERSALUI_WIFI_MAX_CONNECT_TRIES 5 // between each try is a delay of 500ms
#endif
#ifndef UNIVERSALUI_WIFI_RECONNECT_WAIT
#define UNIVERSALUI_WIFI_RECONNECT_WAIT 500 // in [ms], delay between next WiFi status check
#endif

// optional configuration settings, to be defined before including this file
//#define UNIVERSALUI_WIFI_REBOOT_ON_FAILED_CONNECT
//#define COPY_TO_SERIAL                    // if logged messages should be immediately printed on Serial

// following settings are per default adapted to default behaviour of the board
#ifndef UNIVERSALUI_SERIAL_BAUDRATE
#if defined(ESP32)
#define UNIVERSALUI_SERIAL_BAUDRATE 115200 // baud rate of ESP32's boot loader
#elif defined(ESP8266)
#define UNIVERSALUI_SERIAL_BAUDRATE 74800 // baud rate of ES8266's boot loader
#else
#define UNIVERSALUI_SERIAL_BAUDRATE 57600 // baud rate of ATmega328p's boot loader
#endif
#endif //of: #ifndef UNIVERSALUI_SERIAL_BAUDRATE

/**
 * Encapsules and supports status visualisation with several methods:<ul>
 * <li>status LED</li>
 * <li>serial interface (mainly for debugging)</li>
 * <li>via web page</li>
 * </ul>
 */
class UniversalUI
{
private:
    const char *_appname;
    BlinkLed *_statusLed = nullptr;
    String _statusMessage = "";
    LogBuffer _log = LogBuffer();
    bool _otaActive = false;
    NTPClient *_timeClient = NULL;
    bool _ntpTimeValid = false;
    word _lastNtpUpdateMs = 0;
    const char *_userErrorMessage = nullptr;
    word _userErrorMessageBlinkTill = 0;
    /**
     * Number of current, independent activities.
     * As long there is activity, status LED shall be on.
     */
    byte _activityCount = 0;

    void initOTA();
    void initWifi(const char *SSID, const char *WPSK);
    void statusErrorOta(const char *errorText);
    Print &log(const __FlashStringHelper *prefix);
    static char *printTimeInterval(char *buf, word m, byte idx);
    void checkStatusLed();

public:
    /**
     * Creates UniversalUI component with no status LED.
     * @param appname name of application. Used for hostname, OTA id and page header.
     */
    UniversalUI(const char *appname)
    {
        _appname = appname;
    }

    void setNtpClient(NTPClient *timeClient)
    {
        _timeClient = timeClient;
    }

    bool isNtpTimeValid()
    {
        return _timeClient != NULL && _ntpTimeValid;
    }

    /** 
     * To be called on <code>setUp();</code>.
     * @param mainFileName to be provided with macro <code>__FILE__</code>
     * @param buildTimestamp to be provided with macro <code>__TIMESTAMP__</code>
     */
    void init(const __FlashStringHelper *mainFileName, const __FlashStringHelper *buildTimestamp)
    {
        init(NOT_A_PIN, mainFileName, buildTimestamp);
    }

    /**
     * Initializes UniversalUI component with status LED at the given pin, active at high.
     * @param statusLedPin GPIO pin number where status led is attached
     * @param mainFileName to be provided with macro <code>__FILE__</code>
     * @param buildTimestamp to be provided with macro <code>__TIMESTAMP__</code>
     */
    void init(const int statusLedPin, const __FlashStringHelper *mainFileName, const __FlashStringHelper *buildTimestamp)
    {
        init(statusLedPin, false, mainFileName, buildTimestamp);
    }

    /**
     * Initializes UniversalUI component with status LED at the given pin.
     * @param statusLedPin GPIO pin number where status led is attached
     * @param statusLedActiveOnLow if true, LED is active on LOW; otherwise on HIGH
     * @param mainFileName to be provided with macro <code>__FILE__</code>
     * @param buildTimestamp to be provided with macro <code>__TIMESTAMP__</code>
     */
    void init(const int statusLedPin, const bool statusLedActiveOnLow, const __FlashStringHelper *mainFileName, const __FlashStringHelper *buildTimestamp);

    /**
     * Sets current blink interval. LED off is 0, 0.
     */
    void setBlink(const int onMillis, const int offMillis);

    /**
     * Stops current status led effect and switches it off.
     * Leaves the current status messae. Might be used to indicate activity during an action.
     */
    void statusLedOff();
    /**
     * Immediately switches status led on, with no status message.
     * Led will be switched off with setBlink()/blinkError() or statusOff().
     */
    void statusLedOn();

    /** Notifies about starting an activity. */
    void startActivity();
    /** Notfies about a finished activity. */
    void finishActivity();

    void statusActive(const char *message);

    void statusError(const char *message);

    void statusOk();

    /** Indicates error in user interaction (no system error).
     * @param ptrToMessage message to show in webUI (must be a static string: universalUi only stores the reference to it!)
     * @param durationInSeconds how many seconds to error blink
     */
    void reportUiError(const char *ptrToMessage, const byte blinkDurationInSeconds);
    /** Indicate that the error in user interaction has been resolved. */
    void clearUiError();
    bool hasUiError();
    const char *getUiErrorMessage();

    bool hasStatusMessage() { return '\0' != _statusMessage[0]; }
    const char *getStatusMessage() { return _statusMessage.c_str(); }

    /**
     * To be called in <code>loop()</code>.
     * <ul>
     * <li>updates state of blink pin</li>
     * <li>checks OTA</li>
     * </ul>
     * 
     * @return true if no internal activity and more workload can be processed
     */
    bool handle();

    Print &logError();
    Print &logWarn();
    Print &logInfo();
    Print &logDebug();
    Print &logTrace();
    void logError(const String msg)
    {
        logError() << msg << endl;
    }
    void logError(const __FlashStringHelper *msg)
    {
        logError() << msg << endl;
    }
    void logWarn(const String msg)
    {
        logWarn() << msg << endl;
    }
    void logWarn(const __FlashStringHelper *msg)
    {
        logWarn() << msg << endl;
    }
    void logInfo(const String msg)
    {
        logInfo() << msg << endl;
    }
    void logInfo(const __FlashStringHelper *msg)
    {
        logInfo() << msg << endl;
    }
    void logDebug(const String msg)
    {
        logDebug() << msg << endl;
    }
    void logDebug(const __FlashStringHelper *msg)
    {
        logDebug() << msg << endl;
    }

    const char *getHtmlLog(const byte part);

    static void printTimeInterval(char *buf, word millis);
}; // of class UniversalUI
#endif
