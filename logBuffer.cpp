/*
LogBuffer - circular rolling buffer for storing log.

Copyright (C) 2020  Matthias Clauß

This program is free software; you can redistribute it and/or modify it under the terms of the GNU General Public License as published by the Free Software Foundation; either version 3 of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with this program; if not, see <http://www.gnu.org/licenses/>.
*/
#include "logBuffer.h"

/* 
 * implementation of LogBuffer
 */
const char *LogBuffer::getLog(const byte part)
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

const word LogBuffer::incWithRollover(word &idx)
{
    const word x = idx;
    if (++idx >= LOGBUF_LENGTH)
    {
        idx = 0;
        clipped = true;
    }
    return x;
}
void LogBuffer::bufEnd()
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

LogBuffer::LogBuffer(const bool encodePercent) : _encodePercent(encodePercent)
{
    buf[LOGBUF_LENGTH] = '\0';
    buf[0] = '\0';
}

void LogBuffer::clear()
{
    clipped = false;
    appendIndex = 0;
    buf[LOGBUF_LENGTH] = '\0';
    buf[0] = '\0';
}

size_t LogBuffer::write(const char *msg)
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
size_t LogBuffer::write(uint8_t c)
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
