/**
 * CommandDecoder.ino
 *
 * Practical example: receive a JSON command packet over Serial and act on it.
 *
 * The sketch listens for a single-line JSON object terminated by a newline.
 * It extracts up to three well-known fields, then drives hardware:
 *
 *   Field       Type     Action
 *   ─────────────────────────────────────────────────────────────
 *   "led"       bool     Turn the built-in LED (pin 13) on or off
 *   "pwm"       int      Write a PWM duty cycle (0-255) to pin 3
 *   "message"   string   Echo the message back to Serial
 *
 * Try sending (9600 baud, line ending = Newline):
 *
 *   {"led":true,"pwm":128,"message":"hello"}
 *   {"led":false,"pwm":0}
 *   {"message":"just a note"}
 *   {"pwm":200}
 *
 * Unknown keys are silently ignored, so the packet can carry extra fields
 * without breaking the decoder.
 *
 * ── Memory budget ────────────────────────────────────────────────────────────
 *   Parser<6,24>   ~36 bytes SRAM
 *   Cmd struct     ~28 bytes SRAM
 *   Input buffer   ~82 bytes SRAM (MAX_LINE chars)
 *   ─────────────────────────────
 *   Total          ~146 bytes     (out of 2048 on Uno)
 */

#include <MiniJson.h>

// ── Hardware pins ─────────────────────────────────────────────────────────────

static const uint8_t PIN_LED = 13;   // built-in LED
static const uint8_t PIN_PWM =  3;   // PWM-capable pin

// ── Command struct ────────────────────────────────────────────────────────────
// Populated field-by-field as the parser fires events.

struct Cmd
{
    // Parsed values
    bool    ledOn    {false};
    uint8_t pwmValue {0};
    char    message  [24];   // truncated to 23 chars + null

    // Validity flags: set to true when the key-value pair is seen
    bool    hasLed     {false};
    bool    hasPwm     {false};
    bool    hasMessage {false};

    // Key tracking: remembers which key the parser last saw
    enum Key : uint8_t { K_NONE, K_LED, K_PWM, K_MESSAGE } pendingKey {K_NONE};

    void reset()
    {
        ledOn      = false;
        pwmValue   = 0;
        message[0] = '\0';
        hasLed     = false;
        hasPwm     = false;
        hasMessage = false;
        pendingKey = K_NONE;
    }
};

// ── Parser callback ───────────────────────────────────────────────────────────

void onEvent(MiniJson::Event ev,
             const char*     str,
             long            ival,
             float        /* fval */,
             void*           ctx)
{
    Cmd* cmd = static_cast<Cmd*>(ctx);

    switch (ev)
    {
    // ── Key seen: remember which field comes next ─────────────────────────────
    case MiniJson::EV_KEY:
        if      (strcmp(str, "led")     == 0) cmd->pendingKey = Cmd::K_LED;
        else if (strcmp(str, "pwm")     == 0) cmd->pendingKey = Cmd::K_PWM;
        else if (strcmp(str, "message") == 0) cmd->pendingKey = Cmd::K_MESSAGE;
        else                                  cmd->pendingKey = Cmd::K_NONE;
        break;

    // ── Boolean value ─────────────────────────────────────────────────────────
    case MiniJson::EV_BOOL:
        if (cmd->pendingKey == Cmd::K_LED)
        {
            cmd->ledOn  = (ival != 0);
            cmd->hasLed = true;
        }
        cmd->pendingKey = Cmd::K_NONE;
        break;

    // ── Integer value ─────────────────────────────────────────────────────────
    case MiniJson::EV_INTEGER:
        if (cmd->pendingKey == Cmd::K_PWM)
        {
            // Clamp to valid PWM range.
            cmd->pwmValue = (ival < 0) ? 0 : (ival > 255) ? 255
                                           : static_cast<uint8_t>(ival);
            cmd->hasPwm   = true;
        }
        cmd->pendingKey = Cmd::K_NONE;
        break;

    // ── String value ──────────────────────────────────────────────────────────
    case MiniJson::EV_STRING:
        if (cmd->pendingKey == Cmd::K_MESSAGE)
        {
            // str is already truncated to MAX_STRING-1 chars by the parser.
            // Copy safely into our fixed buffer.
            strncpy(cmd->message, str, sizeof(cmd->message) - 1);
            cmd->message[sizeof(cmd->message) - 1] = '\0';
            cmd->hasMessage = true;
        }
        cmd->pendingKey = Cmd::K_NONE;
        break;

    // ── Parse error ───────────────────────────────────────────────────────────
    case MiniJson::EV_ERROR:
        Serial.print(F("[ERROR] "));
        Serial.println(str);
        break;

    // ── Any other event: reset pending key ───────────────────────────────────
    default:
        cmd->pendingKey = Cmd::K_NONE;
        break;
    }
}

