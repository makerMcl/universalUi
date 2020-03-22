/*
UniversalUI - often-used user interface functionality

Copyright (C) 2020  Matthias Clauß

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#include <Print.h>
#include <Streaming.h>
#ifdef ESP32 // Im Falle eines ESP32-Boards
#include <WiFi.h>
#else // bei einem ESP8266-Board
//#include <ESP8266WiFi.h>
#endif
#include <ESPmDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <NTPClient.h>

#include "universalUIsettings.h"
#include "universalUI.h"

const char *ssid = LOCAL_WIFI_SSID;
const char *wpsk = LOCAL_WIFI_WPSK;

word OTA_ERROR_BLINK[4] = {125, 125, 875, 125};
const int TIME_UNIT_DIVIDER[] = {1000, 60, 60, 24, 0};
String TIME_UNIT_LABEL[] = {"ms", "sek", "min", "h", "d"};

void UniversalUI::initWifi(const char *SSID, const char *WPSK)
{
    WiFi.setHostname(_appname);
    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, wpsk);
    while (WiFi.status() != WL_CONNECTED)
    {
        Serial.print(".");
        delay(500);
    }
    Serial << "\nConnected with IP=" << WiFi.localIP() << endl;
}

void UniversalUI::initOTA()
{
    ArduinoOTA
        .onStart([this]() {
            String type;
            if (ArduinoOTA.getCommand() == U_FLASH)
                type = "sketch";
            else // U_SPIFFS
                type = "filesystem";

            // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
            statusActive("OTA update");
            _otaActive = true;
            Serial.println("Start updating " + type);
        })
        .onEnd([this]() {
            _otaActive = false;
            Serial.println(" End");
        })
        .onProgress([](unsigned int progress, unsigned int total) {
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
        })
        .onError([this](ota_error_t error) {
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
}

void UniversalUI::init(const int statusLedPin, const bool statusLedActiveOnLow)
{
#if defined(ESP32) || defined(ESP8266)
    Serial.begin(115200);
#endif
    Serial.printf("\n\nSketchname: %s\nBuild: %s\tSDK: %s\n", (__FILE__), (__TIMESTAMP__), ESP.getSdkVersion());
    //Serial <<"compiler version: "<< __VERSION__<<endl;
    Serial << "MAC address is " << WiFi.macAddress() << "  ";

    if (NOT_A_PIN != statusLedPin)
    {
        Serial << "setting status pin to " << statusLedPin << endl;
        _statusLed.init(statusLedPin, statusLedActiveOnLow);
    }

#if defined(ESP32) || defined(ESP8266)
    initWifi(ssid, wpsk);
    initOTA();
    int ntpTries = 3;
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
#endif
    Serial << "\nReady\n\n";
}

boolean UniversalUI::handle()
{
    _statusLed.update();
    ArduinoOTA.handle();
    if (_otaActive)
    {
        return false;
    }
    // update cycle for NTP queries
    if (NULL != _timeClient && ((long)(millis() - _lastNtpUpdateMs) >= (_ntpTimeValid ? NTP_UPDATE_INTERVAL : NTP_RETRY_INTERVAL)))
    {
        _ntpTimeValid = _timeClient->forceUpdate();
        if (_ntpTimeValid)
        {
            logInfo("time updated successfully from NTP");
        }
        else
        {
            logError("time update failed from NTP");
        }
        _lastNtpUpdateMs = millis();
    }
    return true;
}

void UniversalUI::setBlink(const int onMillis, const int offMillis)
{
    _statusLed.setBlink(onMillis, offMillis);
}

void UniversalUI::statusLedOff()
{
    _statusLed.setBlink(0, 0);
}
void UniversalUI::statusLedOn()
{
    _statusLed.setBlink(-1, 0);
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
    _statusLed.setBlinkPattern4(OTA_ERROR_BLINK);
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

Print &UniversalUI::log(const char *prefix)
{
    if (NULL != _timeClient && _ntpTimeValid)
    {
        _log << _timeClient->getFormattedTime();
    }
    else
    {
        _log << _WIDTH(millis(), 8);
    }
    _log << ": " << prefix;
    return _log;
}

Print &UniversalUI::logError()
{
    return log("ERROR \t");
}

Print &UniversalUI::logWarn()
{
    return log("WARN \t");
}

Print &UniversalUI::logInfo()
{
    return log("INFO \t");
}

Print &UniversalUI::logDebug()
{
    return log("DEBUG \t");
}

Print &UniversalUI::logTrace()
{
    return log("TRACE \t");
}
void UniversalUI::printTimeInterval(char *buf, word m)
{
    printTimeInterval(buf, m, 0);
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
    buf += sprintf(buf, "%d%s", v, &(TIME_UNIT_LABEL[idx][0]));
    return buf;
}
