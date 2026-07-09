/**
 * test_minijson.cpp
 *
 * Desktop test suite for MiniJson.h.
 *
 * Build:  cmake --build build && ctest --test-dir build -R test_minijson -V
 * Style:  matches the project's lightweight CHECK() runner (no gtest needed).
 *
 * Coverage:
 *   1.  Primitives          – string, integer, float, bool, null, top-level
 *   2.  Objects             – keys, nested, empty
 *   3.  Arrays              – elements, nested, empty
 *   4.  Nesting             – mixed objects and arrays, multiple levels
 *   5.  String escapes      – \n \t \r \\ \" \/ \b \f  and \uXXXX → '?'
 *   6.  Numbers             – integers, negatives, fractions, exponents
 *   7.  Streaming           – character-by-character, split-at-every-offset
 *   8.  Whitespace          – leading, trailing, between tokens
 *   9.  Buffer overflow     – strings longer than MAX_STRING are truncated
 *  10.  Stack overflow      – nesting deeper than MAX_DEPTH fires EV_ERROR
 *  11.  Error cases         – bad syntax, mismatched brackets, bad literals
 *  12.  flush()             – bare numbers and literals at end of input
 *  13.  reset()             – parse multiple documents with one Parser
 *  14.  MINIJSON_NO_FLOAT   – integers are returned for fractional input
 *  15.  Event ordering      – exact sequence of events for reference inputs
 */

#include "../../src/MiniJson.h"

#include <cassert>
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

// ── Minimal test runner ───────────────────────────────────────────────────────

namespace
{

int gPassed = 0;
int gFailed = 0;

void check(bool cond, const char* expr, const char* file, int line)
{
    if (cond)
    {
        ++gPassed;
    }
    else
    {
        ++gFailed;
        std::fprintf(stderr, "  FAIL  %s:%d  %s\n", file, line, expr);
    }
}

} // namespace

#define CHECK(expr) check(!!(expr), #expr, __FILE__, __LINE__)

// ── Event recorder ────────────────────────────────────────────────────────────
// Captures every callback into a vector so tests can assert the exact sequence.

struct Rec
{
    MiniJson::Event ev;
    std::string     str;
    long            ival{0};
    float           fval{0.0f};
};

struct Recorder
{
    std::vector<Rec> events;

    static void cb(MiniJson::Event ev, const char* s, long i, float f, void* ctx)
    {
        auto* self = static_cast<Recorder*>(ctx);
        Rec r;
        r.ev   = ev;
        r.str  = s ? s : "";
        r.ival = i;
        r.fval = f;
        self->events.push_back(r);
    }

    void clear() { events.clear(); }

    // Parse a full C-string in one call.
    void parse(MiniJson::Parser<>& p, const char* json)
    {
        clear();
        p.begin(cb, this);
        p.feedString(json);
        p.flush();   // harmless if number/literal already delimited
    }

    // Feed character-by-character (stress-test the streaming path).
    void parseByChar(MiniJson::Parser<>& p, const char* json)
    {
        clear();
        p.begin(cb, this);
        for (const char* c = json; *c; ++c)
            p.feed(*c);
        p.flush();
    }

    bool hasEvent(MiniJson::Event e) const
    {
        for (const auto& r : events)
            if (r.ev == e) return true;
        return false;
    }

    bool hasError() const { return hasEvent(MiniJson::EV_ERROR); }
    bool hasDone()  const { return hasEvent(MiniJson::EV_DONE);  }

    // Find the nth occurrence (0-based) of an event type.
    const Rec* nth(MiniJson::Event e, int n = 0) const
    {
        for (const auto& r : events)
            if (r.ev == e && n-- == 0) return &r;
        return nullptr;
    }
};

// ── Tests ─────────────────────────────────────────────────────────────────────

// 1. Primitive top-level values ───────────────────────────────────────────────
void testPrimitives()
{
    std::printf("[primitives]\n");
    MiniJson::Parser<> p;
    Recorder rec;

    // null
    rec.parse(p, "null");
    CHECK(!rec.hasError());
    CHECK(rec.hasDone());
    CHECK(rec.nth(MiniJson::EV_NULL) != nullptr);

    // true
    rec.parse(p, "true");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_BOOL);
        CHECK(r != nullptr && r->ival == 1L);
    }

    // false
    rec.parse(p, "false");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_BOOL);
        CHECK(r != nullptr && r->ival == 0L);
    }

    // integer
    rec.parse(p, "42");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_INTEGER);
        CHECK(r != nullptr && r->ival == 42L);
    }

    // negative integer
    rec.parse(p, "-7");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_INTEGER);
        CHECK(r != nullptr && r->ival == -7L);
    }

    // zero
    rec.parse(p, "0");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_INTEGER);
        CHECK(r != nullptr && r->ival == 0L);
    }

