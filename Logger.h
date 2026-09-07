#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <Print.h>

class Logger {
public:
    virtual ~Logger() = default;

    virtual Print& logInfo() = 0;
    virtual Print& logWarn() = 0;
    virtual Print& logError() = 0;
    virtual Print& logDebug() = 0;
    virtual Print& logTrace() = 0;

    virtual void logError(const String msg);
    virtual void logError(const __FlashStringHelper *msg);
    virtual void logWarn(const String msg);
    virtual void logWarn(const __FlashStringHelper *msg);
    virtual void logInfo(const String msg);
    virtual void logInfo(const __FlashStringHelper *msg);
    virtual void logDebug(const String msg);
    virtual void logDebug(const __FlashStringHelper *msg);
};

#endif
