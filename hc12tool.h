/**
 * This class serves as utility to manage a HC-12 module on UART0 as remote debugging connector.
 * 
 * Simply connect the HC12 with your Txd0 and RxD0 pins, that is your default UART Arduino addresses with the Serial object:<ul>
 * <li> Txd0 of Arduino module  ==>  Rx pin of HC-12 module
 * <li> Rxd0 of Arduino module  ==>  Tx pin of HC-12 module
 * <li> any output-able GPIO    ==>  set pin of HC-12 module
 * <li> 3.3V of Arduino module  ==>  Vcc of HC-12 module
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
 * A call to setBaudrate() will produce a single line of output on serial. * 
 * If hc-12 command mode is available, the remote module will receive the text:
 * <pre>Configuring HC-12: baudrate set to &lt;n&gt;</pre>
 * If not available, the remote module will receive the text:
 * <pre>Configuring HC-12: AT -> command mode not available</pre>
 * 
 * Optionally, sets more options of the HC-12 module:<ul>
 * <li>channel (AT+C)</li>
 * <li>PA output (AT+P)</li>
 * <li>transmission mode (AT+FU)</li>
 * </ul>
 * 
 * Typical use (use heap instance since only needed in setup) might be:<pre>
 * #include "universalUIglobal.h"
 * void setup() {
 *   Hc12Tool hc12Tool(5);
 *   hc12Tool.setBaudrate(BPS115200);
 *   //...
 * }
 * </pre>
 * 
 * Per default, received unexpected bytes and activity info is logged to Serial. This can be configured by calling
 * <pre>hc12Tool.setVerbosity();</pre>
 * and affects subsequent actions.
 * 
 * 
 * 
 * Learnings not stated in the datasheet:
 * <li>every response from module is terminated by "\r\n"
 * <li>to get test command executed, it should be terminated by "\r\n", otherwise can't distinguish from other commands 
 * <li>module does not like set-commands after query sent with SoftwareSerial.println(), then always answers with "ERROR"
 */
#ifndef _HC12_DEBUG_TOOL_H
#define _HC12_DEBUG_TOOL_H
#include <Arduino.h>

// available baud rates of HC-12 module: 1200bps, 2400bps, 4800bps, 9600bps, 19200bps, 38400bps, 57600bps and 115200bps
enum Hc12_BaudRate
{
    BPS1200 = 0,
    BPS2400 = 1,
    BPS4800 = 2,
    BPS9600 = 3, // default value of HC-12 module
    BPS19200 = 4,
    BPS38400 = 5,
    BPS57600 = 6,
    BPS115200 = 7
};

enum Hc12_TransmissionPower
{
    DBMminus1 = 1, //  0.8 mW
    DBM2 = 2,      //  1.6 mW
    DBM5 = 3,      //  3.2 mW
    DBM8 = 4,      //  6.3 mW
    DBM11 = 5,     //  12 mW
    DBM14 = 6,     //  25 mW
    DBM17 = 7,     //  50 mW
    DBM20 = 8      // 100 mW, default value of HC-12 module
};

enum Hc12_TransmissionMode
{
    FU1 = 1, // more power saving / short range: air baud rate fixed at (maximum) 250kbps, Iidle ~ 3.6mA
    FU2 = 2, // power saving: UART limited to 1200/2400/4800 bps / short range, air baud rate fixed at maximum, Iidle ~0.08mA
    FU3 = 3, // default = full speed: flexible air baud rate, Iidle~16mA
    FU4 = 4  // ultra long distance: UART fixed at 1200 bps, air baud rate fixed at 500 bps
};

////// now internal macro and data structures
#ifdef VERBOSE_DEBUG_HC12TOOL // debugging support
#define HC12TOOL_DEBUG(X) Serial.print(X);
#else
#define HC12TOOL_DEBUG(X) ;
#endif //of: ifdef DEBUG_HC12TOOL

#if defined(ESP32) || defined(ESP8266) // define function to use for setting the baudrate
#define SETBAUDRATE updateBaudRate
#else
#define SETBAUDRATE begin
#endif

struct Hc12toolVerbosity
{
    bool showUnexpectedBytes : 1;
    bool printActivityInfo : 1, : 5;
    bool baudRateSet : 1;
};

// ordering must adhere to order of enum `Hc12_BaudRate`
#define NUM_HC12_BAUDRATES 8
const unsigned long HC12_BAUDRATE_NUMERIC[NUM_HC12_BAUDRATES] = {1200, 2400, 4800, 9600, 19200, 38400, 57600, 115200};

