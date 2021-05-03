/*
LogBuffer - circular rolling buffer for storing log.

Copyright (C) 2020  Matthias Clauß

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#include <Arduino.h>

#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include "universalUIsettings.h"

#ifdef VERBOSE_DEBUG_LOGBUFFER
#define LOGBUFFER_DEBUG(M, V) (Serial << M << V);
#define LOGBUFFER_DEBUGN(M, V) (Serial << M << V << endl);
#else
#define LOGBUFFER_DEBUG(M, V) ;
#define LOGBUFFER_DEBUGN(M, V) ;
#endif

/**
 * Collects all data into buf.
 * 
 * Implemented as circular ring buffer:
 * if buf size is reached, first data logged will be overwritten, preceded by "[...]".
 * 
 * To determine max size, use the output of the compiler (memory used).
 * If not fitting, you will get errors like "section ... will not fit in region ..." from the linker.
 * TOCHECK: beware that some memory areas has special purpose, hopefully the compiler will warn?
 * 
 * define-Parameters:
 * <li><code>#define LOGBUF_LENGTH 51200</code> - default is 16 characters (for testing purpose)</li>
 * <li><code>COPY_TO_SERIAL</code> - if defined, logged data will be mirrored via Serial.print</li>
 */

// need this (at least on ESP32) since arduino loop and webserver might run on different cores/threads
#if defined(ESP32)
#define MUTEX_LOCK portENTER_CRITICAL(&logBuffer_mutex);
#define MUTEX_UNLOCK portEXIT_CRITICAL(&logBuffer_mutex);
static portMUX_TYPE logBuffer_mutex = portMUX_INITIALIZER_UNLOCKED;
#else
#define MUTEX_LOCK noInterrupts(); // we must not implement waiting for a mutex here since in ISR wie can't wait!
#define MUTEX_UNLOCK interrupts(); // we can only disable interrupts for the critical section of updating the buffer
#define RESPONSE_TRY_AGAIN 0xFFFF // is defined by AsyncWebServer
#endif

class LogBuffer : public Print
{
private:
    size_t _bufSize;
    bool _encodePercent;
    char *_buffer;
    size_t _appendIndex = 0; // where to append next logged character
    bool _clipped = false;
    char clippedMarker[7] = {'[', '.', '.', '.', ']', ' ', '\0'};
    void bufEnd()
    {
        size_t p = _appendIndex;
        _buffer[incWithRollover(p)] = '\0';
        if (_clipped)
        {
            for (int i = 0; clippedMarker[i] != '\0'; ++i)
            {
                _buffer[incWithRollover(p)] = clippedMarker[i];
            }
        }
    }

    /** Increment given argument by one with handling rollover, returning the original value (next index to write to). */
    const word incWithRollover(size_t &idx)
    {
        const word x = idx;
        if (++idx >= _bufSize)
        {
            idx = 0;
            _clipped = true;
        }
        return x;
    }

    /** Copies content from sourceBuf[startIndex] to targetBuf, taking care of available data lengths */
    size_t copyLog(uint8_t *targetBuf, size_t maxTargetLen, size_t startIndex, char *sourceBuf, size_t availableLogLen)
    {
        if (0 == maxTargetLen && availableLogLen > 0)
        {
            LOGBUFFER_DEBUGN("      logBuffer.copy: try again, availableLogLen=", availableLogLen)
            return RESPONSE_TRY_AGAIN;
        }
        const size_t copyLen = ((availableLogLen - startIndex) < maxTargetLen) ? (availableLogLen - startIndex) : maxTargetLen;
        LOGBUFFER_DEBUG("      logBuffer.copy: copyLen=", copyLen)
        LOGBUFFER_DEBUG(", startIndex=", startIndex)
        LOGBUFFER_DEBUGN(", bufStartOfs=", (sourceBuf - &_buffer[0]))
#ifdef VERBOSE_DEBUG
        if (0 == startIndex)
        {
            Serial.print("      logBuffer.copy: 1>");
            for (size_t i = 0; i < copyLen; ++i)
            {
                Serial.print((char)sourceBuf[startIndex + i]);
            }
            Serial.println("<1");
        }
#endif
        memcpy(targetBuf, &sourceBuf[startIndex], copyLen);

#ifdef VERBOSE_DEBUG
        if (0 == startIndex)
        {
            Serial.print("      logBuffer.copy: 2>");
            for (size_t i = 0; i < copyLen; ++i)
            {
                Serial.print((char)targetBuf[i]);
            }
            Serial.println("<2 ");
        }
#endif
        return copyLen;
    }

public:
    /**
     * Constructor with externally supplied memory.
     * If statically allocated memory is provided here, the linker will help you estimating managing available memory.
     */
    LogBuffer(const size_t capacity, char *buffer, const bool encodePercent = false) : _bufSize(capacity - 1), _encodePercent(encodePercent), _buffer(buffer)
    {
        _buffer[_bufSize] = '\0';
        _buffer[0] = '\0';
    }

