/**
 * MiniJson.h  –  Streaming (SAX-style) JSON parser
 *
 * Designed for severely resource-constrained systems:
 *
 *   Target   : Arduino Uno  (ATmega328P – 2 KB SRAM, 32 KB Flash)
 *   RAM      : sizeof(Parser<8,32>) ≈ 54 bytes, entirely on the stack / in a
 *              global – zero heap allocations, ever.
 *   Flash    : ≈ 1.6 KB  (≈ 3.2 KB when float support is enabled)
 *   Standard : C++11 (subset available on avr-g++)
 *
 * ── Model ───────────────────────────────────────────────────────────────────
 *
 * Characters are fed one at a time via Parser::feed(char).  Every completed
 * JSON token fires a single user callback:
 *
 *   void myHandler(MiniJson::Event ev,
 *                  const char*    str,   // EV_KEY / EV_STRING / EV_ERROR
 *                  long           ival,  // EV_INTEGER / EV_BOOL (0 or 1)
 *                  float          fval,  // EV_FLOAT
 *                  void*          ctx)   // user-supplied context pointer
 *
 * The `str` pointer is valid only for the duration of the callback; it points
 * into an internal fixed-size buffer.  Copy it if you need it to outlive the
 * call.
 *
 * ── Quick start ─────────────────────────────────────────────────────────────
 *
 *   #include <MiniJson.h>
 *
 *   MiniJson::Parser<> parser;  // MAX_DEPTH=8, MAX_STRING=32
 *
 *   void handler(MiniJson::Event ev, const char* s, long i, float f, void*) {
 *     if (ev == MiniJson::EV_KEY)     { Serial.print("key: "); Serial.println(s); }
 *     if (ev == MiniJson::EV_INTEGER) { Serial.print("int: "); Serial.println(i); }
 *   }
 *
 *   void setup() {
 *     Serial.begin(9600);
 *     parser.begin(handler);
 *   }
 *   void loop() {
 *     while (Serial.available())
 *       parser.feed((char)Serial.read());
 *   }
 *
 * ── Template parameters ──────────────────────────────────────────────────────
 *
 *   MAX_DEPTH   Maximum nesting of objects/arrays.  Each level costs 1 byte.
 *               Default 8.  Values >254 are clamped by the uint8_t type.
 *
 *   MAX_STRING  Size of the internal character buffer INCLUDING the null
 *               terminator.  Strings/keys longer than (MAX_STRING-1) chars
 *               are silently truncated.  Default 32 (31 usable chars).
 *
 * ── Compile-time switches ────────────────────────────────────────────────────
 *
 *   #define MINIJSON_NO_FLOAT
 *     Disables floating-point parsing (EV_FLOAT is never fired; fractional
 *     numbers are reported as EV_INTEGER via truncation).  Saves ≈ 1.5 KB
 *     Flash on AVR by avoiding the soft-float library pull-in.
 *
 * ── Error handling ───────────────────────────────────────────────────────────
 *
 *   On a parse error, EV_ERROR is fired with a short description in `str`
 *   and the parser latches into the error state.  Call reset() (or begin()
 *   again) to restart.
 *
 * ── End-of-input ─────────────────────────────────────────────────────────────
 *
 *   For numbers and literals that are NOT followed by a delimiter (e.g. a
 *   bare `42` with no trailing whitespace), call flush() after the last
 *   character.  For values inside containers (objects, arrays) this is never
 *   needed because the closing `}` / `]` acts as a natural delimiter.
 *
 * ── Portability ──────────────────────────────────────────────────────────────
 *
 *   The file is self-contained.  On Arduino it includes <Arduino.h>;
 *   on any other platform it falls back to standard C headers only.
 *   No virtual functions, no RTTI, no exceptions, no std::.
 */

#pragma once

#ifdef ARDUINO
#  include <Arduino.h>
#else
#  include <stdint.h>
#  include <string.h>
#  include <stdlib.h>   // atol, atof
#endif