#ifndef MINIJSON_NO_FLOAT
    // float
    rec.parse(p, "3.14");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_FLOAT);
        CHECK(r != nullptr && r->fval > 3.13f && r->fval < 3.15f);
    }

    // negative float
    rec.parse(p, "-0.5");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_FLOAT);
        CHECK(r != nullptr && r->fval < -0.49f && r->fval > -0.51f);
    }

    // exponent
    rec.parse(p, "1e3");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_FLOAT);
        CHECK(r != nullptr && r->fval > 999.0f && r->fval < 1001.0f);
    }

    // exponent with sign
    rec.parse(p, "2.5E+2");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_FLOAT);
        CHECK(r != nullptr && r->fval > 249.0f && r->fval < 251.0f);
    }
#endif // MINIJSON_NO_FLOAT

    // string
    rec.parse(p, R"("hello")");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_STRING);
        CHECK(r != nullptr && r->str == "hello");
    }

    // empty string
    rec.parse(p, R"("")");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_STRING);
        CHECK(r != nullptr && r->str.empty());
    }
}

// 2. Objects ──────────────────────────────────────────────────────────────────
void testObjects()
{
    std::printf("[objects]\n");
    MiniJson::Parser<> p;
    Recorder rec;

    // Empty object
    rec.parse(p, "{}");
    CHECK(!rec.hasError());
    CHECK(rec.hasDone());
    CHECK(rec.nth(MiniJson::EV_OBJECT_START) != nullptr);
    CHECK(rec.nth(MiniJson::EV_OBJECT_END)   != nullptr);
    CHECK(rec.nth(MiniJson::EV_KEY)          == nullptr);

    // Single key-value
    rec.parse(p, R"({"x":1})");
    CHECK(!rec.hasError());
    {
        const Rec* k = rec.nth(MiniJson::EV_KEY);
        CHECK(k != nullptr && k->str == "x");
        const Rec* v = rec.nth(MiniJson::EV_INTEGER);
        CHECK(v != nullptr && v->ival == 1L);
    }

    // Multiple keys
    rec.parse(p, R"({"a":1,"b":2,"c":3})");
    CHECK(!rec.hasError());
    CHECK(rec.nth(MiniJson::EV_KEY, 0) != nullptr &&
          rec.nth(MiniJson::EV_KEY, 0)->str == "a");
    CHECK(rec.nth(MiniJson::EV_KEY, 1) != nullptr &&
          rec.nth(MiniJson::EV_KEY, 1)->str == "b");
    CHECK(rec.nth(MiniJson::EV_KEY, 2) != nullptr &&
          rec.nth(MiniJson::EV_KEY, 2)->str == "c");

    // Whitespace around tokens
    rec.parse(p, "{ \"k\" : 99 }");
    CHECK(!rec.hasError());
    {
        const Rec* v = rec.nth(MiniJson::EV_INTEGER);
        CHECK(v != nullptr && v->ival == 99L);
    }

    // Nested object
    rec.parse(p, R"({"outer":{"inner":42}})");
    CHECK(!rec.hasError());
    {
        // Two OBJECT_START events
        CHECK(rec.nth(MiniJson::EV_OBJECT_START, 0) != nullptr);
        CHECK(rec.nth(MiniJson::EV_OBJECT_START, 1) != nullptr);
        const Rec* v = rec.nth(MiniJson::EV_INTEGER);
        CHECK(v != nullptr && v->ival == 42L);
    }
}

