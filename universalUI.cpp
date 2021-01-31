/*
UniversalUI - often-used user interface functionality

Copyright (C) 2020  Matthias Clauﬂ

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#include <Print.h>
#include <Streaming.h>
#if defined(ESP32) // ESP32 board
#include <WiFi.h>
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#elif defined(ESP8266) // ESP8266 board
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#endif
// #include <WiFiUdp.h>
#include <NTPClient.h>
#include <SPIFFS.h>
extern "C"
{
#include "esp_spiffs.h"
}

#include "universalUIsettings.h"
#include "universalUI.h"

const char *ssid = LOCAL_WIFI_SSID;
const char *wpsk = LOCAL_WIFI_WPSK;

blinkDuration_t OTA_ERROR_BLINK[4] = {125, 125, 875, 125};
const int TIME_UNIT_DIVIDER[] = {1000, 60, 60, 24, 0};
String TIME_UNIT_LABEL[] = {"ms", "sek", "min", "h", "d"};

void UniversalUI::reconnectWifi()
{
#if defined(ESP32) || defined(ESP8266)
    logWarn() << "No connection, performing Wifi reset\n";
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

void UniversalUI::initOTA()
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
                if (esp_spiffs_mounted(NULL)) // check if begin() has been called before
                {
                    SPIFFS.end();
                }
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

void UniversalUI::init(const int statusLedPin, const bool statusLedActiveOnLow, const __FlashStringHelper *mainFileName, const __FlashStringHelper *buildTimestamp)
{
    Serial.begin(UNIVERSALUI_SERIAL_BAUDRATE);
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

boolean UniversalUI::handle()
{
    if (nullptr != _statusLed)
        _statusLed->update();
#if defined(ESP32) || defined(ESP8266)
    if ((WiFi.status() != WL_CONNECTED) && (millis() - _lastWifiReconnectCheck) > UNIVERSALUI_WIFI_RECONNECT_PERIOD)
    {
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

void UniversalUI::setBlink(const int onMillis, const int offMillis)
{
    if (nullptr != _statusLed)
    {
        _statusLed->setBlink(onMillis, offMillis);
    }
}

void UniversalUI::statusLedOff()
{
    if (nullptr != _statusLed)
        _statusLed->off();
}
void UniversalUI::statusLedOn()
{
    if (nullptr != _statusLed)
        _statusLed->on();
}

void UniversalUI::statusActive(const char *message)
{
    Serial << "setting status to active: " << message << endl;
    statusLedOn();
    _statusMessage = message;
}
void UniversalUI::statusError(const char *message)
{
    Serial << "setting status to error: " << message << endl;
    setBlink(125, 125);
    _statusMessage = message;
}
void UniversalUI::statusErrorOta(const char *errorText)
{
    Serial << "setting status to error: " << errorText << endl;
    if (nullptr != _statusLed)
        _statusLed->setBlinkPattern4(OTA_ERROR_BLINK);
    _statusMessage = errorText;
}
void UniversalUI::statusOk()
{
    Serial << "setting status to ok" << endl;
    statusLedOff();
    _statusMessage = "";
}

const char *UniversalUI::getHtmlLog(const byte part)
{
    return _log.getLog(part);
}

Print &UniversalUI::log(const __FlashStringHelper *prefix)
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

Print &UniversalUI::logError()
{
    return log(F("ERROR \t"));
}

Print &UniversalUI::logWarn()
{
    return log(F("WARN \t"));
}

Print &UniversalUI::logInfo()
{
    return log(F("INFO  \t"));
}

Print &UniversalUI::logDebug()
{
    return log(F("DEBUG \t"));
}

Print &UniversalUI::logTrace()
{
    return log(F("TRACE \t"));
}
/** @return append position */
char *UniversalUI::printTimeInterval(char *buf, word m, byte idx)
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
void UniversalUI::appendTimeInterval(AppendBuffer buf, word m, byte idx)
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

void UniversalUI::startActivity()
{
    ++_activityCount;
    if (1 == _activityCount)
    {
        statusLedOn();
    }
}
void UniversalUI::checkStatusLed()
{
    if (0 == _userErrorMessageBlinkTill)
    {
        if (0 == _activityCount)
            statusLedOff();
        else
            statusLedOn();
    }
}
/** Notfies about a finished activity. */
void UniversalUI::finishActivity()
{
    --_activityCount;
    checkStatusLed();
}

void UniversalUI::reportUiError(const char *ptrToMessage, const byte blinkDurationInSeconds)
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
void UniversalUI::clearUiError()
{
    _userErrorMessage = nullptr;
    _userErrorMessageBlinkTill = 0;
    checkStatusLed();
}
bool UniversalUI::hasUiError()
{
    return nullptr != _userErrorMessage;
}
const char *UniversalUI::getUiErrorMessage()
{
    return _userErrorMessage;
}