namespace MiniJson
{

// ── Event codes ───────────────────────────────────────────────────────────────

enum Event : uint8_t
{
    EV_OBJECT_START,  ///< '{' opened           – str/ival/fval unused
    EV_OBJECT_END,    ///< '}' closed
    EV_ARRAY_START,   ///< '[' opened
    EV_ARRAY_END,     ///< ']' closed
    EV_KEY,           ///< object key            – str = key text (null-terminated)
    EV_STRING,        ///< string value          – str = value text
    EV_INTEGER,       ///< integer number        – ival = value
    EV_FLOAT,         ///< floating-point number – fval = value
    EV_BOOL,          ///< boolean               – ival = 1 (true) or 0 (false)
    EV_NULL,          ///< null keyword
    EV_ERROR,         ///< parse error           – str = short description
    EV_DONE,          ///< top-level value fully parsed
};

// ── Callback type ─────────────────────────────────────────────────────────────

/**
 * Every JSON event calls back here.
 *
 * @param ev    Event code (see enum above).
 * @param str   Non-null for EV_KEY, EV_STRING, EV_ERROR; null otherwise.
 *              Points into an internal buffer – valid only during this call.
 * @param ival  Meaningful for EV_INTEGER and EV_BOOL.
 * @param fval  Meaningful for EV_FLOAT.
 * @param ctx   The user-data pointer passed to Parser::begin().
 */
typedef void (*Callback)(Event ev,
                         const char* str,
                         long        ival,
                         float       fval,
                         void*       ctx);

// ── Parser ────────────────────────────────────────────────────────────────────

/**
 * Template parameters:
 *   MAX_DEPTH   – maximum nesting depth (default 8, 1 byte per level)
 *   MAX_STRING  – buffer size incl. null terminator (default 32)
 */
template<uint8_t MAX_DEPTH = 8, uint8_t MAX_STRING = 32>
class Parser
{
public:
    // ── Construction ─────────────────────────────────────────────────────────

    Parser() { reset(); }

    /**
     * Attach callback and optional user-data; also resets parser state.
     * Must be called before the first feed().
     */
    void begin(Callback cb, void* ctx = nullptr)
    {
        m_cb  = cb;
        m_ctx = ctx;
        reset();
    }

    /**
     * Reset to initial state, ready to parse a fresh document.
     * Does NOT clear the callback / context set by begin().
     */
    void reset()
    {
        m_state   = ST_VALUE;
        m_depth   = 0;
        m_bufLen  = 0;
        m_escape  = false;
        m_uEscape = 0;
        m_inKey   = false;
        m_hasDot  = false;
        m_hasExp  = false;
        m_litChar = 0;
        m_litPos  = 0;
    }

    // ── Feeding ───────────────────────────────────────────────────────────────

    /**
     * Feed one character into the parser.
     *
     * @return true  while more input is expected.
     * @return false once EV_DONE or EV_ERROR has been fired.
     *
     * Calling feed() after it returns false is safe (no-op).
     */
    bool feed(char c);

    /**
     * Feed every character of a null-terminated string.
     * Stops early on EV_DONE or EV_ERROR.
     */
    void feedString(const char* s)
    {
        while (*s != '\0' && feed(*s))
            ++s;
    }

#ifdef ARDUINO
    /**
     * Feed every character of a Flash-string (F("...") macro).
     * Only available on Arduino.
     */
    void feedProgmem(const __FlashStringHelper* s)
    {
        const char* p = reinterpret_cast<const char*>(s);
        char c;
        while ((c = static_cast<char>(pgm_read_byte(p++))) != '\0' && feed(c))
            ;
    }
#endif

    /**
     * Signal end-of-input.
     *
     * Needed when the last value is a bare number or literal NOT followed by
     * whitespace or a closing bracket – e.g. the string `"42"` with no
     * trailing newline.  Safe to call at any time; has no effect in states
     * other than ST_NUMBER and ST_LITERAL.
     */
    void flush()
    {
        if (m_state == ST_DONE || m_state == ST_ERROR)
            return;

        if (m_state == ST_NUMBER)
        {
            endNumber();
            afterValue();
            // afterValue() may have set ST_DONE (depth==0) or ST_COMMA
            // (still inside a container).  An unclosed container is caught
            // by the depth check below.
        }
        else if (m_state == ST_LITERAL)
        {
            // A complete literal must have all its characters already fed.
            // If it hasn't, that is a genuine truncation error.
            error("unexpected EOF");
            return;
        }

        // If there are unclosed containers at EOF that is a syntax error.
        if (m_state != ST_DONE && m_state != ST_ERROR && m_depth > 0)
        {
            error("unclosed container");
        }
    }

    // ── Status ────────────────────────────────────────────────────────────────

    bool isDone()   const { return m_state == ST_DONE;  }
    bool hasError() const { return m_state == ST_ERROR; }

    /** Current nesting depth (0 = at top level). */
    uint8_t depth() const { return m_depth; }

private:
    // ── Internal state codes ──────────────────────────────────────────────────

