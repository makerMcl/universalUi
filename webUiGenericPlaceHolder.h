#ifndef WEBUI_GENERIC_PLACEHOLDER_H
#define WEBUI_GENERIC_PLACEHOLDER_H
#include "universalUIglobal.h"

String universalUiPlaceholderProcessor(const String &var, AppendBuffer &buf)
{
    if (0 == strcmp_P(var.c_str(), PSTR("__TIMESTAMP__")))
        return F(__TIMESTAMP__);
    if (0 == strcmp_P(var.c_str(), PSTR("STATUS")))
        return ui.getStatusMessage();
    if (0 == strcmp_P(var.c_str(), PSTR("STATUSBAR")))
    {
        if (ui.hasStatusMessage())
        {
            buf.reset();
            buf.sprintf_P(F("<p style=\"color:blue;background-color:lightgrey;text-align:center;\">Status: %s</p>"), ui.getStatusMessage());
            return buf.c_str();
        }
        else
        {
            return "";
        }
    }
    if (0 == strcmp_P(var.c_str(), PSTR("RESET_REASON")))
    {
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
    }
    if (0 == strcmp_P(var.c_str(), PSTR("SYSTIME")))
    {
        buf.reset();
        if (ui.isNtpTimeValid())
        {
            buf.sprintf(F("%lu @ %s"), millis(), ui.getFormattedTime().c_str());
            return buf.c_str();
        }
        else
        {
            buf.sprintf(F("%lu ms"), millis());
            return buf.c_str();
        }
    }
    if (0 == strcmp_P(var.c_str(), PSTR("USERMESSAGE")))
    {
        if (ui.hasUiError())
        {
            buf.reset();
            buf.sprintf_P(F("<h3 style='color:red;'>%s</h3>"), &ui.getUiErrorMessage()[0]);
            return buf.c_str();
        }
        else
            return "";
    }
    if (0 == strcmp_P(var.c_str(), PSTR("LOG0")))
        return ui.getHtmlLog(0);
    if (0 == strcmp_P(var.c_str(), PSTR("LOG1")))
        return ui.getHtmlLog(1);
    ui.logError() << F("DEBUG: variable not found: ") << var << endl;
    return F("???");
}

const String PARAM_REFRESH = "r";

class RefreshState
{
private:
    byte _webuiRefresh = 0; // time interval in [seconds] for page refresh
    bool _webuiRefreshEnabled = true;

public:
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
            buf.sprintf(F("<meta http-equiv=\"refresh\" content=\"%d;url=%s?r=%d#refresh\">"), _webuiRefresh, uri.c_str(), _webuiRefresh);
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
        buf.sprintf_P(F("<a href=\"%s?r=%d\">"), uri.c_str(), (_webuiRefreshEnabled && _webuiRefresh > 0) ? 0 : 1);
        if (_webuiRefreshEnabled && _webuiRefresh > 0)
            buf.append_P(F("Stop"));
        else
            buf.append_P(F("Start"));
        buf.append_P(F(" Refresh</a>"));
        return buf.c_str();
    }
};

#endif
