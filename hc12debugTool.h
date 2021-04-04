/**
 * This class serves as utility to manage a HC-12 module on UART0 as remote debugging connector.
 * 
 * Simply connect the HC12 with your Txd0 and RxD0 pins, that is your default UART Arduino addresses with the Serial object:<ul>
 * <li> Txd0 of Arduino module  ==>  Rx pin of HC-12 module
 * <li> Rxd0 of Arduino module  ==>  Tx pin of HC-12 module
 * <li> 3.3V of Arduino module  ==> Vcc of HC-12 module
 * </ul>
 * 
 * That given any output/input on Serial is also transmitted/received via the HC-12 module and can be used to monitor the module from a remote location.
 * 
 * This library supports that approach by:<ul>
 * <li>determine of the set pin of HC-12 module is connected, resulting in hc-12 command mode available
 * <li>if available and different, configure the HC-12 module to the preferred baud rate
 * <li>if hc-12 command mode is not available, setting the baud rate on Serial to the default 9600 baud of the HC-12
 * <ul>
 * 
 * A call to setPreferredBaudrate() will produce a single line of output on serial. * 
 * If hc-12 command mode is available, the remote module will receive the text:
 * <pre>Configuring HC-12: baudrate set to &lt;n&gt;</pre>
 * If not available, the remote module will receive the text:
 * <pre>Configuring HC-12: AT -> command mode not available</pre>
 * 
 * Optionally, sets more options of the HC-12 module:
 * <li>channel
 * <li>PA output
 * 
 * 
 * Typical use (use heap instance since only needed in setup) might be:<pre>
 * #include "universalUIglobal.h"
 * void setup() {
 *   Hc12DebugTool hc12DebugTool(5);
 *   hc12DebugTool.setPreferredBaudrate(BPS115200);
 *   //...
 * }
 * </pre>
 * 
 * TODO/potential for more:
 * <li>support more configuration details, e.g. transmission power (AT+P), channel (AT+C), UART transmission mode (AT+FU)
 */
#ifndef _HC12_DEBUG_TOOL_H
#define _HC12_DEBUG_TOOL_H
#include <Arduino.h>

#ifdef VERBOSE_DEBUG_HC12TOOL
#define HC12TOOL_DEBUG(X) Serial.print(X);
#else
#define HC12TOOL_DEBUG(X) ;
#endif //of: ifdef DEBUG_HC12TOOL

// define function to use for setting the baudrate
#if defined(ESP32) || defined(ESP8266)
#define SETBAUDRATE updateBaudRate
#else
#define SETBAUDRATE begin
#endif

// available baud rates of HC-12 module: 1200bps, 2400bps, 4800bps, 9600bps, 19200bps, 38400bps, 57600bps and 115200bps
enum Hc12_BaudRate
{
    BPS1200 = 0,
    BPS2400 = 1,
    BPS4800 = 2,
    BPS9600 = 3,
    BPS19200 = 4,
    BPS38400 = 5,
    BPS57600 = 6,
    BPS115200 = 7
};

// ordering must adhere to order of enum
#define NUM_HC12_BAUDRATES 8
const char *const HC12_BAUDRATE_STRING[NUM_HC12_BAUDRATES] /*PROGMEM*/ = {"1200", "2400", "4800", "9600", "19200", "38400", "57600", "115200"};
const unsigned long HC12_BAUDRATE_NUMERIC[NUM_HC12_BAUDRATES] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};

// TODO convert to use it from progmem/flash; did not get it working yet
const char *const COMMAND_AT /*PROGMEM*/ = "AT\r\n";  // AT-command must be line-terminated to indicate end-of-command
const char *const RESPONSE_AT /*PROGMEM*/ = "OK\r\n"; // HC-12 terminates his responses always with \r\n (<13>,<10>)