// TODO convert to use it from progmem/flash; did not get it working yet
const char *const COMMAND_AT /*PROGMEM*/ = "AT\r\n";  // AT-command must be line-terminated to indicate end-of-command
const char *const RESPONSE_AT /*PROGMEM*/ = "OK\r\n"; // HC-12 terminates his responses always with \r\n (<13>,<10>)

#define HC12_READCONFIGURATION_MAXBUFLEN 40 // experiment showed to expect 32 bytes

template <typename S>
class Hc12Tool
{
public:
    /**
     * Create instance of this class, configured with GPIO connected to set pin.
     * This pin will be configured for output at `setBaudrate()`.
     * If pin number is 0, calls to setBaudrate has no effect at all.
     * 
     * Optionally configure with fallback baudrate, to be set if command mode was not available. 
     * If fallback is set to value 0, no fallback is performed.
     * 
     * Some notes on usage:<ul>
     * <li>Hc12Tool tries to detect the current baudrate of the hc-12 module and will change if necessary.</li>
     * <li>It is not necessary to call begin() beforehand on the hc12Serial interface.
     * Hc12Tool detects that and will will try to dete</li>
     * 
     * @param setPin number of gpio the set pin of hc-12 module is/might be connected to.
     * @param hc12Serial UART interface (stack instance) the hc-12 is connected to
     * @param fallbackSerialTo if value>0 and command mode is not available, Serial baudrate is set to that value. Defaults value is 9600 [Baud].
     * @param waitForAvailableWrite set to 0 if using SoftwareSerial! value >0 is number of milliseconds to wait for Serial.availableForWrite()
     */
    // Note: one might need a mode without printing anything to debug... implement it if-needed using an additional flag and set-method
    Hc12Tool(uint8_t setPin,
             S &hc12Serial,
             uint32_t fallbackSerialTo = 9600,
             uint8_t waitForAvailableWrite = 0) : _setPinNo(setPin), _hc12Serial(hc12Serial), _fallbackSerialTo(fallbackSerialTo), _waitForAvailableWrite(waitForAvailableWrite) {}

    void setParameters(Hc12_BaudRate baudRate, Hc12_TransmissionPower power)
    {
        if (enterCommandMode(baudRate))
        {
            configureTransmissionPower(power);
            configureBaudrate(baudRate);
            exitCommandMode();
        }
    }
    void setParameters(Hc12_BaudRate baudRate, Hc12_TransmissionPower power, const uint8_t channel)
    {
        if (enterCommandMode(baudRate))
        {
            configureChannel(channel);
            configureTransmissionPower(power);
            configureBaudrate(baudRate);
            exitCommandMode();
        }
    }
    void setParameters(Hc12_BaudRate baudRate, Hc12_TransmissionPower power, const uint8_t channel, Hc12_TransmissionMode mode)
    {
        if (enterCommandMode(baudRate))
        {
            configureChannel(channel);
            configureTransmissionPower(power);
            configureTransmissionMode(mode);
            configureBaudrate(baudRate); // must be the last command, as it adapts the baudrate of _hc12Serial
            exitCommandMode();
        }
    }

    /**
     * Sets the given baudrate for the module.
     * 
     * If a HC-12 module is detected, it will be configured using AT-command to the baudrate given as argument.
     * 
     * Note that this affects also air baud rate of HC-12 module, see datasheet for more details.
     */
    void setBaudrate(Hc12_BaudRate baudRate)
    {
        if (enterCommandMode(baudRate))
        {
            configureBaudrate(baudRate);
            exitCommandMode();
        }
    }

    void setChannel(const uint8_t channel)
    {
        if (enterCommandMode(BPS9600))
        {
            configureChannel(channel);
            exitCommandMode();
        }
    }
    void setTransmissionPower(Hc12_TransmissionPower power)
    {
        if (enterCommandMode(BPS9600))
        {
            configureTransmissionPower(power);
            exitCommandMode();
        }
    }
    void setTransmissionMode(Hc12_TransmissionMode mode)
    {
        if (enterCommandMode(BPS9600))
        {
            configureTransmissionMode(mode);
            exitCommandMode();
        }
    }

    /**
     * Configures log verbosity.
     * Default setting after class initialization is: true, true, Serial.
     * 
     * @param showActivityInfo if true, messages for activity are printed to debugStream
     * @param showUnexpectedBytes if true, incoming unexpected bytes are dumped to debugStream
     * @param debugStream where to print verbose log to. Defaults to Serial
     */
    void setVerbosity(const bool showActivityInfo, const bool showUnexpectedBytes = false, Print &debugStream = Serial)
    {
        _verbosity.showUnexpectedBytes = showUnexpectedBytes;
        _verbosity.printActivityInfo = showActivityInfo;
        _debug = debugStream;
    }

