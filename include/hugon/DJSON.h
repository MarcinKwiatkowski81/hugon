#pragma once

#include <hugon/Hugon.h>

#include <memory>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// ── JsonValue ─────────────────────────────────────────────────────────────────
// DOM node that can hold any JSON value.  All five JSON types are stored inline;
// only the member matching `type` carries meaningful data.

struct JsonValue {
    enum class Type { Null, Bool, Number, String, Array, Object };

    using Array  = std::vector<JsonValue>;
    using Object = std::vector<std::pair<std::string, JsonValue>>;

    Type        type   {Type::Null};
    bool        boolean{false};
    double      number {0.0};
    std::string string {};
    Array       array  {};
    Object      object {};

    // ── Type predicates ───────────────────────────────────────────────────────
    [[nodiscard]] bool isNull()   const noexcept { return type == Type::Null;   }
    [[nodiscard]] bool isBool()   const noexcept { return type == Type::Bool;   }
    [[nodiscard]] bool isNumber() const noexcept { return type == Type::Number; }
    [[nodiscard]] bool isString() const noexcept { return type == Type::String; }
    [[nodiscard]] bool isArray()  const noexcept { return type == Type::Array;  }
    [[nodiscard]] bool isObject() const noexcept { return type == Type::Object; }

    // ── Checked accessors (throw std::runtime_error on type mismatch) ─────────
    [[nodiscard]] bool               getBool()   const;
    [[nodiscard]] double             getNumber() const;
    [[nodiscard]] const std::string& getString() const;
    [[nodiscard]] const Array&       getArray()  const;
    [[nodiscard]] const Object&      getObject() const;

    // ── Object lookup ─────────────────────────────────────────────────────────
    // Returns nullptr when key is absent or node is not an object.
    [[nodiscard]] const JsonValue* find(std::string_view key) const noexcept;

    // Returns (or inserts) the value for key; throws if not an object.
    JsonValue& operator[](std::string_view key);

    // ── Array subscript ───────────────────────────────────────────────────────
    JsonValue&       operator[](std::size_t idx);
    const JsonValue& operator[](std::size_t idx) const;

    // ── Factories ─────────────────────────────────────────────────────────────
    static JsonValue null()                          { return {};                           }
    static JsonValue from(bool b);
    static JsonValue from(double n);
    static JsonValue from(std::string s);
    static JsonValue from(std::string_view sv)       { return from(std::string{sv});        }
    static JsonValue from(const char* s)             { return from(std::string{s});         }
    static JsonValue from(int n)                     { return from(static_cast<double>(n)); }
    static JsonValue makeArray()  { JsonValue v; v.type = Type::Array;  return v; }
    static JsonValue makeObject() { JsonValue v; v.type = Type::Object; return v; }
};

// ── JsonDomParser ─────────────────────────────────────────────────────────────
// Wraps HugonParser (SAX) and accumulates events into a JsonValue tree.
//
// Usage:
//   JsonDomParser p;
//   p.feed(chunk1);
//   p.feed(chunk2);
//   JsonValue root = p.finish();

class JsonDomParser {
public:
    JsonDomParser();
    ~JsonDomParser();
    JsonDomParser(JsonDomParser&&) noexcept;
    JsonDomParser& operator=(JsonDomParser&&) noexcept;

    // Feed a chunk of JSON text (may be called multiple times).
    void feed(std::string_view data);

    // Finalise parsing and return the root value.
    // Throws std::runtime_error if the stream is incomplete.
    [[nodiscard]] JsonValue finish();

private:
    struct Impl;
    std::unique_ptr<Impl> mImpl;
};

// ── JsonGenerator ─────────────────────────────────────────────────────────────
// Serialises a JsonValue tree back to a JSON string.
//
// Usage:
//   std::string compact = JsonGenerator::serialize(root);
//   std::string pretty  = JsonGenerator::serialize(root, 2); // 2-space indent

class JsonGenerator {
public:
    // indent < 0 → compact; indent >= 0 → pretty-print with that many spaces.
    static std::string serialize(const JsonValue& val, int indent = -1);

private:
    static void write(const JsonValue& val, std::string& out, int indent, int depth);
    static void writeString(std::string_view s, std::string& out);
};
