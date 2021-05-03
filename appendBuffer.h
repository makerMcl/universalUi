#ifndef APPENDBUFFER_H
#define APPENDBUFFER_H
#include <Arduino.h>

// need this (at least onesp32) since arduino loop and webserver might run on different cores/threads
#if defined(ESP32)
#define MUTEX_LOCK portENTER_CRITICAL(&logBuffer_mutex);
#define MUTEX_UNLOCK portEXIT_CRITICAL(&logBuffer_mutex);
static portMUX_TYPE logBuffer_mutex = portMUX_INITIALIZER_UNLOCKED;
#else
#define MUTEX_LOCK noInterrupts(); // we must not implement waiting for a mutex here since in ISR wie can't wait!
#define MUTEX_UNLOCK interrupts(); // we can only disable interrupts for the critical section of updating the buffer
#endif

/**
 * Buffer with limited length.
 * Overflowing characters are truncated.
 */
class AppendBuffer : public Print
{
public:
    /* Usage: <code>buf->sprintf_P(PSTR("abc"), ...);</code> */
    void printf_P(const char *pstrFormat...)
    {
        va_list args;
        va_start(args, pstrFormat);
        vprintf_P(pstrFormat, args);
        va_end(args);
    }
    void vprintf_P(const char *pstrFormat, va_list args)
    {
        const int remains = getCapacityLeft();
        const int written = vsnprintf_P(_appendPos, remains, pstrFormat, args);
        _appendPos += (written < remains) ? written : remains;
    }
    /** Convenience function - shortcut for: <code>abuf.reset(); abuf.printf_P(); return abuf.c_str();</code> */
    const char *format(const char *pstrFormat...)
    {
        reset();
        va_list args;
        va_start(args, pstrFormat);
        vprintf_P(pstrFormat, args);
        va_end(args);
        return c_str();
    }

    /** Append string to this buffer */
    void append(const String &s)
    {
        write(s.c_str());
    }
    size_t write(const char *str)
    {
        MUTEX_LOCK;
        size_t maxLength = getCapacityLeft();
        size_t written = 0;
        while (('\0' != *str) && (maxLength > 1))
        {
            *_appendPos++ = *str++;
            --maxLength;
            ++written;
        }
        *_appendPos = '\0';
        MUTEX_UNLOCK;
        return written;
    }

    /** Append string from flash/program memory to this buffer */
    void append_P(const __FlashStringHelper *pgmstr)
    {
        MUTEX_LOCK;
        // _appendPos = appendstr_P(_appendPos, pgmstr, _maxsize - _appendPos + _buf);
        const char *pstr = (char *)pgmstr;
        size_t maxLength = getCapacityLeft();
        char b;
        while (('\0' != (b = pgm_read_byte(pstr++))) && (maxLength > 1))
        {
            *_appendPos++ = b;
            --maxLength;
        }
        *_appendPos = '\0';
        MUTEX_UNLOCK;
    }

    virtual size_t write(uint8_t c)
    {
        MUTEX_LOCK;
        if (getCapacityLeft() > 1)
        {
            *_appendPos++ = c;
            *_appendPos = '\0';
            MUTEX_UNLOCK;
            return 1;
        }
        else
        {
            MUTEX_UNLOCK;
            return 0;
        }
    }

    /** Reset buffer to empty string */
    void reset()
    {
        MUTEX_LOCK;
        _appendPos = _buf;
        *_buf = '\0';
        MUTEX_UNLOCK;
    }

    char *c_str()
    {
        return _buf;
    }

    /**
     * @return number of bytes contained
     */
    size_t size()
    {
        MUTEX_LOCK;
        const size_t result = (_appendPos - _buf);
        MUTEX_UNLOCK;
        return result;
    }

    /**
     * Create an instance with externally supplied memory for buffer.
     * This allows to use statically allocated memory to be recognized at linking time.
     */
    AppendBuffer(const size_t size, char *buf) : _maxsize(size), _buf(buf)
    {
        _appendPos = _buf;
    }

    /**
     * Use this constructor at your own risk: the linker won't provide an error if not enough memory available!
     */
    AppendBuffer(size_t size) : _maxsize(size), _buf(new char[_maxsize])
    {
        _appendPos = _buf;
    }
    ~AppendBuffer()
    {
        delete _buf;
    }

private:
    size_t _maxsize; // number of characters, including the trailing '\0'
    char *_buf;
    char *_appendPos; // position in buffer where next character to place at

    /** 
     * Not synchronized!
     * @return number of bytes that can be written at mosted
     */
    size_t getCapacityLeft() { return (_maxsize - (size_t)_appendPos + (size_t)_buf); }
};
#endif
