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

#ifndef NTP_UPDATE_INTERVAL
#define NTP_UPDATE_INTERVAL 3600000 // every hour, in [ms]
#define NTP_RETRY_INTERVAL 60000    // once a minute, in [ms]
#endif

//#define COPY_TO_SERIAL // if logged messages should be immediately printed on Serial

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
    BlinkLed _statusLed = BlinkLed();
    String _statusMessage = "";
    LogBuffer _log = LogBuffer();
    bool _otaActive = false;
    NTPClient *_timeClient = NULL;
    bool _ntpTimeValid = false;
    word _lastNtpUpdateMs = 0;
    /**
     * Number of current, independent activities.
     * As long there is activity, status LED shall be on.
     */
    byte _activityCount = 0;

    void initOTA();
    void initWifi(const char *SSID, const char *WPSK);
    void statusErrorOta(const char *errorText);
    Print &log(const char *prefix);
    static char *printTimeInterval(char *buf, word m, byte idx);

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
     */
    void init()
    {
        init(NOT_A_PIN);
    }

    /**
     * Initializes UniversalUI component with status LED at the given pin, active at high.
     */
    void init(const int statusLedPin)
    {
        init(statusLedPin, false);
    }

    /**
     * Initializes UniversalUI component with status LED at the given pin.
     */
    void init(const int statusLedPin, const bool statusLedActiveOnLow);

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
    void startActivity()
    {
        ++_activityCount;
        if (1 == _activityCount)
        {
            statusLedOn();
        }
    }
    /** Notfies about a finished activity. */
    void finishActivity()
    {
        --_activityCount;
        if (0 == _activityCount)
        {
            statusLedOff();
        }
    }

    void statusActive(const char *message);

    void statusError(const char *message);

    void statusOk();

    const char *getStatusMessage()
    {
        return &_statusMessage[0];
    }

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
    void logWarn(const String msg)
    {
        logWarn() << msg << endl;
    }
    void logInfo(const String msg)
    {
        logInfo() << msg << endl;
    }
    void logDebug(const String msg)
    {
        logDebug() << msg << endl;
    }

    const char *getHtmlLog(const byte part);

    static void printTimeInterval(char *buf, word millis);
}; // of class UniversalUI
#endif