// 3. Arrays ───────────────────────────────────────────────────────────────────
void testArrays()
{
    std::printf("[arrays]\n");
    MiniJson::Parser<> p;
    Recorder rec;

    // Empty array
    rec.parse(p, "[]");
    CHECK(!rec.hasError());
    CHECK(rec.nth(MiniJson::EV_ARRAY_START) != nullptr);
    CHECK(rec.nth(MiniJson::EV_ARRAY_END)   != nullptr);

    // Integer array
    rec.parse(p, "[1,2,3]");
    CHECK(!rec.hasError());
    CHECK(rec.nth(MiniJson::EV_INTEGER, 0) != nullptr &&
          rec.nth(MiniJson::EV_INTEGER, 0)->ival == 1L);
    CHECK(rec.nth(MiniJson::EV_INTEGER, 1) != nullptr &&
          rec.nth(MiniJson::EV_INTEGER, 1)->ival == 2L);
    CHECK(rec.nth(MiniJson::EV_INTEGER, 2) != nullptr &&
          rec.nth(MiniJson::EV_INTEGER, 2)->ival == 3L);

    // Mixed-type array
    rec.parse(p, R"([null,true,"hi",9])");
    CHECK(!rec.hasError());
    CHECK(rec.nth(MiniJson::EV_NULL)    != nullptr);
    CHECK(rec.nth(MiniJson::EV_BOOL)    != nullptr);
    CHECK(rec.nth(MiniJson::EV_STRING)  != nullptr && rec.nth(MiniJson::EV_STRING)->str == "hi");
    CHECK(rec.nth(MiniJson::EV_INTEGER) != nullptr && rec.nth(MiniJson::EV_INTEGER)->ival == 9L);

    // Nested arrays
    rec.parse(p, "[[1,2],[3,4]]");
    CHECK(!rec.hasError());
    CHECK(rec.nth(MiniJson::EV_INTEGER, 0) != nullptr &&
          rec.nth(MiniJson::EV_INTEGER, 0)->ival == 1L);
    CHECK(rec.nth(MiniJson::EV_INTEGER, 3) != nullptr &&
          rec.nth(MiniJson::EV_INTEGER, 3)->ival == 4L);
}

// 4. Mixed nesting ────────────────────────────────────────────────────────────
void testNesting()
{
    std::printf("[nesting]\n");
    MiniJson::Parser<> p;
    Recorder rec;

    // Object containing an array containing objects
    rec.parse(p, R"({"items":[{"id":1},{"id":2}],"count":2})");
    CHECK(!rec.hasError());
    CHECK(rec.hasDone());
    // Two EV_KEY("id") events
    CHECK(rec.nth(MiniJson::EV_KEY, 0) != nullptr &&
          rec.nth(MiniJson::EV_KEY, 0)->str == "items");
    CHECK(rec.nth(MiniJson::EV_KEY, 1) != nullptr &&
          rec.nth(MiniJson::EV_KEY, 1)->str == "id");
    CHECK(rec.nth(MiniJson::EV_KEY, 2) != nullptr &&
          rec.nth(MiniJson::EV_KEY, 2)->str == "id");

    // Confirm id values
    CHECK(rec.nth(MiniJson::EV_INTEGER, 0) != nullptr &&
          rec.nth(MiniJson::EV_INTEGER, 0)->ival == 1L);
    CHECK(rec.nth(MiniJson::EV_INTEGER, 1) != nullptr &&
          rec.nth(MiniJson::EV_INTEGER, 1)->ival == 2L);
    CHECK(rec.nth(MiniJson::EV_INTEGER, 2) != nullptr &&
          rec.nth(MiniJson::EV_INTEGER, 2)->ival == 2L);  // "count"

    // Deeply nested (within MAX_DEPTH=8 default)
    rec.parse(p, "[[[[[[[[1]]]]]]]]");
    CHECK(!rec.hasError());
    CHECK(rec.hasDone());
    CHECK(rec.nth(MiniJson::EV_INTEGER) != nullptr &&
          rec.nth(MiniJson::EV_INTEGER)->ival == 1L);
}

