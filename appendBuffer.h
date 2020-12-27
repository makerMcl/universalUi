#ifndef APPENDBUFFER_H
#define APPENDBUFFER_H
#include <Arduino.h>

/**
 * Buffer with limited length.
 * Overflowing characters are truncated.
 */
class AppendBuffer : public Print
{
public:
    // void printf(char *format, ...)
    // {
    //     va_list args;
    //     va_start(args, format);
    //     const int remains = getRemainingSize();
    //     const int written = snprintf(_appendPos, remains, format, args);
    //     _appendPos += (written < remains) ? written : remains;
    // }

    // TODO bug - prints invalid memory area
    /* Usage: <code>buf->sprintf_P(F("abc"), ...);</code> */
    void printf(const __FlashStringHelper *pgmFormat...)
    {
        va_list args;
        // va_start(args, pgmFormat);
        const int remains = getRemainingSize();
        const char *pFormat = (char *)pgmFormat;
        const int written = snprintf_P(_appendPos, remains, pFormat, args);
        _appendPos += (written < remains) ? written : remains;
    }

    /** Append string to this buffer */
    void append(const String &s)
    {
        write(s.c_str());
    }
    size_t write(const char *str)
    {
        size_t maxLength = getRemainingSize();
        size_t written = 0;
        while (('\0' != *str) && (maxLength > 1))
        {
            *_appendPos++ = *str++;
            --maxLength;
            ++written;
        }
        *_appendPos = '\0';
        return written;
    }

    /** Append string from flash/program memory to this buffer */
    void append_P(const __FlashStringHelper *pgmstr)
    {
        // _appendPos = appendstr_P(_appendPos, pgmstr, _maxsize - _appendPos + _buf);
        const char *pstr = (char *)pgmstr;
        size_t maxLength = getRemainingSize();
        char b;
        while (('\0' != (b = pgm_read_byte(pstr++))) && (maxLength > 1))
        {
            *_appendPos++ = b;
            --maxLength;
        }
        *_appendPos = '\0';
    }

    virtual size_t write(uint8_t c)
    {
        if (getRemainingSize() > 1)
        {
            *_appendPos++ = c;
            *_appendPos = '\0';
            return 1;
        }
        else
            return 0;
    }

    /** Reset buffer to empty string */
    void reset()
    {
        _appendPos = _buf;
        *_buf = '\0';
    }

    char *c_str()
    {
        return _buf;
    }

    AppendBuffer(size_t size) : _maxsize(size)
    {
        _buf = (char *)malloc(size);
        _appendPos = _buf;
    }


private:
    size_t _maxsize;  // number of characters, including the trailing '\0'
    char *_appendPos; // position in buffer where next character to place at
    char *_buf;

    size_t getRemainingSize() { return (_maxsize - (size_t)_appendPos + (size_t)_buf); }
};
#endif
