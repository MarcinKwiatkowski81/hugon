# MiniJson

A streaming (SAX-style) JSON parser designed for **Arduino Uno** and any other
severely resource-constrained system.

## Design goals

| Goal | How |
|---|---|
| Zero heap allocations | All buffers are template parameters → stack/global |
| No `std::` | Pure C++11 portable subset |
| No exceptions, no RTTI | Safe with `-fno-exceptions -fno-rtti` |
| Tiny RAM footprint | `Parser<8,32>` ≈ **54 bytes** |
| Tiny Flash footprint | ≈ **1.6 KB** (≈ 3.2 KB with float support) |
| Feed one byte at a time | Ideal for `Serial.read()` and network streams |

---

## Quick start

```cpp
#include <MiniJson.h>

MiniJson::Parser<> parser;   // MAX_DEPTH=8, MAX_STRING=32

void onEvent(MiniJson::Event ev,
             const char* str, long ival, float fval, void* /*ctx*/) {
    if (ev == MiniJson::EV_KEY)     { Serial.print("key: "); Serial.println(str); }
    if (ev == MiniJson::EV_INTEGER) { Serial.print("int: "); Serial.println(ival); }
    if (ev == MiniJson::EV_STRING)  { Serial.print("str: "); Serial.println(str); }
    if (ev == MiniJson::EV_FLOAT)   { Serial.print("flt: "); Serial.println(fval); }
    if (ev == MiniJson::EV_BOOL)    { Serial.println(ival ? "true" : "false"); }
    if (ev == MiniJson::EV_NULL)    { Serial.println("null"); }
    if (ev == MiniJson::EV_ERROR)   { Serial.print("ERR: "); Serial.println(str); }
}

void setup() {
    Serial.begin(9600);
    parser.begin(onEvent);
    parser.feedProgmem(F("{\"id\":1,\"ok\":true}"));
    parser.flush();
}
void loop() {
    // Streaming from Serial:
    while (Serial.available())
        parser.feed((char)Serial.read());
}
```

---

## Events

| Event | `str` | `ival` | `fval` |
|---|---|---|---|
| `EV_OBJECT_START` | — | — | — |
| `EV_OBJECT_END`   | — | — | — |
| `EV_ARRAY_START`  | — | — | — |
| `EV_ARRAY_END`    | — | — | — |
| `EV_KEY`          | key text (null-terminated) | — | — |
| `EV_STRING`       | value text | — | — |
| `EV_INTEGER`      | — | value | — |
| `EV_FLOAT`        | — | — | value |
| `EV_BOOL`         | — | `1` (true) or `0` (false) | — |
| `EV_NULL`         | — | — | — |
| `EV_ERROR`        | short description | — | — |
| `EV_DONE`         | — | — | — |

> **`str` is only valid during the callback.** Copy it immediately if you need
> it to outlive the call.

---

## Template parameters

```cpp
MiniJson::Parser<MAX_DEPTH, MAX_STRING> parser;
```

| Parameter | Default | Effect |
|---|---|---|
| `MAX_DEPTH` | `8` | Maximum nesting depth. Each level costs **1 byte**. |
| `MAX_STRING` | `32` | Buffer size incl. null terminator. Strings longer than `MAX_STRING-1` are **silently truncated**. |

### RAM budget

```
sizeof(Parser<MAX_DEPTH, MAX_STRING>)
    = 4            // callback pointer + context pointer  (2 bytes each on AVR)
    + 2            // state + depth
    + MAX_DEPTH    // container-type stack
    + MAX_STRING   // string/key/number buffer
    + 1            // buffer length
    + 5            // boolean flags (escape, uEscape, inKey, hasDot, hasExp)
    + 2            // litChar + litPos
    ≈  14 + MAX_DEPTH + MAX_STRING  bytes
```

For the defaults: `14 + 8 + 32 = 54 bytes`.

---

## Compile-time switches

### `MINIJSON_NO_FLOAT`

Define before including the header (or in your build system) to disable
floating-point parsing.  All numbers — including those containing `.` or `e` —
are reported as `EV_INTEGER` via `atol()` truncation.