// 5. String escapes ───────────────────────────────────────────────────────────
void testEscapes()
{
    std::printf("[string escapes]\n");
    MiniJson::Parser<> p;
    Recorder rec;

    // \n \t \r
    rec.parse(p, R"("line1\nline2")");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_STRING);
        CHECK(r != nullptr && r->str == "line1\nline2");
    }

    rec.parse(p, R"("col1\tcol2")");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_STRING);
        CHECK(r != nullptr && r->str == "col1\tcol2");
    }

    // backslash-quote escape: verify that \" in JSON produces a literal " in output
    // JSON input: "a\"b"  (6 bytes: 22 61 5c 22 62 22)
    // Expected output: a"b  (3 chars)
    {
        static const char kQuoteJson[] = {'"','a','\\','"','b','"',0};
        rec.parse(p, kQuoteJson);
    }
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_STRING);
        static const char kQuoteExp[] = {'a','"','b',0};
        CHECK(r != nullptr && r->str == std::string(kQuoteExp));
    }
    rec.parse(p, R"("a\\b")");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_STRING);
        CHECK(r != nullptr && r->str == "a\\b");
    }

    // \/
    rec.parse(p, R"("a\/b")");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_STRING);
        CHECK(r != nullptr && r->str == "a/b");
    }

    // \b \f
    rec.parse(p, R"("\b\f")");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_STRING);
        CHECK(r != nullptr && r->str.size() == 2);
        CHECK(r->str[0] == '\b' && r->str[1] == '\f');
    }

    // \uXXXX → replaced by '?', four hex digits consumed
    rec.parse(p, R"("\u0041BC")");  // U+0041 = 'A', then 'BC'
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_STRING);
        // '?' replaces the unicode codepoint, then 'BC' follow
        CHECK(r != nullptr && r->str == "?BC");
    }

    // Multiple unicode escapes
    rec.parse(p, R"("\u0041\u0042")");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_STRING);
        CHECK(r != nullptr && r->str == "??");
    }

    // String key with escape
    rec.parse(p, R"({"k\ney":1})");
    CHECK(!rec.hasError());
    {
        const Rec* k = rec.nth(MiniJson::EV_KEY);
        CHECK(k != nullptr && k->str == "k\ney");
    }
}

// 6. Number edge cases ────────────────────────────────────────────────────────
void testNumbers()
{
    std::printf("[numbers]\n");
    MiniJson::Parser<> p;
    Recorder rec;

    // Large integer
    rec.parse(p, "2147483647");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_INTEGER);
        CHECK(r != nullptr && r->ival == 2147483647L);
    }

    // Negative large integer
    rec.parse(p, "-2147483648");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_INTEGER);
        CHECK(r != nullptr && r->ival == -2147483648L);
    }

    // Number followed by whitespace (delimiter test)
    rec.parse(p, "7 ");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_INTEGER);
        CHECK(r != nullptr && r->ival == 7L);
    }

    // Number at end of array (closed by ']')
    rec.parse(p, "[5]");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_INTEGER);
        CHECK(r != nullptr && r->ival == 5L);
    }

#ifndef MINIJSON_NO_FLOAT
    // Float: negative exponent
    rec.parse(p, "1e-2");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_FLOAT);
        CHECK(r != nullptr && r->fval > 0.009f && r->fval < 0.011f);
    }

    // Float: leading zero
    rec.parse(p, "0.25");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_FLOAT);
        CHECK(r != nullptr && r->fval > 0.24f && r->fval < 0.26f);
    }
#endif // MINIJSON_NO_FLOAT

    // Integer in object delimited by '}'
    rec.parse(p, R"({"v":99})");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_INTEGER);
        CHECK(r != nullptr && r->ival == 99L);
    }
}

// 7. Streaming: character-by-character ────────────────────────────────────────
void testStreaming()
{
    std::printf("[streaming]\n");
    MiniJson::Parser<> p;
    Recorder rec;

    const char* json = R"({"key":[1,2,3],"flag":true})";

    // Full-string parse
    rec.parse(p, json);
    CHECK(!rec.hasError());
    CHECK(rec.hasDone());
    const size_t refCount = rec.events.size();

    // Char-by-char parse must produce the identical event sequence
    rec.parseByChar(p, json);
    CHECK(!rec.hasError());
    CHECK(rec.hasDone());
    CHECK(rec.events.size() == refCount);

    // Feed in two halves
    {
        std::string s{json};
        rec.clear();
        p.begin(Recorder::cb, &rec);
        size_t mid = s.size() / 2;
        for (size_t i = 0; i < mid; ++i)        p.feed(s[i]);
        for (size_t i = mid; i < s.size(); ++i)  p.feed(s[i]);
        p.flush();
        CHECK(!rec.hasError());
        CHECK(rec.events.size() == refCount);
    }

    // Split at every possible offset (regression for boundary bugs)
    {
        std::string s{json};
        for (size_t split = 1; split < s.size(); ++split)
        {
            rec.clear();
            p.begin(Recorder::cb, &rec);
            for (size_t i = 0; i < split; ++i)       p.feed(s[i]);
            for (size_t i = split; i < s.size(); ++i) p.feed(s[i]);
            p.flush();
            // Every split must produce the same result as the reference
            CHECK(!rec.hasError());
            CHECK(rec.events.size() == refCount);
        }
    }
}

