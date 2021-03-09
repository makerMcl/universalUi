/*
UniversalUI - often-used user interface functionality

Copyright (C) 2020  Matthias Clauß

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#ifndef UNIVERSALUI_H
#define UNIVERSALUI_H

#include <Arduino.h>
#include <Streaming.h>
#include <NTPClient.h>
#include <Print.h>
#if defined(ESP32) // ESP32 board
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <SPIFFS.h>
extern "C"
{
#include "esp_spiffs.h"
}
#elif defined(ESP8266) // ESP8266 board
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <FS.h>
#endif

#include "universalUIsettings.h"
#include "logBuffer.h"
#include "blinkLed.h"
#include "appendBuffer.h"

// configuration section, to be modified via earlier #define's
#ifndef NTP_UPDATE_INTERVAL
#define NTP_UPDATE_INTERVAL 3600000 // every hour, in [ms]
#define NTP_RETRY_INTERVAL 60000    // once a minute, in [ms]
#endif
#ifndef NTP_INITIAL_TRIES
#define NTP_INITIAL_TRIES 3
#endif

#ifndef UNIVERSALUI_WIFI_MAX_CONNECT_TRIES
#define UNIVERSALUI_WIFI_MAX_CONNECT_TRIES 10 // between each try is a delay of UNIVERSALUI_WIFI_RECONNECT_WAIT (defaults to 500ms), we should wait 3seconds at least
#endif
#ifndef UNIVERSALUI_WIFI_RECONNECT_WAIT
#define UNIVERSALUI_WIFI_RECONNECT_WAIT 500 // in [ms], delay between next WiFi status check
#endif
#ifndef UNIVERSALUI_WIFI_RECONNECT_PERIOD
#define UNIVERSALUI_WIFI_RECONNECT_PERIOD 30000 // 30sec in [ms], time to wait between WiFi reconnect attempts
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

static const int TIME_UNIT_DIVIDER[] = {1000, 60, 60, 24, 0}; // last divider must be zero to indicate end of array
static String TIME_UNIT_LABEL[] = {"ms", "sek", "min", "h", "d"};

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
    // member constants

    blinkDuration_t OTA_ERROR_BLINK[4] = {125, 125, 875, 125};
    const char *ssid = LOCAL_WIFI_SSID;
    const char *wpsk = LOCAL_WIFI_WPSK;
    // variables

    const char *_appname;
    BlinkLed *_statusLed;
    String _statusMessage = "";
    LogBuffer _log = LogBuffer(true);
    bool _otaActive = false;
    NTPClient *_timeClient = NULL;
    bool _ntpTimeValid = false;
    unsigned long _lastNtpUpdateMs = 0;
    unsigned long _lastWifiReconnectCheck = 0;
    const char *_userErrorMessage = nullptr;
    word _userErrorMessageBlinkTill = 0;
    /**
     * Number of current, independent activities.
     * As long there is activity, status LED shall be on.
     */
    byte _activityCount = 0;

    void initOTA()
    {
#if defined(ESP32) || defined(ESP8266)
        ArduinoOTA
            .onStart([this]() {
                String type;
                if (ArduinoOTA.getCommand() == U_FLASH)
                    type = "sketch";
                else
                { // U_SPIFFS
                    type = "filesystem";
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wdeprecated-declarations"
#if defined(ESP32)
                    if (esp_spiffs_mounted(NULL)) // check if begin() has been called before
                    {
                        SPIFFS.end();
                    }
#elif defined(ESP8266)
                    SPIFFS.end();
#endif
#pragma GCC diagnostic pop
                }
                statusActive("OTA update");
                _otaActive = true;
                Serial.println("Start updating " + type);
            });
        ArduinoOTA.onEnd([this]() {
            _otaActive = false;
            Serial.println(" End");
        });
        ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
            // OTA uploads in 1024 byte chunks
            const byte part = (progress * 100 / total);
            if ((part % 10 == 0) && (part > ((progress - 1024) * 100 / total)))
            {
                Serial.printf("%u%%\n", part);
            }
            else
            {
                Serial.print('.');
            }
        });
        ArduinoOTA.onError([this](ota_error_t error) {
            Serial.printf("OTA Error[%u]: ", error);
            char reason[30];
            if (error == OTA_AUTH_ERROR)
                strcpy(reason, "OTA error: Auth Failed");
            else if (error == OTA_BEGIN_ERROR)
                strcpy(reason, "OTA error: Begin Failed");
            else if (error == OTA_CONNECT_ERROR)
                strcpy(reason, "OTA error: Connect Failed");
            else if (error == OTA_RECEIVE_ERROR)
                strcpy(reason, "OTA error: Receive Failed");
            else if (error == OTA_END_ERROR)
                strcpy(reason, "OTA error: End Failed");
            else
                strcpy(reason, "OTA error: unknown");
            Serial.println(reason);
            statusErrorOta(reason);
            _otaActive = false;
        });
        ArduinoOTA.setHostname(_appname);
        ArduinoOTA.setPort(OTA_PORT);
        ArduinoOTA.setPasswordHash(OTA_AUTH_MD5);
        ArduinoOTA.begin();