template <typename S>
class Hc12DebugTool
{
public:
    /**
     * Create instance of this class, configured with GPIO connected to set pin.
     * This pin will be configured for output at `setPreferredBaudrate()`.
     * If pin number is 0, calls to setPreferredBaudrate has no effect at all.
     * 
     * Optionally configure with fallback baudrate, to be set if command mode was not available. 
     * If fallback is set to value 0, no fallback is performed.
     * 
     * Some notes on usage:<ul>
     * <li>hc12debugTool tries to detect the current baudrate of the hc-12 module and will change if necessary.</li>
     * <li>It is not necessary to call begin() beforehand on the hc12Serial interface.
     * hc12DebugTool detects that and will will try to dete</li>
     * 
     * @param setPin number of gpio the set pin of hc-12 module is/might be connected to.
     * @param hc12Serial UART interface (stack instance) the hc-12 is connected to
     * @param debug Printer where activity info and unexpected bytes are written to
     * @param fallbackSerialTo if value>0 and command mode is not available, Serial baudrate is set to that value. Defaults value is 9600 [Baud].
     * @param waitForAvailableWrite set to 0 if using SoftwareSerial! value >0 is number of milliseconds to wait for Serial.availableForWrite()
     */
    // Note: one might need a mode without printing anything to debug... implement it if-needed using an additional flag and set-method
    Hc12DebugTool(uint8_t setPin,
                  S &hc12Serial,
                  Print &debug,
                  uint32_t fallbackSerialTo = 9600,
                  uint8_t waitForAvailableWrite = 0) : _setPinNo(setPin), _hc12Serial(hc12Serial), _debug(debug), _fallbackSerialTo(fallbackSerialTo), _waitForAvailableWrite(waitForAvailableWrite) {}

    /**
     * Sets the given baudrate for the module.
     * 
     * If a HC-12 module is detected, it will be configured using AT-command to the baudrate given as argument.
     * 
     * Note that this affects also air baud rate of HC-12 module, see datasheet for more details.
     */
    void setPreferredBaudrate(Hc12_BaudRate baudRate)
    {
        if (!_setPinNo || !_hc12Serial)
            return;
        // read any bytes still in buffer/transmission
        dumpPendingBytes();
        _debug.print(F("\nConfiguring HC-12: "));
        pinMode(_setPinNo, OUTPUT);
        digitalWrite(_setPinNo, LOW);
        delay(40); // wait to enter command mode
        // 1st attempt communication with HC-12 with pre-set baudrate
        bool commandModeEnabled = false;
        if (_hc12Serial.isListening() && sendValidatedCommand(COMMAND_AT, RESPONSE_AT, true))
        {
            commandModeEnabled = true;
        }
        else
        {
            // 1st try with preferred baudrate, in case it is already set at the module
            dumpPendingBytes();
            changeBaudRate(HC12_BAUDRATE_NUMERIC[baudRate]);
            if (sendValidatedCommand(COMMAND_AT, RESPONSE_AT, true))
            {
                _debug.print(F(" hc12serial found at preferred baudrate, "));
                commandModeEnabled = true;
            }
            else
            {
                // 2nd: try default of HC-12
                changeBaudRate(9600);
                if (sendValidatedCommand(COMMAND_AT, RESPONSE_AT, true))
                {
                    _debug.print(F(" hc12serial found at 9600 baud, "));
                    commandModeEnabled = true;
                }
                else
                    // try all existing baudrates
                    for (int i = 0; i < NUM_HC12_BAUDRATES; ++i)
                    {
                        changeBaudRate(HC12_BAUDRATE_NUMERIC[i]);
                        _hc12Serial.flush();
                        delay(10);
                        dumpPendingBytes();
                        if (sendValidatedCommand(COMMAND_AT, RESPONSE_AT, false))
                        {
                            _debug.print(F(" found hc12serial at "));
                            _debug.print(HC12_BAUDRATE_STRING[i]);
                            _debug.print(F(" baud, "));
                            commandModeEnabled = true;
                            break;
                        }
                    }
            }
        }
        // tolerate incoming, unexpected bytes, because there could be more on the line
        if (commandModeEnabled) // assume set pin is connected since connected module reacts to AT commands,
        {
            // "AT+RB" - query baudrate: response should be: "OK+B9600"
            _hc12Serial.write("AT+RB", 5);
            if (readExpectedResponse("OK+B", false) && readExpectedResponse(HC12_BAUDRATE_STRING[baudRate], false))
            {
                _debug.println(F("preferred baudrate already configured"));
            }
            else
            {
                // "AT+Bxxxx" - set baudrate: response should be "OK+B19200"
                _hc12Serial.write("AT+B", 4);
                if (sendValidatedCommand(HC12_BAUDRATE_STRING[baudRate], "OK+B", false))
                {
                    if (readExpectedResponse(HC12_BAUDRATE_STRING[baudRate], false))
                    {
                        _debug.print(F("baudrate set to "));
                        _debug.println(HC12_BAUDRATE_STRING[baudRate]);
                        changeBaudRate(HC12_BAUDRATE_NUMERIC[baudRate]);
                    }
                    else
                    {
                        if (0 < _fallbackSerialTo)
                        {
                            _debug.println(F("unexpected response setting baudrate, setting local to fallback"));
                            changeBaudRate(_fallbackSerialTo);
                        }
                        else
                            _debug.println(F("unexpected response setting baudrate"));
                    }
                }
            }
        }
        else
        {
            if (0 < _fallbackSerialTo)
            {
                _debug.println(F(" -> command mode not available, setting local to fallback"));
                changeBaudRate(_fallbackSerialTo);
            }
            else
                _debug.println(F(" -> command mode not available"));
        }
        // wait to enter UART mode
        delay(80);
    }

private:
    uint8_t _setPinNo;
    S &_hc12Serial;
    Print &_debug;
    uint32_t _fallbackSerialTo;
    uint8_t _waitForAvailableWrite;