// ── Apply a fully-parsed command to hardware ──────────────────────────────────

void applyCommand(const Cmd& cmd)
{
    if (cmd.hasLed)
    {
        digitalWrite(PIN_LED, cmd.ledOn ? HIGH : LOW);
        Serial.print(F("[LED] "));
        Serial.println(cmd.ledOn ? F("ON") : F("OFF"));
    }

    if (cmd.hasPwm)
    {
        analogWrite(PIN_PWM, cmd.pwmValue);
        Serial.print(F("[PWM] duty="));
        Serial.println(cmd.pwmValue);
    }

    if (cmd.hasMessage)
    {
        Serial.print(F("[MSG] "));
        Serial.println(cmd.message);
    }

    if (!cmd.hasLed && !cmd.hasPwm && !cmd.hasMessage)
    {
        Serial.println(F("[WARN] no recognised fields in packet"));
    }
}

// ── Globals ───────────────────────────────────────────────────────────────────

// Parser: MAX_DEPTH=6 (plenty for a flat command object),
//         MAX_STRING=24 (matches Cmd::message buffer).
MiniJson::Parser<6, 24> parser;
Cmd cmd;

// Line accumulation buffer: holds incoming bytes until '\n' is seen.
// 82 bytes covers a typical compact JSON command with some room to spare.
static const uint8_t MAX_LINE = 82;
static char   lineBuf[MAX_LINE];
static uint8_t lineLen = 0;

// ── setup / loop ──────────────────────────────────────────────────────────────

void setup()
{
    pinMode(PIN_LED, OUTPUT);
    pinMode(PIN_PWM, OUTPUT);

    Serial.begin(9600);
    while (!Serial) { /* wait for USB-CDC on Leonardo/Micro */ }

    Serial.println(F("CommandDecoder ready."));
    Serial.println(F("Send a JSON object followed by newline, e.g.:"));
    Serial.println(F(R"(  {"led":true,"pwm":128,"message":"hello"})"));

    cmd.reset();
    parser.begin(onEvent, &cmd);
}

void loop()
{
    while (Serial.available() > 0)
    {
        const char c = static_cast<char>(Serial.read());

        if (c == '\n' || c == '\r')
        {
            // End of line: parse whatever we have accumulated.
            if (lineLen == 0)
                continue;   // blank line – ignore

            lineBuf[lineLen] = '\0';

            // Feed the complete line to the parser.
            cmd.reset();
            parser.begin(onEvent, &cmd);
            parser.feedString(lineBuf);
            parser.flush();

            if (!parser.hasError())
                applyCommand(cmd);

            lineLen = 0;
        }
        else
        {
            // Accumulate the character.
            if (lineLen < MAX_LINE - 1)
            {
                lineBuf[lineLen++] = c;
            }
            else
            {
                // Line too long – discard and report.
                Serial.println(F("[ERROR] line too long, discarding"));
                lineLen = 0;
            }
        }
    }
}