    /**
     * Queries configuration of module.
     * 
     * Requires configured baudrate, so best call it after <code>set...()</code>.
     *
     * Note: result is allocated on heap, you should free it of no longer used.
     * This is implemented intentionally to have it available after exiting this method.
     */
    char *getConfigurationInfo()
    {
        dumpPendingBytes();
        pinMode(_setPinNo, OUTPUT);
        digitalWrite(_setPinNo, LOW);
        delay(40); // wait to enter command mode
        _hc12Serial.println("AT+RX");
        char buf[HC12_READCONFIGURATION_MAXBUFLEN + 1];
        size_t bufPos = 0;
        const unsigned long startTime = millis();
        while ((_hc12Serial.available() || ((millis() - startTime) < 300)) && (bufPos < HC12_READCONFIGURATION_MAXBUFLEN))
        {
            while (_hc12Serial.available() && (bufPos < HC12_READCONFIGURATION_MAXBUFLEN))
            {
                buf[bufPos++] = _hc12Serial.read();
                // ignore substring "OK+"
                if (bufPos >= 3 && 'O' == buf[bufPos - 3] && 'K' == buf[bufPos - 2] && '+' == buf[bufPos - 1])
                    bufPos -= 3;
                buf[bufPos] = '\0';
            }
        }
        digitalWrite(_setPinNo, HIGH);
        delay(80);
        char *result = (char *)malloc(bufPos + 1);
        memcpy(result, buf, bufPos + 1);
        return result;
    }