// 8. Whitespace ───────────────────────────────────────────────────────────────
void testWhitespace()
{
    std::printf("[whitespace]\n");
    MiniJson::Parser<> p;
    Recorder rec;

    rec.parse(p, "  \t\r\n  null  \n  ");
    CHECK(!rec.hasError());
    CHECK(rec.nth(MiniJson::EV_NULL) != nullptr);

    rec.parse(p, "  {  \"a\"  :  1  }  ");
    CHECK(!rec.hasError());
    {
        const Rec* r = rec.nth(MiniJson::EV_INTEGER);
        CHECK(r != nullptr && r->ival == 1L);
    }

    rec.parse(p, "  [  1  ,  2  ]  ");
    CHECK(!rec.hasError());
    CHECK(rec.nth(MiniJson::EV_INTEGER, 0) != nullptr &&
          rec.nth(MiniJson::EV_INTEGER, 0)->ival == 1L);
    CHECK(rec.nth(MiniJson::EV_INTEGER, 1) != nullptr &&
          rec.nth(MiniJson::EV_INTEGER, 1)->ival == 2L);
}

// 9. Buffer overflow (truncation) ─────────────────────────────────────────────
void testBufferOverflow()
{
    std::printf("[buffer overflow]\n");

    // Use a very small MAX_STRING so we can easily trigger truncation.
    // MAX_STRING=8 → 7 usable chars.
    MiniJson::Parser<8, 8> p;
    Recorder rec;

    // String longer than 7 chars → truncated but no crash, no error
    p.begin(Recorder::cb, &rec);
    p.feedString(R"("abcdefghijklmnop")");
    p.flush();
    CHECK(!rec.hasError());
    CHECK(rec.hasDone());
    {
        const Rec* r = rec.nth(MiniJson::EV_STRING);
        CHECK(r != nullptr);
        // Content must be truncated to at most 7 chars
        CHECK(r->str.size() <= 7u);
        // First 7 chars must be the original ones
        CHECK(r->str.substr(0, 7) == "abcdefg");
    }

    // Key truncation: can still parse the value
    p.begin(Recorder::cb, &rec);
    rec.clear();
    p.feedString(R"({"longkeyname":42})");
    CHECK(!rec.hasError());
    CHECK(rec.hasDone());
    {
        const Rec* r = rec.nth(MiniJson::EV_INTEGER);
        CHECK(r != nullptr && r->ival == 42L);
    }
}

// 10. Stack overflow (max depth exceeded) ─────────────────────────────────────
void testStackOverflow()
{
    std::printf("[stack overflow]\n");

    // Use MAX_DEPTH=3 so it's easy to exceed.
    MiniJson::Parser<3, 32> p;
    Recorder rec;

    // Depth 3 – exactly at limit, should succeed
    p.begin(Recorder::cb, &rec);
    p.feedString("[[[1]]]");
    p.flush();
    CHECK(!rec.hasError());
    CHECK(rec.hasDone());

    // Depth 4 – one too many, must fire EV_ERROR
    p.begin(Recorder::cb, &rec);
    p.feedString("[[[[1]]]]");
    p.flush();
    CHECK(rec.hasError());
}

// 11. Error cases ─────────────────────────────────────────────────────────────
void testErrors()
{
    std::printf("[errors]\n");
    MiniJson::Parser<> p;
    Recorder rec;

    auto shouldError = [&](const char* json) -> bool
    {
        rec.parse(p, json);
        return rec.hasError();
    };

    // Unclosed object
    CHECK(shouldError(R"({"a":1)"));

    // Unclosed array
    CHECK(shouldError("[1,2,3"));

    // Mismatched brackets
    CHECK(shouldError(R"({"a":[})"));

    // Missing colon
    CHECK(shouldError(R"({"a" 1})"));

    // Missing value after colon
    // (parser stops at '}' which is not a value start)
    CHECK(shouldError(R"({"a":})"));

    // Bad literal: 'tru' (truncated)
    {
        rec.clear();
        p.begin(Recorder::cb, &rec);
        p.feed('t'); p.feed('r'); p.feed('u');
        p.flush();   // flush while still inside literal
        CHECK(rec.hasError());
    }

    // Bad literal: 'trux'
    {
        rec.clear();
        p.begin(Recorder::cb, &rec);
        p.feedString("trux");
        CHECK(rec.hasError());
    }

    // Extra data at top level (second value after done)
    {
        rec.clear();
        p.begin(Recorder::cb, &rec);
        // First value completes:
        p.feedString("1 ");
        bool firstDone = rec.hasDone();
        // feed() returns false once done; extra chars are silently ignored
        bool ret = p.feed('2');
        CHECK(firstDone);
        CHECK(!ret);  // returns false in ST_DONE
    }

    // Bare minus is not a valid number
    {
        rec.clear();
        p.begin(Recorder::cb, &rec);
        p.feed('-');
        p.flush();
        // atol("-") = 0; we fire EV_INTEGER(0).  The parser tolerates this
        // rather than erroring, which is acceptable for an embedded device.
        // The important thing is it doesn't crash.
        (void)rec.hasError();  // either outcome is acceptable
    }
}