```cpp
#define MINIJSON_NO_FLOAT
#include <MiniJson.h>
```

**Benefit:** saves ≈ 1.5 KB of Flash on AVR by not pulling in the soft-float
library.  Highly recommended if your JSON contains only integer values.

---

## API reference

```cpp
// Initialise / re-initialise.  Clears state and registers callback.
void begin(Callback cb, void* ctx = nullptr);

// Reset parser state.  Does NOT clear the callback.
// Use this to parse a second document with the same Parser instance.
void reset();

// Feed one character.
// Returns true while more input is expected.
// Returns false (and has already fired EV_DONE or EV_ERROR) when finished.
bool feed(char c);

// Feed a null-terminated C-string character by character.
void feedString(const char* s);

// Feed a Flash string (Arduino only).  Reads directly from Flash;
// does not copy to SRAM.
void feedProgmem(const __FlashStringHelper* s);  // Arduino only

// Signal end-of-input.
// Required when the last top-level value is a bare number or literal
// not followed by whitespace.  Safe to call at any time; no-op in all
// states except ST_NUMBER and ST_LITERAL.
void flush();

// Status accessors.
bool    isDone()   const;
bool    hasError() const;
uint8_t depth()    const;   // current nesting level (0 = top level)
```

---

## String escapes

| Sequence | Result |
|---|---|
| `\"` | `"` |
| `\\` | `\` |
| `\/` | `/` |
| `\n` | newline |
| `\r` | carriage return |
| `\t` | tab |
| `\b` | backspace |
| `\f` | form feed |
| `\uXXXX` | `?` (replacement character; 4 hex digits consumed and discarded) |

AVR has no Unicode support, so `\uXXXX` emits a single `?`.  UTF-8 byte
sequences that arrive as raw bytes are passed through unchanged.

---

## End-of-input / `flush()`

For values *inside* a container, the closing `}` or `]` naturally terminates
any pending number or literal, so `flush()` is never needed:

```cpp
parser.feedString("[42]");      // ']' terminates the 42 → no flush needed
parser.feedString(R"({"n":7})");  // '}' terminates the 7 → no flush needed
```

For a **bare top-level** number or literal with no trailing character, call
`flush()` after the last byte:

```cpp
parser.feedString("42");   // nothing after '2' to terminate the number
parser.flush();            // → fires EV_INTEGER(42) + EV_DONE
```

---

## Parsing multiple documents

Call `reset()` (or `begin()` again) between documents:

```cpp
parser.begin(onEvent);

parser.feedString(R"({"a":1})");   // fires EV_DONE

parser.reset();                    // ready for next document
parser.feedString("[1,2,3]");      // fires EV_DONE again
```

---

## Examples

| Sketch | Description |
|---|---|
| `BasicParse` | Parse a hard-coded JSON string stored in Flash (PROGMEM). |
| `SerialStream` | Read JSON over Serial, print indented events as they arrive. |

---

## Desktop test suite

The library ships with a comprehensive test suite that runs on any Linux/macOS/
Windows machine with a C++11 compiler — no Arduino hardware required.

```bash
# From the repository root:
cmake -B build
cmake --build build
ctest --test-dir build -R test_minijson -V
```

The suite covers: all primitive types, objects, arrays, nested structures,
string escapes, number edge-cases, character-by-character streaming, buffer
overflow (truncation), stack overflow (depth limit), error recovery, `flush()`,
`reset()`, `MINIJSON_NO_FLOAT`, and exact event-ordering assertions.

---

## Limitations

* **No DOM tree** — this is intentional.  A DOM would require dynamic memory.
  Use the SAX callbacks to build only the fields you need.
* **`\uXXXX`** escapes emit `?`.  Full Unicode requires more RAM than an
  Uno provides.
* **Max string length** is `MAX_STRING - 1` characters.  Longer strings are
  silently truncated.  Increase `MAX_STRING` if you need longer values (each
  extra byte costs 1 byte of SRAM).
* **Single-threaded** — the parser is not re-entrant.  Do not call `feed()`
  from an ISR while it may also be called from `loop()`.
