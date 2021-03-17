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

#include <AsyncWebSynchronization.h>
#include "universalUIsettings.h"
#include "debuglog.h"

#if defined(ESP32)
#define MUTEX_YIELD yield()
#elif defined(ESP8266)
#define MUTEX_YIELD esp_yield()
#else
#define MUTEX_YIELD ;
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
class LogBuffer : public Print
{
#ifndef LOGBUF_LENGTH    // expected to be set in main sketch
#define LOGBUF_LENGTH 16 // default size, for testing
#endif
private:
    char clippedMarker[7] = {'[', '.', '.', '.', ']', ' ', '\0'};
    char buf[LOGBUF_LENGTH + 1];
    size_t appendIndex = 0; // where to append next logged character
    bool clipped = false;
    bool _encodePercent;
    AsyncWebLock xMutex = AsyncWebLock(); // need this since arduino loop and webserver run on different cores/threads
    void bufEnd()
    {
        size_t p = appendIndex;
        buf[incWithRollover(p)] = '\0';
        if (clipped)
        {
            for (int i = 0; clippedMarker[i] != '\0'; ++i)
            {
                buf[incWithRollover(p)] = clippedMarker[i];
            }
        }
    }

    /** Increment given argument by one with handling rollover, returning the original value (next index to write to). */
    const word incWithRollover(size_t &idx)
    {
        const word x = idx;
        if (++idx >= LOGBUF_LENGTH)
        {
            idx = 0;
            clipped = true;
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
        LOGBUFFER_DEBUGN(", bufStartOfs=", (sourceBuf - &buf[0]))
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
     * Note: supports fix for https://github.com/me-no-dev/ESPAsyncWebServer/issues/333: '%' in template result is evaluated as template again
     * @param encodePercent true if '%' in content should be stored as "%%"
     */
    LogBuffer(const bool encodePercent = false) : _encodePercent(encodePercent)
    {
        buf[LOGBUF_LENGTH] = '\0';
        buf[0] = '\0';
    }

    virtual size_t write(uint8_t c)
    {
#ifdef COPY_TO_SERIAL
        Serial.print((char)c);
#endif
        while (xMutex.lock())
            yield(); // we expect to be in the arduino thread here
        buf[incWithRollover(appendIndex)] = c;
        if (_encodePercent && ('%' == c))
        {
            buf[incWithRollover(appendIndex)] = c;
        }
        bufEnd();
        xMutex.unlock();
        return 1;
    }

    size_t write(const char *msg)
    {
#ifdef COPY_TO_SERIAL
        Serial.print(msg);
#endif
        word i = 0;
        while (xMutex.lock())
            yield(); // we expect to be in the arduino thread here
        while (msg[i] != '\0')
        {
            buf[incWithRollover(appendIndex)] = msg[i];
            if (_encodePercent && ('%' == msg[i]))
            {
                buf[incWithRollover(appendIndex)] = msg[i];
            }
            ++i;
        }
        bufEnd();
        xMutex.unlock();
        return i;
    }

    /** Reset the log buffer to initial = empty state. */
    void clear()
    {
        while (xMutex.lock())
            yield();
        clipped = false;
        appendIndex = 0;
        buf[LOGBUF_LENGTH] = '\0';
        buf[0] = '\0';
        xMutex.unlock();
    }

    /** Get the log buffer content.
     * Must be called twice, first with argument <code>0</code>, 2nd with argument <code>1</code>.
     */
    const char *getLog(const byte part) const
    {
        const char *result;
        while (xMutex.lock())
            MUTEX_YIELD; // note: we are not in the arduino thread here
        if (0 == part)
        {
            result = clipped ? &buf[appendIndex + 1] : &buf[0];
        }
        else if (1 == part && clipped)
        {
            result = &buf[0];
        }
        else
        {
            result = &clippedMarker[strlen(clippedMarker)]; // empty string
        }
        xMutex.unlock();
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
        while (xMutex.lock())
            MUTEX_YIELD; // note: we are not in the arduino thread here
        if (0 == index || !clipped)
        {
            bufferRollIndex = appendIndex;
            LOGBUFFER_DEBUG("initialized bufferRollIndex=", bufferRollIndex)
            LOGBUFFER_DEBUGN(" appendIndex=", appendIndex);
        }
        if (clipped)
        {
            if (index >= LOGBUF_LENGTH)
            {
                // buffer completely read
                result = 0;
            }
            else if ((index + 1) < (LOGBUF_LENGTH - appendIndex))
            {
                // part 0: appendIndex+1 .. LOGBUF_LENGTH;  length = (LOGBUF_LENGTH-appendIndex-1)
                LOGBUFFER_DEBUG("    part 0: maxLen=", maxLen)
                LOGBUFFER_DEBUG(", index=", index)
                LOGBUFFER_DEBUGN(", bufferRollIndex=", bufferRollIndex)
                result = copyLog(targetBuf, maxLen, index, &buf[bufferRollIndex + 1], (LOGBUF_LENGTH - bufferRollIndex - 1));
            }
            else
            {
                LOGBUFFER_DEBUG("    part 1: maxLen=", maxLen)
                LOGBUFFER_DEBUG(", index=", index)
                LOGBUFFER_DEBUGN(", bufferRollIndex=", bufferRollIndex);
                // part 1: 0 .. appendIndex-1;  length = appendIndex
                result = copyLog(targetBuf, maxLen, index - (LOGBUF_LENGTH - bufferRollIndex - 1), &buf[0], bufferRollIndex);
            }
        }
        else
        {
            if (index < appendIndex)
            {
                LOGBUFFER_DEBUG("    logBuffer: all maxLen=", maxLen)
                LOGBUFFER_DEBUG(", index=", index)
                LOGBUFFER_DEBUGN(", bufferRollIndex=", bufferRollIndex)
                // result = ((appendIndex - index) < maxLen) ? (appendIndex - index) : maxLen;
                // memcpy(targetBuf, &buf[index], result);
                result = copyLog(targetBuf, maxLen, index, &buf[0], bufferRollIndex);
            }
            else
                result = 0;
        }
        xMutex.unlock();
        return result;
    }
};
#endif