    enum State : uint8_t
    {
        ST_VALUE,    ///< expecting a value
        ST_KEY,      ///< expecting a key or '}'
        ST_COLON,    ///< expecting ':'
        ST_COMMA,    ///< expecting ',' or closing bracket
        ST_STRING,   ///< inside a quoted string
        ST_NUMBER,   ///< accumulating number characters
        ST_LITERAL,  ///< accumulating true / false / null
        ST_DONE,
        ST_ERROR,
    };

    enum Container : uint8_t { CT_OBJECT = 0, CT_ARRAY = 1 };

    // ── Member variables ─────────────────────────────────────────────────────
    //
    // Total for Parser<8,32>:
    //   m_cb + m_ctx          : 4 bytes  (2-byte pointers on AVR)
    //   m_state               : 1 byte
    //   m_depth               : 1 byte
    //   m_stack[MAX_DEPTH]    : 8 bytes
    //   m_buf[MAX_STRING]     : 32 bytes
    //   m_bufLen              : 1 byte
    //   flags (5 × bool)      : 5 bytes
    //   m_litChar + m_litPos  : 2 bytes
    //   ──────────────────────────────
    //   Total                 ≈ 54 bytes

    Callback  m_cb{nullptr};
    void*     m_ctx{nullptr};

    State     m_state{ST_VALUE};
    uint8_t   m_depth{0};
    uint8_t   m_stack[MAX_DEPTH];   ///< CT_OBJECT or CT_ARRAY per level

    char      m_buf[MAX_STRING];    ///< string / key / number accumulation
    uint8_t   m_bufLen{0};

    bool      m_escape{false};      ///< just saw '\' inside a string
    uint8_t   m_uEscape{0};         ///< >0 → consuming hex digits of \uXXXX
    bool      m_inKey{false};       ///< current string is a key, not a value

    bool      m_hasDot{false};      ///< number contains '.'
    bool      m_hasExp{false};      ///< number contains 'e'/'E'

    char      m_litChar{0};         ///< first char of literal: 't','f','n'
    uint8_t   m_litPos{0};          ///< chars consumed so far (including first)

    // ── Helpers ───────────────────────────────────────────────────────────────

    void fire(Event ev,
              const char* str  = nullptr,
              long        ival = 0L,
              float       fval = 0.0f)
    {
        if (m_cb) m_cb(ev, str, ival, fval, m_ctx);
    }

    void error(const char* msg)
    {
        m_state = ST_ERROR;
        fire(EV_ERROR, msg);
    }

    /** Append one char to m_buf, silently truncating on overflow. */
    void bufAppend(char c)
    {
        if (m_bufLen < static_cast<uint8_t>(MAX_STRING - 1u))
            m_buf[m_bufLen++] = c;
        // else: overflow – truncate; null terminator slot is preserved
    }

    void bufClear() { m_bufLen = 0; }

    /** Null-terminate and return the buffer. */
    const char* bufStr()
    {
        m_buf[m_bufLen] = '\0';
        return m_buf;
    }

    /** Push a container type.  Returns false (and sets error) if stack full. */
    bool pushContainer(Container ct)
    {
        if (m_depth >= MAX_DEPTH)
        {
            error("too deep");
            return false;
        }
        m_stack[m_depth++] = static_cast<uint8_t>(ct);
        return true;
    }

    /**
     * Transition to the correct state after a complete value has been emitted.
     * If depth==0 the parser is done; otherwise we expect a comma or bracket.
     */
    void afterValue()
    {
        if (m_depth == 0)
        {
            m_state = ST_DONE;
            fire(EV_DONE);
        }
        else
        {
            m_state = ST_COMMA;
        }
    }

    /** Parse the accumulated number buffer and fire the appropriate event. */
    void endNumber()
    {
        bufStr();

#ifndef MINIJSON_NO_FLOAT
        if (m_hasDot || m_hasExp)
            fire(EV_FLOAT, nullptr, 0L, static_cast<float>(atof(m_buf)));
        else
#endif
            fire(EV_INTEGER, nullptr, atol(m_buf));

        m_hasDot = false;
        m_hasExp = false;
    }

    static bool isWS(char c)
    {
        return c == ' ' || c == '\t' || c == '\r' || c == '\n';
    }