#endif
    }

    void reconnectWifi()
    {
#if defined(ESP32) || defined(ESP8266)
        WiFi.persistent(false);
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        WiFi.mode(WIFI_STA);
        // WiFi.config(ip, gateway, subnet); // Only for fix IP needed
        WiFi.begin(ssid, wpsk);
        int triesLeft = UNIVERSALUI_WIFI_MAX_CONNECT_TRIES;
        while (WiFi.status() != WL_CONNECTED && triesLeft > 0)
        {
            Serial.print(".");
            --triesLeft;
            delay(UNIVERSALUI_WIFI_RECONNECT_WAIT);
        }
        if (WiFi.status() == WL_CONNECTED)
        {
            Serial << "\nConnected with IP=" << WiFi.localIP() << endl;
        }
        else
        {
            Serial << "\nConnect failed, status=" << WiFi.status() << " (";
            switch (WiFi.status())
            {
            case WL_IDLE_STATUS:
                Serial << "IDLE";
                break;
            case WL_NO_SSID_AVAIL:
                Serial << "NO_SSID_AVAIL";
                break;
            case WL_SCAN_COMPLETED:
                Serial << "SCAN_COMPLETED";
                break;
            case WL_CONNECT_FAILED:
                Serial << "CONNECT_FAILED";
                break;
            case WL_CONNECTION_LOST:
                Serial << "CONNECTION_LOST";
                break;
            case WL_DISCONNECTED:
                Serial << "DISCONNECTED";
                break;
            default:
                Serial << "unknown";
            };
            Serial << ")" << endl;
#ifdef UNIVERSALUI_WIFI_REBOOT_ON_FAILED_CONNECT
            Serial << "restarting..." << endl;
            delay(UNIVERSALUI_WIFI_RECONNECT_WAIT);
            ESP.restart();
#endif
        }
        _lastWifiReconnectCheck = millis();
#endif
    }

    void statusErrorOta(const char *errorText)
    {
        Serial << "setting status to (ota) error: " << errorText << endl;
        if (nullptr != _statusLed)
            _statusLed->setBlinkPattern4(OTA_ERROR_BLINK);
        _statusMessage = errorText;
    }

    Print &log(const __FlashStringHelper *prefix)
    {
        if (NULL != _timeClient && _ntpTimeValid)
        {
            _log << _timeClient->getFormattedTime();
        }
        else
        {
            _log << _WIDTH(millis(), 8);
        }
        _log << F("   ") << prefix;
        return _log;
    }

    /** @return append position */
    static char *printTimeInterval(char *buf, word m, byte idx)
    {
        const byte v = m % TIME_UNIT_DIVIDER[idx];
        if (0 < TIME_UNIT_DIVIDER[idx])
        {
            m /= TIME_UNIT_DIVIDER[idx];
            if (m > 0)
            {
                buf = printTimeInterval(buf, m, idx + 1);
                buf += sprintf(buf, ", ");
            }
        }
        buf += sprintf_P(buf, PSTR("%d%s"), v, &(TIME_UNIT_LABEL[idx][0]));
        return buf;
    }

    static void appendTimeInterval(AppendBuffer buf, word m, byte idx)
    {
        const byte v = m % TIME_UNIT_DIVIDER[idx];
        if (0 < TIME_UNIT_DIVIDER[idx])
        {
            m /= TIME_UNIT_DIVIDER[idx];
            if (m > 0)
            {
                appendTimeInterval(buf, m, idx + 1);
                buf.append_P(F(", "));
            }
        }
        buf.printf_P(PSTR("%d%s"), v, &(TIME_UNIT_LABEL[idx][0]));
    }

    void checkStatusLed()
    {
        if (0 == _userErrorMessageBlinkTill)
        {
            if (0 == _activityCount)
                statusLedOff();
            else
                statusLedOn();
        }
    }