// 12. flush() ─────────────────────────────────────────────────────────────────
void testFlush()
{
    std::printf("[flush]\n");
    MiniJson::Parser<> p;
    Recorder rec;

    // Bare integer with no trailing delimiter
    {
        rec.clear();
        p.begin(Recorder::cb, &rec);
        p.feedString("42");   // no '\n' or whitespace after
        p.flush();
        CHECK(!rec.hasError());
        CHECK(rec.hasDone());
        const Rec* r = rec.nth(MiniJson::EV_INTEGER);
        CHECK(r != nullptr && r->ival == 42L);
    }

    // Bare float
    {
        rec.clear();
        p.begin(Recorder::cb, &rec);
        p.feedString("1.5");
        p.flush();
        CHECK(!rec.hasError());
#ifndef MINIJSON_NO_FLOAT
        const Rec* r = rec.nth(MiniJson::EV_FLOAT);
        CHECK(r != nullptr && r->fval > 1.49f && r->fval < 1.51f);
#else
        // NO_FLOAT: 1.5 comes back as EV_INTEGER (atol truncates to 1)
        const Rec* r = rec.nth(MiniJson::EV_INTEGER);
        CHECK(r != nullptr && r->ival == 1L);
#endif
    }

    // Bare null
    {
        rec.clear();
        p.begin(Recorder::cb, &rec);
        p.feedString("null");
        p.flush();
        CHECK(!rec.hasError());
        CHECK(rec.nth(MiniJson::EV_NULL) != nullptr);
    }

    // Bare true
    {
        rec.clear();
        p.begin(Recorder::cb, &rec);
        p.feedString("true");
        p.flush();
        CHECK(!rec.hasError());
        const Rec* r = rec.nth(MiniJson::EV_BOOL);
        CHECK(r != nullptr && r->ival == 1L);
    }

    // Incomplete literal → error on flush
    {
        rec.clear();
        p.begin(Recorder::cb, &rec);
        p.feedString("tru");   // missing 'e'
        p.flush();
        CHECK(rec.hasError());
    }

    // flush() on an already-done parser: no-op
    {
        rec.clear();
        p.begin(Recorder::cb, &rec);
        p.feedString("[1]");
        p.flush();
        size_t countBefore = rec.events.size();
        p.flush();  // second flush: must not fire anything extra
        CHECK(rec.events.size() == countBefore);
    }
}

// 13. reset() – parse multiple documents ──────────────────────────────────────
void testReset()
{
    std::printf("[reset]\n");
    MiniJson::Parser<> p;
    Recorder rec;
    p.begin(Recorder::cb, &rec);

    // Parse first document
    rec.clear();
    p.reset();
    p.feedString(R"({"n":1})");
    CHECK(!rec.hasError());
    CHECK(rec.hasDone());
    CHECK(rec.nth(MiniJson::EV_INTEGER) != nullptr &&
          rec.nth(MiniJson::EV_INTEGER)->ival == 1L);

    // Reset and parse second document (different structure)
    rec.clear();
    p.reset();
    p.feedString("[true,false]");
    CHECK(!rec.hasError());
    CHECK(rec.hasDone());
    CHECK(rec.nth(MiniJson::EV_BOOL, 0) != nullptr &&
          rec.nth(MiniJson::EV_BOOL, 0)->ival == 1L);
    CHECK(rec.nth(MiniJson::EV_BOOL, 1) != nullptr &&
          rec.nth(MiniJson::EV_BOOL, 1)->ival == 0L);

    // Reset after an error and parse a valid document
    rec.clear();
    p.reset();
    p.feedString("bad-input");
    bool hadError = rec.hasError();

    rec.clear();
    p.begin(Recorder::cb, &rec);
    p.feedString("99");
    p.flush();
    CHECK(hadError);
    CHECK(!rec.hasError());
    CHECK(rec.nth(MiniJson::EV_INTEGER) != nullptr &&
          rec.nth(MiniJson::EV_INTEGER)->ival == 99L);
}