    /**
     * Read and write to target all available bytes from source, 
     * with waiting at least till timeout or numMinByte received.
     * 
     * @param source stream where to read from (e.g. HC-12)
     * @param target stream where to write read bytes to
     * @param numMinByte minimum number of bytes to wait for before exiting (or timeout)
     * @param maxWaitMillis number if milliseconds to wait at most (effective timeout)
     */
    static void waitAndDump(Stream &source, Stream &target, uint8_t numMinByte, const uint64_t maxWaitMillis)
    {
        const unsigned long startTime = millis();
        while (source.available() || (numMinByte && ((millis() - startTime) < maxWaitMillis)))
        {
            while (source.available())
            {
                const int v = source.read();
                target.write(v);
                --numMinByte;
            }
        }
    }

private:
    uint8_t _setPinNo;
    S &_hc12Serial;
    uint32_t _fallbackSerialTo;
    uint8_t _waitForAvailableWrite;
    Hc12toolVerbosity _verbosity = {true, true, false};
    Print &_debug = Serial;

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
                logActivity(F("hc12serial not available for write"));
                return false;
            }
        }
        sendCommand(command);
        return readExpectedResponse(expectedResponse, tolerateUnexpected, false);
    }
    void sendCommand(const char *command)
    {
        /* TODO: does not work, try again later?
        const char *pgmStr = command;
        char c;
        while ('\0' != (c = pgm_read_byte(pgmStr++)))
        {
            HC12TOOL_DEBUG(c);
            _hc12Serial.write(c);
        }
        */
        _hc12Serial.write(command, strlen(command));
        _hc12Serial.flush(); // wait till command has been fully sent
    }

    void changeBaudRate(unsigned long baudRate, const bool forceSet)
    {
        _hc12Serial.flush(); // always write pending data in TX-FIFO
        _hc12Serial.SETBAUDRATE(baudRate);
        logActivity(F("  set serial-baudrate to "));
        if (_verbosity.printActivityInfo)
            _debug.println(baudRate);
    }

    void dumpVerboseChar(const char c)
    {
        if (_verbosity.showUnexpectedBytes)
            _debug.write(c);
    }
    void dumpPendingBytes()
    {
        if (_verbosity.showUnexpectedBytes)
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
        else
        {
            while (_hc12Serial.available())
                _hc12Serial.read();
        }
    }

    void logActivity(const __FlashStringHelper *msg)
    {
        if (_verbosity.printActivityInfo)
        {
            _debug.print(msg);
        }
    }
    void logActivity(const char *value)
    {
        if (_verbosity.printActivityInfo)
        {
            _debug.print(value);
        }
    }
    void logActivity(const uint32_t &value)
    {
        if (_verbosity.printActivityInfo)
        {
            _debug.print(value);
        }
    }

    bool enterCommandMode(Hc12_BaudRate baudRate)
    {
        if (!_setPinNo || !_hc12Serial)
            return false;
        // read any bytes still in buffer/transmission
        dumpPendingBytes();
        logActivity(F("\nConfiguring HC-12: "));
        pinMode(_setPinNo, OUTPUT);
        digitalWrite(_setPinNo, LOW);
        delay(41); // wait to enter command mode
        // 1st attempt communication with HC-12 with pre-set baudrate
        if (_hc12Serial.isListening() && sendValidatedCommand(COMMAND_AT, RESPONSE_AT, true))
        {
            _verbosity.baudRateSet = true;
            return true;
        }
        else
        {
            // 1st try with preferred baudrate, in case it is already set at the module
            dumpPendingBytes();
            changeBaudRate(HC12_BAUDRATE_NUMERIC[baudRate], false);
            if (sendValidatedCommand(COMMAND_AT, RESPONSE_AT, true))
            {
                _verbosity.baudRateSet = true;
                logActivity(F("  hc12serial found at preferred baudrate, "));
                return true;
            }
            else
            {
                // 2nd: try default of HC-12
                changeBaudRate(9600, false);
                if (sendValidatedCommand(COMMAND_AT, RESPONSE_AT, true))
                {
                    _verbosity.baudRateSet = true;
                    logActivity(F(" hc12serial found at 9600 baud, "));
                    return true;
                }
                else
                    // try all existing baudrates
                    for (int i = 0; i < NUM_HC12_BAUDRATES; ++i)
                    {
                        changeBaudRate(HC12_BAUDRATE_NUMERIC[i], false);
                        _hc12Serial.flush();
                        delay(10);
                        dumpPendingBytes();
                        if (sendValidatedCommand(COMMAND_AT, RESPONSE_AT, false))
                        {
                            _verbosity.baudRateSet = true;
                            logActivity(F(" found hc12serial at "));
                            logActivity(HC12_BAUDRATE_NUMERIC[i]);
                            logActivity(F(" baud, "));
                            return true;
                        }
                    }
            }
        }

        if (0 < _fallbackSerialTo)
        {
            logActivity(F(" -> command mode not available, setting local to fallback"));
            changeBaudRate(_fallbackSerialTo, false);
        }
        else
            logActivity(F(" -> command mode not available"));
        return false;
    }
    void exitCommandMode()
    {
        digitalWrite(_setPinNo, HIGH);
        // 80ms wait to enter UART mode; 200ms to avoid resetting default UART mode (9600, 8N1) with entering command mode again
        delay(220);
    }

    void ignoreEndOfLine(bool consumeOtherChars)
    {
        int waitCycles = 30; // we need 4 cycles at least for AT+RP response
        uint8_t state = 0;
        HC12TOOL_DEBUG("\nIgnoring...")
        while (waitCycles)
        {
            int x = -1;
            HC12TOOL_DEBUG("i");
            while (_hc12Serial.available())
            {
                x = _hc12Serial.read();
                HC12TOOL_DEBUG(" c:")
                HC12TOOL_DEBUG(x)
                if ('\r' == x)
                {
                    state = 1;
                    break;
                }
                else if ('\n' == x)
                {
                    state = (1 == state) ? 2 : 0;
                    break;
                }
                else if (!consumeOtherChars)
                    break;
            }
            if (2 == state)
                break; // terminating with EOL found
            HC12TOOL_DEBUG("|")
            if (-1 != x)
                dumpVerboseChar((char)x);
            delay(1); // to decouple wait cycles from processing speed
            --waitCycles;
        }
        HC12TOOL_DEBUG(" done.\n")
    }

    /** wait for specific response, return true if found */
    // note: each response from HC-12 is termined by "\r\n" (<13>,<10>)
    boolean readExpectedResponse(const char *expectedResponse, const bool tolerateUnexpected, const bool acceptTerminators)
    {
        size_t responsePos = 0;
        uint8_t readCycles = 100; // maximum number of wait cycles to wait for incoming response

        char expectedByte = expectedResponse[responsePos]; // preparation for algorithm with for progmem usage
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
                    HC12TOOL_DEBUG('{')
                    HC12TOOL_DEBUG((char)x)
                    HC12TOOL_DEBUG('}')
                }
                else
                { // got something else
                    HC12TOOL_DEBUG('<')
                    HC12TOOL_DEBUG(x)
                    HC12TOOL_DEBUG('>')
                    dumpVerboseChar((char)x);
                    if (tolerateUnexpected)
                    {
                        responsePos = 0;
                        expectedByte = expectedResponse[responsePos];
                    }
                    else
                    {
                        return false;
                    }
                }
            }
        }
        // read+consume terminating newline
        if (acceptTerminators)
            ignoreEndOfLine(false);
        // got the expected response?
        return '\0' == expectedByte;
    }

    /** @return true if successful */
    bool configure(const char *queryCommand, const char *expectedQueryResponse, char *expectedQueryValue,
                   const char *setCommand, char *commandValue, const char *expectedSetResponse,
                   const __FlashStringHelper *successMessage)
    {
        HC12TOOL_DEBUG(F("[query='"))
        HC12TOOL_DEBUG(queryCommand)
        HC12TOOL_DEBUG(F("', expecting='"))
        HC12TOOL_DEBUG(expectedQueryResponse)
        HC12TOOL_DEBUG(expectedQueryValue)
        HC12TOOL_DEBUG(F("'] -> "))
        sendCommand(queryCommand);
        if (readExpectedResponse(expectedQueryResponse, false, false) && readExpectedResponse(expectedQueryValue, false, true))
        {
            logActivity(F("  "));
            logActivity(successMessage);
            logActivity(expectedQueryValue);
            logActivity(F(" already set\n"));
            return true;
        }
        else
        {
            ignoreEndOfLine(true); // ignore remaing response to query command
            dumpPendingBytes();
            // digitalWrite(_setPinNo, HIGH);
            // delay(280); // wait 80ms to exit command mode
            // digitalWrite(_setPinNo, LOW);
            // delay(41); // wait to enter command mode
            HC12TOOL_DEBUG(F("sending set-command: "))
            sendCommand(setCommand);
            bool result = false;
            if (sendValidatedCommand(commandValue, expectedSetResponse, false))
            {
                if (readExpectedResponse(commandValue, false, true))
                {
                    logActivity(F("  successfully set "));
                    logActivity(successMessage);
                    logActivity(commandValue);
                    result = true;
                }
                else
                {
                    logActivity(F("unexpected response to "));
                    logActivity(setCommand);
                    logActivity(commandValue);
                }
            }
            else
            {
                logActivity(F("failed sending command "));
                logActivity(setCommand);
                logActivity(commandValue);
            }
            logActivity(F("\n"));
            return result;
        }
    }

    // "AT+RB" - query baudrate: response should be: "OK+B9600"
    void configureBaudrate(Hc12_BaudRate baudRate)
    {
        if (BPS1200 <= baudRate && baudRate <= BPS115200)
        {
            char str[7]; // 6 chars for "115200" incl. \0-terminator
            snprintf_P(str, 7, PSTR("%lu"), HC12_BAUDRATE_NUMERIC[baudRate]);
            if (configure("AT+RB", "OK+B", str, "AT+B", str, "OK+B", F("baudrate ")))
            {
                changeBaudRate(HC12_BAUDRATE_NUMERIC[baudRate], true);
            }
            else
            {
                if (0 < _fallbackSerialTo)
                {
                    logActivity(F("  setting baudrate to fallback\n"));
                    changeBaudRate(_fallbackSerialTo, true);
                }
            }
        }
        else
            logActivity(F("invalid baudrate"));
    }

    // AT+Cxxx / response should be: OK+C021
    void configureChannel(const uint8_t channel)
    {
        if (0 < channel && channel <= 127)
        {
            char str[4]; // 3 digits with leading 0 + \0-terminator
            snprintf_P(str, 4, PSTR("%03u"), channel);
            configure("AT+RC", "OK+RC", str, "AT+C", str, "OK+C", F("channel "));
        }
        else
            logActivity(F("invalid channel"));
    }
    void configureTransmissionPower(Hc12_TransmissionPower power)
    {
        if (DBMminus1 <= power && power <= DBM20)
        {
            // response is: OK+RP:+20 dBm
            char str[4]; // 3 digits with leading 0 + \0-terminator
            snprintf_P(str, 4, PSTR("%d"), power);
            char str2[8];
            snprintf_P(str2, 8, PSTR("%0+3ddBm"), (power - 1) * 3 - 1);
            configure("AT+RP", "OK+RP:", str2, "AT+P", str, "OK+P", F("transmission power "));
        }
        else
        {
            logActivity(F("invalid power"));
        }
    }
    void configureTransmissionMode(Hc12_TransmissionMode mode)
    {
        char str[2];                   // 3 digits with leading 0 + \0-terminator
        str[0] = ((uint8_t)mode) + 48; // 48 is decimal for '0
        str[1] = '\0';
        if (FU1 <= mode && mode <= FU4)
        {
            configure("AT+RF", "OK+FU", str, "AT+FU", str, "OK+FU", F("transmission mode FU"));
        }
        else
        {
            logActivity(F("invalid mode"));
        }
    }
};
#endif
