/**
 * SerialStream.ino
 *
 * Demonstrates MiniJson reading a JSON document character-by-character from
 * the Serial port.  The parser is fed each incoming byte as it arrives;
 * events are printed as they fire.
 *
 * Usage
 * ─────
 * 1. Upload the sketch to an Arduino Uno.
 * 2. Open the Serial Monitor at 9600 baud, line ending = Newline.
 * 3. Paste or type a JSON value and press Enter.
 *    Examples:
 *      {"led":1,"brightness":128}
 *      [1,2,3]
 *      true
 *
 * After EV_DONE the parser automatically resets so you can send another
 * document without pressing the reset button.
 *
 * Memory note
 * ───────────
 *   Parser<8,48> uses ~70 bytes of SRAM (keys/strings up to 47 chars).
 *   All Serial.print(F("...")) strings stay in Flash.
 */

#include <MiniJson.h>

// ── Shared context passed through the void* ───────────────────────────────────
// Keeps track of nesting depth so we can indent the output.
struct Ctx
{
    uint8_t indent{0};
};

// ── Helper: print 'indent' levels of two-space indentation ───────────────────
static void printIndent(uint8_t depth)
{
    for (uint8_t i = 0; i < depth; ++i)
        Serial.print(F("  "));
}

// ── Event handler ─────────────────────────────────────────────────────────────
void onEvent(MiniJson::Event ev,
             const char*     str,
             long            ival,
             float           fval,
             void*           ctx)
{
    auto* c = static_cast<Ctx*>(ctx);

    switch (ev)
    {
    case MiniJson::EV_OBJECT_START:
        printIndent(c->indent++);
        Serial.println(F("{"));
        break;

    case MiniJson::EV_OBJECT_END:
        printIndent(--c->indent);
        Serial.println(F("}"));
        break;

    case MiniJson::EV_ARRAY_START:
        printIndent(c->indent++);
        Serial.println(F("["));
        break;

    case MiniJson::EV_ARRAY_END:
        printIndent(--c->indent);
        Serial.println(F("]"));
        break;

    case MiniJson::EV_KEY:
        printIndent(c->indent);
        Serial.print(F("key: "));
        Serial.println(str);
        break;

    case MiniJson::EV_STRING:
        printIndent(c->indent);
        Serial.print(F("str: "));
        Serial.println(str);
        break;

    case MiniJson::EV_INTEGER:
        printIndent(c->indent);
        Serial.print(F("int: "));
        Serial.println(ival);
        break;

    case MiniJson::EV_FLOAT:
        printIndent(c->indent);
        Serial.print(F("flt: "));
        Serial.println(fval, 4);
        break;

    case MiniJson::EV_BOOL:
        printIndent(c->indent);
        Serial.println(ival ? F("true") : F("false"));
        break;

    case MiniJson::EV_NULL:
        printIndent(c->indent);
        Serial.println(F("null"));
        break;

    case MiniJson::EV_ERROR:
        Serial.print(F("*** parse error: "));
        Serial.println(str);
        c->indent = 0;
        break;

    case MiniJson::EV_DONE:
        Serial.println(F("--- done ---\n"));
        c->indent = 0;
        break;
    }
}

// ── Globals ───────────────────────────────────────────────────────────────────
// Keep MAX_STRING=48 to handle reasonably long keys/values.
// Reduce to 32 (or lower) if SRAM is tight.
MiniJson::Parser<8, 48> parser;
Ctx ctx;

void setup()
{
    Serial.begin(9600);
    while (!Serial) { /* wait for USB-serial on Leonardo/Micro */ }

    Serial.println(F("MiniJson SerialStream ready."));
    Serial.println(F("Send a JSON value followed by newline."));

    parser.begin(onEvent, &ctx);
}

void loop()
{
    while (Serial.available() > 0)
    {
        const char c = static_cast<char>(Serial.read());

        // Newline with no pending data = user just pressed Enter; ignore.
        if ((c == '\n' || c == '\r') && parser.depth() == 0 && !parser.isDone())
            continue;

        const bool more = parser.feed(c);

        // EV_DONE or EV_ERROR already fired inside feed().
        // Reset so the parser is ready for the next document.
        if (!more)
        {
            parser.flush();   // harmless but ensures bare numbers are emitted
            ctx.indent = 0;
            parser.begin(onEvent, &ctx);   // reset + re-attach callback
        }
    }
}
