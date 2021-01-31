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
    word appendIndex = 0; // where to append next logged character
    bool clipped = false;
    bool _encodePercent;
    void bufEnd()
    {
        word p = appendIndex;
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
    const word incWithRollover(word &idx)
    {
        const word x = idx;
        if (++idx >= LOGBUF_LENGTH)
        {
            idx = 0;
            clipped = true;
        }
        return x;
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
        buf[incWithRollover(appendIndex)] = c;
        if (_encodePercent && ('%' == c))
        {
            buf[incWithRollover(appendIndex)] = c;
        }
        bufEnd();
        return 1;
    }

    size_t write(const char *msg)
    {
#ifdef COPY_TO_SERIAL
        Serial.print(msg);
#endif
        word i = 0;
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
        return i;
    }
    /** Get the log buffer content.
     * Must be called twice, first with argument <code>0</code>, 2nd with argument <code>1</code>.
     */
    const char *getLog(const byte part)
    {
        if (0 == part)
        {
            return clipped ? &buf[appendIndex + 1] : &buf[0];
        }
        else if (1 == part && clipped)
        {
            return &buf[0];
        }
        return &clippedMarker[strlen(clippedMarker)]; // empty string
    }
    /** Reset the log buffer to initial = empty state. */
    void clear()
    {
        clipped = false;
        appendIndex = 0;
        buf[LOGBUF_LENGTH] = '\0';
        buf[0] = '\0';
    }
};
#endif