    static bool isDigit(char c)
    {
        return c >= '0' && c <= '9';
    }
};

// ── feed() ────────────────────────────────────────────────────────────────────
// Implemented outside the class body so the switch statement is not nested
// inside a class definition – some older avr-g++ versions are happier that way.

template<uint8_t D, uint8_t S>
bool Parser<D, S>::feed(char c)
{
    // Terminal states: ignore further input.
    if (m_state == ST_DONE || m_state == ST_ERROR)
        return false;

    switch (m_state)
    {

    // ────────────────────────────────────────────────────────────────────────
    // ST_VALUE – we are at a position where exactly one value is expected.
    // ────────────────────────────────────────────────────────────────────────
    case ST_VALUE:
    {
        if (isWS(c)) break;

        if (c == '{')
        {
            if (!pushContainer(CT_OBJECT)) break;
            fire(EV_OBJECT_START);
            m_state = ST_KEY;
        }
        else if (c == '[')
        {
            if (!pushContainer(CT_ARRAY)) break;
            fire(EV_ARRAY_START);
            // Stay in ST_VALUE: the next character is either ']' (empty array)
            // or the first element.
        }
        else if (c == ']')
        {
            // Empty array  [ ]
            if (m_depth == 0 || m_stack[m_depth - 1u] != CT_ARRAY)
            { error("unexpected ]"); break; }
            --m_depth;
            fire(EV_ARRAY_END);
            afterValue();
        }
        else if (c == '"')
        {
            bufClear();
            m_escape  = false;
            m_uEscape = 0;
            m_inKey   = false;
            m_state   = ST_STRING;
        }
        else if (c == '-' || isDigit(c))
        {
            bufClear();
            m_hasDot = false;
            m_hasExp = false;
            bufAppend(c);
            m_state  = ST_NUMBER;
        }
        else if (c == 't' || c == 'f' || c == 'n')
        {
            m_litChar = c;
            m_litPos  = 1;
            m_state   = ST_LITERAL;
        }
        else
        {
            error("bad value");
        }
        break;
    }

    // ────────────────────────────────────────────────────────────────────────
    // ST_KEY – inside an object, expecting a key string or '}'.
    // ────────────────────────────────────────────────────────────────────────
    case ST_KEY:
    {
        if (isWS(c)) break;

        if (c == '}')
        {
            // Empty or fully-consumed object.
            if (m_depth == 0 || m_stack[m_depth - 1u] != CT_OBJECT)
            { error("unexpected }"); break; }
            --m_depth;
            fire(EV_OBJECT_END);
            afterValue();
        }
        else if (c == '"')
        {
            bufClear();
            m_escape  = false;
            m_uEscape = 0;
            m_inKey   = true;
            m_state   = ST_STRING;
        }
        else
        {
            error("expected key");
        }
        break;
    }

    // ────────────────────────────────────────────────────────────────────────
    // ST_COLON – after a key, expecting ':'.
    // ────────────────────────────────────────────────────────────────────────
    case ST_COLON:
    {
        if (isWS(c)) break;

        if (c == ':')
            m_state = ST_VALUE;
        else
            error("expected ':'");
        break;
    }

    // ────────────────────────────────────────────────────────────────────────
    // ST_COMMA – after a value, expecting ',' or a closing bracket.
    // ────────────────────────────────────────────────────────────────────────
    case ST_COMMA:
    {
        if (isWS(c)) break;

        if (c == ',')
        {
            if (m_depth == 0) { error("unexpected ,"); break; }
            m_state = (m_stack[m_depth - 1u] == CT_OBJECT) ? ST_KEY : ST_VALUE;
        }
        else if (c == '}')
        {
            if (m_depth == 0 || m_stack[m_depth - 1u] != CT_OBJECT)
            { error("unexpected }"); break; }
            --m_depth;
            fire(EV_OBJECT_END);
            afterValue();
        }
        else if (c == ']')
        {
            if (m_depth == 0 || m_stack[m_depth - 1u] != CT_ARRAY)
            { error("unexpected ]"); break; }
            --m_depth;
            fire(EV_ARRAY_END);
            afterValue();
        }
        else
        {
            error("expected , or }]");
        }
        break;
    }

    // ────────────────────────────────────────────────────────────────────────
    // ST_STRING – inside a double-quoted string (key or value).
    // ────────────────────────────────────────────────────────────────────────
    case ST_STRING:
    {
        // Consuming the four hex digits of a \uXXXX escape: just discard them.
        if (m_uEscape > 0)
        {
            --m_uEscape;
            break;
        }

        if (m_escape)
        {
            m_escape = false;
            switch (c)
            {
                case '"':  bufAppend('"');  break;
                case '\\': bufAppend('\\'); break;
                case '/':  bufAppend('/');  break;
                case 'n':  bufAppend('\n'); break;
                case 'r':  bufAppend('\r'); break;
                case 't':  bufAppend('\t'); break;
                case 'b':  bufAppend('\b'); break;
                case 'f':  bufAppend('\f'); break;
                case 'u':
                    // Unicode escape: emit a replacement char, skip 4 hex digits.
                    bufAppend('?');
                    m_uEscape = 4;
                    break;
                default:
                    // Unknown escape – pass the character through.
                    bufAppend(c);
                    break;
            }
        }
        else if (c == '\\')
        {
            m_escape = true;
        }
        else if (c == '"')
        {
            // End of string.
            const char* s = bufStr();
            if (m_inKey)
            {
                fire(EV_KEY, s);
                m_state = ST_COLON;
            }
            else
            {
                fire(EV_STRING, s);
                afterValue();
            }
        }
        else if (static_cast<uint8_t>(c) >= 0x20u)
        {
            // Normal printable char (incl. UTF-8 multi-byte, passed through as-is).
            bufAppend(c);
        }
        // else: bare control character < 0x20 – silently skip.
        break;
    }

    // ────────────────────────────────────────────────────────────────────────
    // ST_NUMBER – accumulating digits / sign / dot / exponent.
    // ────────────────────────────────────────────────────────────────────────
    case ST_NUMBER:
    {
        if (isDigit(c))
        {
            bufAppend(c);
        }
        else if (c == '.' && !m_hasDot && !m_hasExp)
        {
            m_hasDot = true;
            bufAppend(c);
        }
        else if ((c == 'e' || c == 'E') && !m_hasExp)
        {
            m_hasExp = true;
            bufAppend(c);
        }
        else if ((c == '+' || c == '-')
                 && m_hasExp
                 && m_bufLen > 0
                 && (m_buf[m_bufLen - 1u] == 'e' || m_buf[m_bufLen - 1u] == 'E'))
        {
            bufAppend(c);
        }
        else
        {
            // This character ends the number.  Emit the number, advance state,
            // then re-process the character in the new state (one safe level of
            // recursion – never deeper).
            endNumber();
            afterValue();
            if (m_state != ST_ERROR && m_state != ST_DONE)
                feed(c);
        }
        break;
    }

    // ────────────────────────────────────────────────────────────────────────
    // ST_LITERAL – reading the tail of true / false / null.
    //
    // m_litChar: first char already consumed ('t', 'f', or 'n')
    // m_litPos : how many chars have been consumed so far (starts at 1)
    //
    // Expected tails (indexed from position 1):
    //   't' → r u e         (tailLen = 3, done at m_litPos == 4)
    //   'f' → a l s e       (tailLen = 4, done at m_litPos == 5)
    //   'n' → u l l         (tailLen = 3, done at m_litPos == 4)
    // ────────────────────────────────────────────────────────────────────────
    case ST_LITERAL:
    {
        // Determine the expected character at m_litPos without storing strings.
        char    expected = '\0';
        bool    done     = false;
        uint8_t pos      = m_litPos;   // current position (1-based)

        if (m_litChar == 't')
        {
            if      (pos == 1) expected = 'r';
            else if (pos == 2) expected = 'u';
            else if (pos == 3) { expected = 'e'; done = true; }
        }
        else if (m_litChar == 'f')
        {
            if      (pos == 1) expected = 'a';
            else if (pos == 2) expected = 'l';
            else if (pos == 3) expected = 's';
            else if (pos == 4) { expected = 'e'; done = true; }
        }
        else  // 'n'
        {
            if      (pos == 1) expected = 'u';
            else if (pos == 2) expected = 'l';
            else if (pos == 3) { expected = 'l'; done = true; }
        }

        if (expected == '\0' || c != expected)
        {
            error("bad literal");
            break;
        }

        if (done)
        {
            if      (m_litChar == 't') fire(EV_BOOL, nullptr, 1L);
            else if (m_litChar == 'f') fire(EV_BOOL, nullptr, 0L);
            else                       fire(EV_NULL);
            afterValue();
        }
        else
        {
            ++m_litPos;
        }
        break;
    }

    default:
        break;

    } // switch

    return m_state != ST_DONE && m_state != ST_ERROR;
}

} // namespace MiniJson
