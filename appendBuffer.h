#ifndef APPENDBUFFER_H
#define APPENDBUFFER_H
#include <Arduino.h>
#include <Stream.h>

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
        const size_t remains = getCapacityLeft();
        if (remains == 0)
            return;
        const int written = vsnprintf_P(_appendPos, remains, pstrFormat, args);
        if (written > 0)
            _appendPos += min(static_cast<size_t>(written), remains - 1);
        *_appendPos = '\0';
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
        if (_maxsize == 0 || !str)
            return 0;
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
        if (_maxsize == 0 || !pgmstr)
            return;
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
        if (_maxsize > 0)
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
        const size_t result = _buf ? (_appendPos - _buf) : 0;
        MUTEX_UNLOCK;
        return result;
    }

    /**
     * Create an instance with externally supplied memory for buffer.
     * This allows to use statically allocated memory to be recognized at linking time.
     */
    AppendBuffer(const size_t size, char *buf) : _maxsize(buf ? size : 0), _buf(buf), _ownsBuffer(false)
    {
        reset();
    }

    /**
     * Use this constructor at your own risk: the linker won't provide an error if not enough memory available!
     */
    AppendBuffer(size_t size) : _maxsize(size > 0 ? size : 1), _buf(new char[_maxsize]), _ownsBuffer(true)
    {
        reset();
    }
    AppendBuffer(const AppendBuffer &) = delete;
    AppendBuffer &operator=(const AppendBuffer &) = delete;
    AppendBuffer &operator=(AppendBuffer &&) = delete;
    AppendBuffer(AppendBuffer &&other)
        : _maxsize(other._maxsize), _buf(other._buf), _appendPos(other._appendPos), _ownsBuffer(other._ownsBuffer)
    {
        other._maxsize = 0;
        other._buf = nullptr;
        other._appendPos = nullptr;
        other._ownsBuffer = false;
    }
    ~AppendBuffer()
    {
        if (_ownsBuffer)
            delete[] _buf;
    }

private:
    size_t _maxsize; // number of characters, including the trailing '\0'
    char *_buf;
    char *_appendPos; // position in buffer where next character to place at
    bool _ownsBuffer;

    /** 
     * Not synchronized!
     * @return number of bytes that can be written at mosted
     */
    size_t getCapacityLeft() { return _buf ? _maxsize - static_cast<size_t>(_appendPos - _buf) : 0; }
};

class MemoryResponseStream : public Stream
{
public:
    MemoryResponseStream(const char *data, size_t length)
        : data(reinterpret_cast<const uint8_t *>(data)), length(length), position(0) {}

    int available() override { return static_cast<int>(length - position); }
    int read() override { return position < length ? data[position++] : -1; }
    int peek() override { return position < length ? data[position] : -1; }

    int read(uint8_t *buffer, size_t size) override
    {
        if (!buffer || size == 0 || position >= length)
            return 0;
        const size_t count = min(size, length - position);
        memcpy(buffer, data + position, count);
        position += count;
        return static_cast<int>(count);
    }

    size_t write(uint8_t) override { return 0; }

private:
    const uint8_t *data;
    size_t length;
    size_t position;
};
#endif