    /** send command and wait for specific response */
    boolean sendValidatedCommand(const char *command, const char *expectedResponse, const bool tolerateUnexpected)
    {
        if (tolerateUnexpected && _waitForAvailableWrite)
        {
            uint8_t writeCycles = _waitForAvailableWrite;
            while (!_hc12Serial.availableForWrite() && writeCycles > 0)
            {
                --writeCycles;
                delay(1);
            }
            if (!writeCycles)
            {
                _debug.println(F("hc12serial not available for write"));
                return false;
            }
        }

        /* does not work, try again later?
        const char *pgmStr = command;
        char c;
        while ('\0' != (c = pgm_read_byte(pgmStr++)))
        {
            HC12TOOL_DEBUG(c);
            _hc12Serial.write(c);
        }
        */
        _hc12Serial.write(command, strlen(command));
        delay(50);
        return readExpectedResponse(expectedResponse, tolerateUnexpected);
    }

    /** wait for specific response, return true if found */
    // note: each response from HC-12 is termined by "\r\n" (<13>,<10>)
    boolean readExpectedResponse(const char *expectedResponse, const bool tolerateUnexpected)
    {
        size_t responsePos = 0;
        uint8_t readCycles = 100; // maximum number of wait cycles to wait for incoming response
        // algorithm already prepared for progmem usage
        char expectedByte = expectedResponse[responsePos];
        while ('\0' != expectedByte && (readCycles > 0))
        {
            if (!_hc12Serial.available())
            {
                --readCycles;
                delay(1);
            }
            while (('\0' != expectedByte) && _hc12Serial.available())
            {
                const int x = _hc12Serial.read();
                if (x == expectedByte)
                {
                    ++responsePos;
                    expectedByte = expectedResponse[responsePos];
                }
                else
                { // got something else
                    _debug.write((char)x);
                    if (tolerateUnexpected)
                    {
                        responsePos = 0;
                        expectedByte = expectedResponse[responsePos];
                    }
                    else
                        return false;
                }
            }
        }
        // got the expected response?
        return '\0' == expectedByte;
    }

    void changeBaudRate(unsigned long baudRate)
    {
        _hc12Serial.flush(); // always write pending data in TX-FIFO
        _hc12Serial.SETBAUDRATE(baudRate);
        _debug.print(F("\nset baudrate to "));
        _debug.println(baudRate);
    }

    void dumpPendingBytes()
    {
        if (_hc12Serial.available())
        {
            _debug.print(F("<unexpected>"));
            while (_hc12Serial.available())
                _debug.write(_hc12Serial.read());
            _debug.print(F("</unexpected>"));
            _debug.println();
        }
    }
};
#endif