    /**
     * Use this constructor at your own risk: it uses dynamically allocated memory.
     * If not enough memory is available, ESP8266 will reboot with `rst cause:1, boot mode:(3,0)`.
     *  
     * Note: supports fix for https://github.com/me-no-dev/ESPAsyncWebServer/issues/333: '%' in template result is evaluated as template again
     * @param encodePercent true if '%' in content should be stored as "%%"
     */
    LogBuffer(const size_t capacity, const bool encodePercent = false) : _bufSize(capacity), _encodePercent(encodePercent), _buffer(new char[_bufSize + 1])
    {
        _buffer[_bufSize] = '\0';
        _buffer[0] = '\0';
    }
    ~LogBuffer()
    {
        delete _buffer;
    }

    virtual size_t write(uint8_t c)
    {
#ifdef COPY_TO_SERIAL
        Serial.print((char)c);
#endif
        MUTEX_LOCK;
        _buffer[incWithRollover(_appendIndex)] = c;
        if (_encodePercent && ('%' == c))
        {
            _buffer[incWithRollover(_appendIndex)] = c;
        }
        MUTEX_UNLOCK;
        return 1;
    }

    size_t write(const char *msg)
    {
#ifdef COPY_TO_SERIAL
        Serial.print(msg);
#endif
        word i = 0;
        MUTEX_LOCK;
        while (msg[i] != '\0')
        {
            _buffer[incWithRollover(_appendIndex)] = msg[i];
            if (_encodePercent && ('%' == msg[i]))
            {
                _buffer[incWithRollover(_appendIndex)] = msg[i];
            }
            ++i;
        }
        bufEnd();
        MUTEX_UNLOCK;
        return i;
    }

    /** Reset the log buffer to initial = empty state. */
    void clear()
    {
        MUTEX_LOCK;
        _clipped = false;
        _appendIndex = 0;
        _buffer[_bufSize] = '\0';
        _buffer[0] = '\0';
        MUTEX_UNLOCK;
    }

    /** Get the log buffer content.
     * Must be called twice, first with argument <code>0</code>, 2nd with argument <code>1</code>.
     */
    const char *getLog(const byte part) const
    {
        const char *result;
        MUTEX_LOCK;
        ; // note: we are not in the arduino thread here
        if (0 == part)
        {
            result = _clipped ? &_buffer[_appendIndex + 1] : &_buffer[0];
        }
        else if (1 == part && _clipped)
        {
            result = &_buffer[0];
        }
        else
        {
            result = &clippedMarker[strlen(clippedMarker)]; // empty string
        }
        MUTEX_UNLOCK;
        return result;
    }

    /**
     * Fills the given buffer with data from the log buffer content.
     * This method also takes care of rolling buffer overflow.
     * 
     * TODO If buffer rollover-position has advanced over index since last read, the new content is filled, that could lead to scrambled result?
     * 
     * @param targetBuf buffer to fill with data
     * @param maxLen maximum number of bytes to fill into buf
     * @param index logical start position of log buffer
     * @param bufferRollIndex required request-specific state-memory, is initialized at first call (index==0)
     * @return number of bytes filled into buf, or 0 if there is no more data available, or RESPONSE_TRY_AGAIN if maxLen is 0 and more content available
     */
    size_t getLog(uint8_t *targetBuf, size_t maxLen, size_t index, size_t &bufferRollIndex)
    {
        size_t result;
        MUTEX_LOCK; // note: we are not in the arduino thread here
        if (0 == index || !_clipped)
        {
            bufferRollIndex = _appendIndex;
            LOGBUFFER_DEBUG("initialized bufferRollIndex=", bufferRollIndex)
            LOGBUFFER_DEBUGN(" appendIndex=", _appendIndex);
        }
        if (_clipped)
        {
            if (index >= _bufSize)
            {
                // buffer completely read
                result = 0;
            }
            else if ((index + 1) < (_bufSize - _appendIndex))
            {
                // part 0: appendIndex+1 .. LOGBUF_LENGTH;  length = (LOGBUF_LENGTH-appendIndex-1)
                LOGBUFFER_DEBUG("    part 0: maxLen=", maxLen)
                LOGBUFFER_DEBUG(", index=", index)
                LOGBUFFER_DEBUGN(", bufferRollIndex=", bufferRollIndex)
                result = copyLog(targetBuf, maxLen, index, &_buffer[bufferRollIndex + 1], (_bufSize - bufferRollIndex - 1));
            }
            else
            {
                LOGBUFFER_DEBUG("    part 1: maxLen=", maxLen)
                LOGBUFFER_DEBUG(", index=", index)
                LOGBUFFER_DEBUGN(", bufferRollIndex=", bufferRollIndex);
                // part 1: 0 .. appendIndex-1;  length = appendIndex
                result = copyLog(targetBuf, maxLen, index - (_bufSize - bufferRollIndex - 1), &_buffer[0], bufferRollIndex);
            }
        }
        else
        {
            if (index < _appendIndex)
            {
                LOGBUFFER_DEBUG("    logBuffer: all maxLen=", maxLen)
                LOGBUFFER_DEBUG(", index=", index)
                LOGBUFFER_DEBUGN(", bufferRollIndex=", bufferRollIndex)
                // result = ((appendIndex - index) < maxLen) ? (appendIndex - index) : maxLen;
                // memcpy(targetBuf, &buf[index], result);
                result = copyLog(targetBuf, maxLen, index, &_buffer[0], bufferRollIndex);
            }
            else
                result = 0;
        }
        MUTEX_UNLOCK;
        return result;
    }
};
#endif
