#include <Arduino.h>
#include "universalUI.h"

#define USE_NTP
#ifdef USE_NTP
#include <NTPClient.h>
#include <WiFiUdp.h>
#endif

#define PIN_LED_STATUS 2

// global ui instance
UniversalUI ui = UniversalUI("test");

#ifdef USE_NTP
// NTP time
WiFiUDP ntpUDP;
NTPClient ntpClient = NTPClient(ntpUDP, "europe.pool.ntp.org");
#endif

void setup()
{
#ifdef USE_NTP
    ui.setNtpClient(&ntpClient);
#endif
    ui.init(PIN_LED_STATUS, __FILE__, __TIMESTAMP__);
    ui.setBlink(50, 950);
}

void loop()
{
    if (ui.handle())
    {
        // put your main code here, to run repeatedly:
    }
}