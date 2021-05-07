#ifndef WEBUI_GENERIC_PLACEHOLDER_H
#define WEBUI_GENERIC_PLACEHOLDER_H

#if defined(ESP32)
#include <rom/rtc.h>
#elif defined(ESP8266)
extern "C"
{
#include "user_interface.h"
}
#endif
#include "universalUIglobal.h"
#include "debug.h"

String universalUiPlaceholderProcessor(const String &var, AppendBuffer &buf)
{
    if (0 == strcmp_P(var.c_str(), PSTR("APPNAME")))
        return ui.getAppName();
    if (0 == strcmp_P(var.c_str(), PSTR("__TIMESTAMP__")))
        return F(__TIMESTAMP__);
    if (0 == strcmp_P(var.c_str(), PSTR("STATUS")))
        return ui.getStatusMessage();
    if (0 == strcmp_P(var.c_str(), PSTR("STATUSBAR")))
    {
        if (ui.hasStatusMessage())
        {
            buf.reset();
            //buf.printf_P(PSTR("<p style=\"color:blue;background-color:lightgrey;text-align:center;\">Status: %s</p>"), ui.getStatusMessage());
            buf.append_P(F("<p style=\"color:blue;background-color:lightgrey;text-align:center;\">Status: "));
            buf.append(ui.getStatusMessage());
            buf.append_P(F("</p>"));
            return buf.c_str();
        }
        else
        {
            return "";
        }
    }
    if (0 == strcmp_P(var.c_str(), PSTR("RESET_REASON")))
    {
#if defined(ESP32)
        switch (rtc_get_reset_reason(0))
        {
        case 1:
            return F("POWERON_RESET"); /**<1,  Vbat power on reset*/
        case 3:
            return F("SW_RESET"); /**<3,  Software reset digital core*/
        case 4:
            return F("OWDT_RESET"); /**<4,  Legacy watch dog reset digital core*/
        case 5:
            return F("DEEPSLEEP_RESET"); /**<5,  Deep Sleep reset digital core*/
        case 6:
            return F("SDIO_RESET"); /**<6,  Reset by SLC module, reset digital core*/
        case 7:
            return F("TG0WDT_SYS_RESET"); /**<7,  Timer Group0 Watch dog reset digital core*/
        case 8:
            return F("TG1WDT_SYS_RESET"); /**<8,  Timer Group1 Watch dog reset digital core*/
        case 9:
            return F("RTCWDT_SYS_RESET"); /**<9,  RTC Watch dog Reset digital core*/
        case 10:
            return F("INTRUSION_RESET"); /**<10, Instrusion tested to reset CPU*/
        case 11:
            return F("TGWDT_CPU_RESET"); /**<11, Time Group reset CPU*/
        case 12:
            return F("SW_CPU_RESET"); /**<12, Software reset CPU*/
        case 13:
            return F("RTCWDT_CPU_RESET"); /**<13, RTC Watch dog Reset CPU*/
        case 14:
            return F("EXT_CPU_RESET"); /**<14, for APP CPU, reseted by PRO CPU*/
        case 15:
            return F("RTCWDT_BROWN_OUT_RESET"); /**<15, Reset when the vdd voltage is not stable*/
        case 16:
            return F("RTCWDT_RTC_RESET"); /**<16, RTC Watch dog reset digital core and rtc module*/
        default:
            return F("NO_MEAN");
        }
#elif defined(ESP8266)
        return ESP.getResetReason();
#else
        return F("???");
#endif
    }
    if (0 == strcmp_P(var.c_str(), PSTR("SYSTIME")))
    {
        buf.reset();
        if (ui.isNtpTimeValid())
        {
            buf.printf_P(PSTR("%lu @ %s"), millis(), ui.getFormattedTime().c_str());
            return buf.c_str();
        }
        else
        {
            buf.printf_P(PSTR("%lu ms"), millis());
            return buf.c_str();
        }
    }
    if (0 == strcmp_P(var.c_str(), PSTR("USERMESSAGE")))
    {
        if (ui.hasUiError())
        {
            buf.reset();
            buf.printf_P(PSTR("<h3 style='color:red;'>%s</h3>"), &ui.getUiErrorMessage()[0]);
            return buf.c_str();
        }
        else
            return "";
    }
    ui.logError() << F("DEBUG: variable not found: ") << var << endl;
    return F("???");
}

const String PARAM_REFRESH = "r";

class RefreshState
{
private:
    byte _webuiRefresh = 0; // time interval in [seconds] for page refresh
    bool _webuiRefreshEnabled = true;
    const byte _refreshTime;

public:
    RefreshState(const byte refreshTime = 1) : _refreshTime(refreshTime) {}

    void evaluateRefreshParameters(AsyncWebServerRequest *request)
    {
        if (request->hasParam(PARAM_REFRESH))
        {
            const String r = request->getParam(PARAM_REFRESH)->value();
            _webuiRefresh = r.toInt();
        }
    }

    String getRefreshTag(AppendBuffer &buf, const String uri)
    {
        if (_webuiRefreshEnabled && _webuiRefresh > 0)
        {
            buf.reset();
            buf.printf_P(PSTR("<meta http-equiv=\"refresh\" content=\"%d;url=%s?r=%d#refresh\">"), _webuiRefresh, uri.c_str(), _webuiRefresh);
            return buf.c_str();
        }
        else
        {
            return "";
        }
    }

    String getRefreshLink(AppendBuffer &buf, const String uri)
    {
        buf.reset();
        buf.printf(("<a href=\"%s?r=%d\">"), uri.c_str(), (_webuiRefreshEnabled && _webuiRefresh > 0) ? 0 : _refreshTime);
        if (_webuiRefreshEnabled && _webuiRefresh > 0)
            buf.append_P(F("Stop"));
        else
            buf.append_P(F("Start"));
        buf.append_P(F(" Refresh</a>"));
        return buf.c_str();
    }
};