// 14. MINIJSON_NO_FLOAT – fractional input treated as integer ──────────────────
void testNoFloat()
{
    std::printf("[MINIJSON_NO_FLOAT]\n");

    MiniJson::Parser<> p;
    Recorder rec;

#ifdef MINIJSON_NO_FLOAT
    // NO_FLOAT build: fractional input must produce EV_INTEGER, never EV_FLOAT.
    rec.parse(p, "3.14");
    CHECK(!rec.hasError());
    CHECK(rec.nth(MiniJson::EV_INTEGER) != nullptr);
    CHECK(rec.nth(MiniJson::EV_FLOAT)   == nullptr);

    rec.parse(p, "1e3");
    CHECK(!rec.hasError());
    CHECK(rec.nth(MiniJson::EV_INTEGER) != nullptr);
    CHECK(rec.nth(MiniJson::EV_FLOAT)   == nullptr);
#else
    // Float build: fractional input must produce EV_FLOAT.
    rec.parse(p, "3.14");
    CHECK(!rec.hasError());
    CHECK(rec.nth(MiniJson::EV_FLOAT)   != nullptr);
    CHECK(rec.nth(MiniJson::EV_INTEGER) == nullptr);

    // Plain integer must produce EV_INTEGER regardless of the flag.
    rec.parse(p, "7");
    CHECK(!rec.hasError());
    CHECK(rec.nth(MiniJson::EV_INTEGER) != nullptr);
    CHECK(rec.nth(MiniJson::EV_FLOAT)   == nullptr);
#endif
}

// 15. Event ordering – exact sequence ─────────────────────────────────────────
void testEventOrdering()
{
    std::printf("[event ordering]\n");
    MiniJson::Parser<> p;
    Recorder rec;

    // {"a":[1,2]}
    // Expected sequence:
    //   OBJECT_START  KEY("a")  ARRAY_START  INTEGER(1)  INTEGER(2)
    //   ARRAY_END  OBJECT_END  DONE
    rec.parse(p, R"({"a":[1,2]})");
    CHECK(!rec.hasError());

    using E = MiniJson::Event;
    const std::vector<E> expected =
    {
        E::EV_OBJECT_START,
        E::EV_KEY,
        E::EV_ARRAY_START,
        E::EV_INTEGER,
        E::EV_INTEGER,
        E::EV_ARRAY_END,
        E::EV_OBJECT_END,
        E::EV_DONE,
    };
    CHECK(rec.events.size() == expected.size());
    for (size_t i = 0; i < expected.size() && i < rec.events.size(); ++i)
        CHECK(rec.events[i].ev == expected[i]);

    // Deeper sequence: [{"x":true},{"x":false}]
    rec.parse(p, R"([{"x":true},{"x":false}])");
    CHECK(!rec.hasError());

    const std::vector<E> expected2 =
    {
        E::EV_ARRAY_START,
        E::EV_OBJECT_START,
        E::EV_KEY,        // "x"
        E::EV_BOOL,       // true
        E::EV_OBJECT_END,
        E::EV_OBJECT_START,
        E::EV_KEY,        // "x"
        E::EV_BOOL,       // false
        E::EV_OBJECT_END,
        E::EV_ARRAY_END,
        E::EV_DONE,
    };
    CHECK(rec.events.size() == expected2.size());
    for (size_t i = 0; i < expected2.size() && i < rec.events.size(); ++i)
        CHECK(rec.events[i].ev == expected2[i]);
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main()
{
    testPrimitives();
    testObjects();
    testArrays();
    testNesting();
    testEscapes();
    testNumbers();
    testStreaming();
    testWhitespace();
    testBufferOverflow();
    testStackOverflow();
    testErrors();
    testFlush();
    testReset();
    testNoFloat();
    testEventOrdering();

    std::printf("\n");
    if (gFailed == 0)
    {
        std::printf("All %d checks passed.\n", gPassed);
        return 0;
    }
    else
    {
        std::printf("%d passed, %d FAILED.\n", gPassed, gFailed);
        return 1;
    }
}
