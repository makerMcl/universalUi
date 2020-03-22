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
    char clippedMarker[7] = "[...] ";
    char buf[LOGBUF_LENGTH + 1];
    word appendIndex = 0; // where to append next logged character
    bool clipped = false;
    void bufEnd();
    /** Increment given argument by one with handling rollover, returning the original value (next index to write to). */
    const word incWithRollover(word &idx);

public:
    LogBuffer();
    virtual size_t write(uint8_t c);
    size_t write(const char *str);
    /** Get the log buffer content.
     * Must be called twice, first with argument <code>0</code>, 2nd with argument <code>1</code>.
     */
    const char *getLog(const byte part);
    /** Reset the log buffer to initial = empty state. */
    void clear();
};
#endif
