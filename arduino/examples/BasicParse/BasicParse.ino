/**
 * BasicParse.ino
 *
 * Demonstrates MiniJson by parsing a JSON object stored in Flash (PROGMEM)
 * and printing each event to the Serial monitor.
 *
 * Expected Serial output (9600 baud):
 *
 *   MiniJson BasicParse
 *   {
 *   key: name
 *   str: Arduino
 *   key: version
 *   int: 2
 *   key: active
 *   bool: true
 *   key: ratio
 *   flt: 0.75
 *   key: tag
 *   null
 *   }
 *   --- done ---
 *
 * RAM usage for the parser object: ~54 bytes (MAX_DEPTH=8, MAX_STRING=32).
 */

#include <MiniJson.h>

// Keep the JSON string in Flash with the F() / PROGMEM mechanism so it does
// not consume precious SRAM.
static const char kJson[] PROGMEM =
    "{\"name\":\"Arduino\","
    "\"version\":2,"
    "\"active\":true,"
    "\"ratio\":0.75,"
    "\"tag\":null}";

// ── Event handler ─────────────────────────────────────────────────────────────
// Called once for every JSON token.  All parameters are passed; use only the
// ones that matter for each event code.

void onEvent(MiniJson::Event ev,
             const char*     str,
             long            ival,
             float           fval,
             void* /*ctx*/)
{
    switch (ev)
    {
    case MiniJson::EV_OBJECT_START:
        Serial.println(F("{"));
        break;

    case MiniJson::EV_OBJECT_END:
        Serial.println(F("}"));
        break;

    case MiniJson::EV_ARRAY_START:
        Serial.println(F("["));
        break;

    case MiniJson::EV_ARRAY_END:
        Serial.println(F("]"));
        break;

    case MiniJson::EV_KEY:
        Serial.print(F("key: "));
        Serial.println(str);
        break;

    case MiniJson::EV_STRING:
        Serial.print(F("str: "));
        Serial.println(str);
        break;

    case MiniJson::EV_INTEGER:
        Serial.print(F("int: "));
        Serial.println(ival);
        break;

    case MiniJson::EV_FLOAT:
        Serial.print(F("flt: "));
        Serial.println(fval, 2);
        break;

    case MiniJson::EV_BOOL:
        Serial.print(F("bool: "));
        Serial.println(ival ? F("true") : F("false"));
        break;

    case MiniJson::EV_NULL:
        Serial.println(F("null"));
        break;

    case MiniJson::EV_ERROR:
        Serial.print(F("ERROR: "));
        Serial.println(str);
        break;

    case MiniJson::EV_DONE:
        Serial.println(F("--- done ---"));
        break;
    }
}

// ── Parser (global so it does not consume stack space) ────────────────────────
MiniJson::Parser<> parser;

void setup()
{
    Serial.begin(9600);
    Serial.println(F("MiniJson BasicParse"));

    parser.begin(onEvent);

    // feedProgmem() reads directly from Flash without copying to SRAM first.
    parser.feedProgmem(F("{\"name\":\"Arduino\","
                          "\"version\":2,"
                          "\"active\":true,"
                          "\"ratio\":0.75,"
                          "\"tag\":null}"));

    // flush() is only needed for bare numbers/literals with no trailing
    // delimiter.  It is harmless to call it here.
    parser.flush();
}

void loop()
{
    // Nothing to do after setup() – the whole document is already parsed.
}
