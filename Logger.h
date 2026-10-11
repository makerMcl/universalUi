#ifndef LOGGER_H
#define LOGGER_H

#include <Arduino.h>
#include <Print.h>

enum class LogLevel : uint8_t { TRACE, DEBUG, INFO, WARN, ERROR };

class Logger {
private:
    class DiscardPrint : public Print {
    public:
        size_t write(uint8_t) override { return 1; }
        size_t write(const uint8_t *, size_t size) override { return size; }
    } discardedLog;
    LogLevel minimumLogLevel = LogLevel::TRACE;

protected:
    bool shouldLog(LogLevel level) const { return level >= minimumLogLevel; }
    Print& ignoredLog() { return discardedLog; }

public:
    virtual ~Logger() = default;

    void setLogLevel(LogLevel level) { minimumLogLevel = level; }
    LogLevel getLogLevel() const { return minimumLogLevel; }
    bool isLogTraceEnabled() const { return shouldLog(LogLevel::TRACE); }
    bool isLogDebugEnabled() const { return shouldLog(LogLevel::DEBUG); }
    bool isLogInfoEnabled() const { return shouldLog(LogLevel::INFO); }
    bool isLogWarnEnabled() const { return shouldLog(LogLevel::WARN); }
    bool isLogErrorEnabled() const { return shouldLog(LogLevel::ERROR); }

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
