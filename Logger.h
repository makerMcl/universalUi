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
    LogLevel minimumLogLevel = LogLevel::DEBUG;

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

    Print& logNamed(LogLevel level, const __FlashStringHelper *loggerName = nullptr)
    {
        Print *output;
        switch (level)
        {
        case LogLevel::TRACE: output = &logTrace(); break;
        case LogLevel::DEBUG: output = &logDebug(); break;
        case LogLevel::INFO: output = &logInfo(); break;
        case LogLevel::WARN: output = &logWarn(); break;
        case LogLevel::ERROR: output = &logError(); break;
        default: output = &ignoredLog(); break;
        }
        if (loggerName)
        {
            output->print('[');
            output->print(loggerName);
            output->print(F("] "));
        }
        return *output;
    }

    virtual void logError(const String msg);
    virtual void logError(const __FlashStringHelper *msg);
    virtual void logWarn(const String msg);
    virtual void logWarn(const __FlashStringHelper *msg);
    virtual void logInfo(const String msg);
    virtual void logInfo(const __FlashStringHelper *msg);
    virtual void logDebug(const String msg);
    virtual void logDebug(const __FlashStringHelper *msg);
};

class LoggerView
{
public:
    LoggerView(Logger *logger = nullptr, const __FlashStringHelper *loggerName = nullptr)
        : _logger(logger), _loggerName(loggerName) {}

    LoggerView &operator=(Logger *logger)
    {
        _logger = logger;
        return *this;
    }

    explicit operator bool() const { return _logger != nullptr; }
    LoggerView *operator->() { return this; }
    const LoggerView *operator->() const { return this; }

    Print &logInfo() { return _logger->logNamed(LogLevel::INFO, _loggerName); }
    Print &logWarn() { return _logger->logNamed(LogLevel::WARN, _loggerName); }
    Print &logError() { return _logger->logNamed(LogLevel::ERROR, _loggerName); }
    Print &logDebug() { return _logger->logNamed(LogLevel::DEBUG, _loggerName); }
    Print &logTrace() { return _logger->logNamed(LogLevel::TRACE, _loggerName); }

    bool isLogInfoEnabled() const { return _logger && _logger->isLogInfoEnabled(); }
    bool isLogWarnEnabled() const { return _logger && _logger->isLogWarnEnabled(); }
    bool isLogErrorEnabled() const { return _logger && _logger->isLogErrorEnabled(); }
    bool isLogDebugEnabled() const { return _logger && _logger->isLogDebugEnabled(); }
    bool isLogTraceEnabled() const { return _logger && _logger->isLogTraceEnabled(); }

private:
    Logger *_logger;
    const __FlashStringHelper *_loggerName;
};

#endif