public:
    /**
     * Creates UniversalUI component with no status LED.
     * @param appname name of application. Used for hostname, OTA id and page header.
     */
    UniversalUI(const char *appname)
    {
        _appname = appname;
    }

    const char *getAppName() const
    {
        return _appname;
    }

    void setNtpClient(NTPClient *timeClient)
    {
        _timeClient = timeClient;
    }

    bool isNtpTimeValid()
    {
        return _timeClient != NULL && _ntpTimeValid;
    }

    String getFormattedTime()
    {
        return (isNtpTimeValid()) ? _timeClient->getFormattedTime() : "";
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
    void init(const int statusLedPin, const bool statusLedActiveOnLow, const __FlashStringHelper *mainFileName, const __FlashStringHelper *buildTimestamp)
    {
        Serial.begin(UNIVERSALUI_SERIAL_BAUDRATE);
        while (!Serial)
            ;
        logInfo() << "Sketchname: " << mainFileName << ", Build: " << buildTimestamp << ", SDK: " << ESP.getSdkVersion() << endl;
        //Serial <<"compiler version: "<< __VERSION__<<endl;
        if (NOT_A_PIN != statusLedPin)
        {
            Serial << "setting status pin to " << statusLedPin << endl;
            _statusLed = new BlinkLed();
            _statusLed->init(statusLedPin, statusLedActiveOnLow ? ACTIVE_LOW : ACTIVE_HIGH);
        }
        else
        {
            _statusLed = nullptr;
        }

#if defined(ESP32) || defined(ESP8266)
        Serial << endl
               << "MAC address is " << WiFi.macAddress() << endl;

#if defined(ESP32)
        WiFi.setHostname(_appname);
#elif defined(ESP8266)
        WiFi.hostname(_appname);
#endif
#if defined(ESP32) || defined(ESP8266)
        reconnectWifi();
#endif

        initOTA();
        if (NULL != _timeClient)
        {
            _timeClient->begin();
            int ntpTries = NTP_INITIAL_TRIES;
            do
            {
                _ntpTimeValid = _timeClient->forceUpdate();
                if (_ntpTimeValid)
                {
                    _lastNtpUpdateMs = millis();
                    logInfo() << "initialized NTP client at millis()=" << _lastNtpUpdateMs << ", time is " << _timeClient->getFormattedTime() << endl;
                    ntpTries = 0;
                    break;
                }
                --ntpTries;
                logError() << "failed getting NTP time (" << ntpTries << ")" << endl;
                delay(500);
            } while (ntpTries > 0);
        }
#endif
        Serial << "\nReady\n\n";
    }

    /**
     * Sets current blink interval. LED off is 0, 0.
     */
    void setBlink(const int onMillis, const int offMillis)
    {
        if (nullptr != _statusLed)
        {
            _statusLed->setBlink(onMillis, offMillis);
        }
    }

    /**
     * Stops current status led effect and switches it off.
     * Leaves the current status messae. Might be used to indicate activity during an action.
     */
    void statusLedOff()
    {
        if (nullptr != _statusLed)
            _statusLed->off();
    }

    /**
     * Immediately switches status led on, with no status message.
     * Led will be switched off with setBlink()/blinkError() or statusOff().
     */
    void statusLedOn()
    {
        if (nullptr != _statusLed)
            _statusLed->on();
    }

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
        checkStatusLed();
    }

    void statusActive(const char *message)
    {
        Serial << "setting status to active: " << message << endl;
        statusLedOn();
        _statusMessage = message;
    }

    void statusError(const char *message)
    {
        Serial << "setting status to error: " << message << endl;
        setBlink(125, 125);
        _statusMessage = message;
    }

    void statusOk()
    {
        Serial << "setting status to ok" << endl;
        statusLedOff();
        _statusMessage = "";
    }

    /** Indicates error in user interaction (no system error).
     * @param ptrToMessage message to show in webUI (must be a static string: universalUi only stores the reference to it!)
     * @param durationInSeconds how many seconds to error blink
     */
    void reportUiError(const char *ptrToMessage, const byte blinkDurationInSeconds)
    {
        _userErrorMessage = ptrToMessage;
        _userErrorMessageBlinkTill = millis() + blinkDurationInSeconds * 1000;
        if (_userErrorMessageBlinkTill < millis())
        {
            // handle overflow: shorten blink time, blink till overflow value
            logDebug() << F("ui error blink with overflow: millis=") << millis() << F(", should blink till") << _userErrorMessageBlinkTill << endl;
            _userErrorMessageBlinkTill = -1000;
        }
        if (nullptr != _statusLed)
            setBlink(200, 300);
    }

    /** Indicate that the error in user interaction has been resolved. */
    void clearUiError()
    {
        _userErrorMessage = nullptr;
        _userErrorMessageBlinkTill = 0;
        checkStatusLed();
    }

    bool hasUiError()
    {
        return nullptr != _userErrorMessage;
    }

    const char *getUiErrorMessage()
    {
        return _userErrorMessage;
    }

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
    bool handle()
    {
        if (nullptr != _statusLed)
            _statusLed->update();
#if defined(ESP32) || defined(ESP8266)
        if ((WiFi.status() != WL_CONNECTED) && (millis() - _lastWifiReconnectCheck) > UNIVERSALUI_WIFI_RECONNECT_PERIOD)
        {
            logWarn() << "No connection, performing Wifi reset\n";
            reconnectWifi();
        }

        ArduinoOTA.handle();
        if (_otaActive)
        {
            return false;
        }
#endif
        // handle ui error blink off
        if (_userErrorMessageBlinkTill > 0 && (millis() > _userErrorMessageBlinkTill))
        {
            if (0 == _activityCount)
                statusLedOff();
            else
                statusLedOn();
        }
        // update cycle for NTP queries
        if (NULL != _timeClient && ((long)(millis() - _lastNtpUpdateMs) >= (_ntpTimeValid ? NTP_UPDATE_INTERVAL : NTP_RETRY_INTERVAL)))
        {
            _ntpTimeValid = _timeClient->forceUpdate();
            if (_ntpTimeValid)
                logInfo("time updated successfully from NTP");
            else
                logError("time update failed from NTP");
            _lastNtpUpdateMs = millis();
        }
        return true;
    }

    Print &logError()
    {
        return log(F("ERROR \t"));
    }

    Print &logWarn()
    {
        return log(F("WARN \t"));
    }

    Print &logInfo()
    {
        return log(F("INFO  \t"));
    }

    Print &logDebug()
    {
        return log(F("DEBUG \t"));
    }

    Print &logTrace()
    {
        return log(F("TRACE \t"));
    }

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

    /** 
     * Note: LogBuffer implements fix for https://github.com/me-no-dev/ESPAsyncWebServer/issues/333: '%' in template result is evaluated as template again
     */
    const char *getHtmlLog(const byte part)
    {
        return _log.getLog(part);
    }

    static void printTimeInterval(char *buf, word millis)
    {
        printTimeInterval(buf, millis, 0);
    }
    static void appendTimeInterval(AppendBuffer buf, word millis)
    {
        appendTimeInterval(buf, millis, 0);
    }
}; // of class UniversalUI
#endif