static const char phPattern[] = {'$', 'L', 'O', 'G', '$'};                   // pattern to replace with content from log buffer
static const size_t phPatternLen = sizeof(phPattern) / sizeof(phPattern[0]); // length of pattern

class FileWithLogBufferResponseDataSource : public AwsResponseDataSource
{
private:
    fs::File _content;
    size_t fileReadPosition = 0;     // how many bytes for file read
    size_t patternFoundPosition = 0; // next location of phPattern char to match
    size_t bufferSourceIndex = 0;    // next index from log buffer, -1 for reading from file source
    size_t bufferRotationPoint;

public:
    FileWithLogBufferResponseDataSource(fs::FS &fs, const String &path)
    {
        _content = fs.open(path, "r");
    }

    virtual size_t fillBuffer(uint8_t *buf, size_t maxLen, size_t index)
    {
        size_t filledLen = 0;
        size_t targetMaxLen = maxLen;
        uint8_t *targetBuf = buf;
        LOGBUFFER_DEBUG("webui: start of fillBuffer with maxLen=", maxLen)
        LOGBUFFER_DEBUGN(", index=", index)
        if ((bufferSourceIndex > 0) || (patternFoundPosition >= phPatternLen))
        { // we are (still) reading from the log buffer
            size_t bufferReadLen;
            // read data from log buffer as long more data available and space left in buf
            do
            {
                bufferReadLen = ui.getHtmlLog(targetBuf, targetMaxLen, bufferSourceIndex, bufferRotationPoint);
                if (RESPONSE_TRY_AGAIN == bufferReadLen)
                {
                    return filledLen > 0 ? filledLen : RESPONSE_TRY_AGAIN;
                }
                bufferSourceIndex += bufferReadLen;
                filledLen += bufferReadLen;
                targetBuf += bufferReadLen;
                targetMaxLen -= bufferReadLen;
                LOGBUFFER_DEBUG("  webui: loaded buffer chunk ", bufferReadLen)
                LOGBUFFER_DEBUG(", maxLen=", maxLen)
                LOGBUFFER_DEBUGN(", targetMaxLen=", targetMaxLen)
#ifdef VERBOSE_DEBUG_LOGBUFFER
                Serial.print("  webui: 3>");
                for (int i = 0; i < filledLen; ++i)
                {
                    Serial.print((char)buf[i]);
                }
                Serial.println("<3");
#endif
            } while (bufferReadLen > 0 && targetMaxLen > 0);
            if (0 == bufferReadLen)
            { // read complete log buffer, finished replacing pattern
                bufferSourceIndex = 0;
                patternFoundPosition = 0;
            }
            // fast exit, also avoid File.read() with a (remaining) buf size of 0
            if (targetMaxLen <= 0)
                return filledLen;
        }

        size_t readLen = _content.read(targetBuf, targetMaxLen);
        if (0 == targetMaxLen && (readLen > 0))
        {
            // more potential data, but _ack() has no space
            LOGBUFFER_DEBUGN("!! WARN: targetMaxLen=0, readLen=", readLen)
            _content.seek(fileReadPosition);
            return RESPONSE_TRY_AGAIN;
        }
        // we use *buf if AsyncWebResponse also for filtering
        if (readLen > 0)
        {
            // search for bufferPlaceHolder
            size_t bufSearchPosition = 0;
            while ((bufSearchPosition < readLen) && (patternFoundPosition < phPatternLen))
            {
                if ((phPattern[patternFoundPosition] == targetBuf[bufSearchPosition++]))
                {
                    ++patternFoundPosition;
                }
                else
                    patternFoundPosition = 0;
            }
            if ((patternFoundPosition > 0) && (bufSearchPosition >= readLen))
            {
                // special handling for possible found placeholder not completely read - introspect next possible bytes to possibly complete pattern
                const size_t remainingPatternLen = (phPatternLen - patternFoundPosition + 1);
                uint8_t tempBuf[remainingPatternLen];
                const size_t tempReadLen = _content.read(tempBuf, remainingPatternLen);
                readLen += tempReadLen;
                // specifically search for the remaining pattern, only to know if pattern is there
                size_t tempSearchPosition = 0;
                while ((tempSearchPosition < remainingPatternLen) && (patternFoundPosition < phPatternLen) && (phPattern[patternFoundPosition] == tempBuf[tempSearchPosition++]))
                    ++patternFoundPosition;
                if (patternFoundPosition < phPatternLen)
                { // read-ahead was not successful
                    _content.seek(fileReadPosition - tempReadLen);
                } // else: pattern was completely found, file position should be directly behind pattern
            }
            if (patternFoundPosition >= phPatternLen)
            {
                LOGBUFFER_DEBUGN("  webui: found pattern at pos=", bufSearchPosition)
                // we found complete pattern
                bufferSourceIndex = 0;
                // next time re-read too many bytes already read from file
                fileReadPosition = (fileReadPosition + readLen) - (readLen - bufSearchPosition); // TODO +1 , optimize expression ??
                _content.seek(fileReadPosition);                                                 // start next file delivery directly after pattern
                return filledLen + (bufSearchPosition - patternFoundPosition);                   // deliver the characters before the pattern
            }
            else
                return filledLen + readLen; // found no pattern, deliver complete chunk
        }
        else
        {
            // reached end of file
            LOGBUFFER_DEBUGN("end-of-file at deliveredBytes=", index)
            return 0;
        }
    }

    virtual ~FileWithLogBufferResponseDataSource()
    {
        if (_content)
            _content.close();
    }
};

#endif
