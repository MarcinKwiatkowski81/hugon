#include <hugon/DJSON.h>

#include <cassert>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <string_view>

// ── JsonValue: checked accessors ──────────────────────────────────────────────

bool JsonValue::getBool() const {
    if (type != Type::Bool)
        throw std::runtime_error{"JsonValue: expected bool"};
    return boolean;
}

double JsonValue::getNumber() const {
    if (type != Type::Number)
        throw std::runtime_error{"JsonValue: expected number"};
    return number;
}

const std::string& JsonValue::getString() const {
    if (type != Type::String)
        throw std::runtime_error{"JsonValue: expected string"};
    return string;
}

const JsonValue::Array& JsonValue::getArray() const {
    if (type != Type::Array)
        throw std::runtime_error{"JsonValue: expected array"};
    return array;
}

const JsonValue::Object& JsonValue::getObject() const {
    if (type != Type::Object)
        throw std::runtime_error{"JsonValue: expected object"};
    return object;
}

// ── JsonValue: lookup / subscript ─────────────────────────────────────────────

const JsonValue* JsonValue::find(std::string_view key) const noexcept {
    if (type != Type::Object) return nullptr;
    for (const auto& [k, v] : object)
        if (k == key) return &v;
    return nullptr;
}

JsonValue& JsonValue::operator[](std::string_view key) {
    if (type != Type::Object)
        throw std::runtime_error{"JsonValue: expected object"};
    for (auto& [k, v] : object)
        if (k == key) return v;
    object.emplace_back(std::string{key}, JsonValue{});
    return object.back().second;
}

JsonValue& JsonValue::operator[](std::size_t idx) {
    if (type != Type::Array)
        throw std::runtime_error{"JsonValue: expected array"};
    return array.at(idx);
}

const JsonValue& JsonValue::operator[](std::size_t idx) const {
    if (type != Type::Array)
        throw std::runtime_error{"JsonValue: expected array"};
    return array.at(idx);
}

// ── JsonValue: factories ──────────────────────────────────────────────────────

JsonValue JsonValue::from(bool b) {
    JsonValue v;
    v.type    = Type::Bool;
    v.boolean = b;
    return v;
}

JsonValue JsonValue::from(double n) {
    JsonValue v;
    v.type   = Type::Number;
    v.number = n;
    return v;
}

JsonValue JsonValue::from(std::string s) {
    JsonValue v;
    v.type   = Type::String;
    v.string = std::move(s);
    return v;
}

// ── Internal helpers ──────────────────────────────────────────────────────────

namespace {

// Strip surrounding single or double quotes and unescape common sequences.
std::string unquote(std::string_view token) {
    if (token.size() < 2) return std::string{token};

    const char quote = token.front();
    if (quote != '"' && quote != '\'') return std::string{token};

    std::string result;
    result.reserve(token.size() - 2);

    for (std::size_t i = 1; i + 1 < token.size(); ++i) {
        if (token[i] != '\\') {
            result += token[i];
            continue;
        }
        if (++i + 1 >= token.size()) break; // truncated escape – best effort
        switch (token[i]) {
            case '"':  result += '"';  break;
            case '\'': result += '\''; break;
            case '\\': result += '\\'; break;
            case '/':  result += '/';  break;
            case 'n':  result += '\n'; break;
            case 'r':  result += '\r'; break;
            case 't':  result += '\t'; break;
            case 'b':  result += '\b'; break;
            case 'f':  result += '\f'; break;
            default:   result += token[i]; break;
        }
    }
    return result;
}

// Parse a token received from the SAX layer into a typed JsonValue.
JsonValue parseAtom(std::string_view token) {
    if (token == "null")  return JsonValue::null();
    if (token == "true")  return JsonValue::from(true);
    if (token == "false") return JsonValue::from(false);

    if (!token.empty() && (token.front() == '"' || token.front() == '\''))
        return JsonValue::from(unquote(token));

    // Attempt numeric parse; fall back to storing as a raw string.
    try {
        std::size_t pos = 0;
        const double n = std::stod(std::string{token}, &pos);
        if (pos == token.size())
            return JsonValue::from(n);
    } catch (...) {}

    return JsonValue::from(std::string{token});
}

} // namespace

// ── JsonDomParser::Impl ───────────────────────────────────────────────────────
// Implements HugonHandler to build a JsonValue tree from SAX events.
//
// When a nested object/array opens, the pendingKey that names it in its parent
// must be saved in the frame — otherwise it gets overwritten by the keys of the
// nested object's own children.  Each Frame therefore carries the key that will
// be used when the container is added to *its* parent on close.

struct JsonDomParser::Impl : HugonHandler {
    struct Frame {
        JsonValue   value;
        std::string parentKey;  // key used when this container is closed
    };

    HugonParser        parser{*this};
    std::vector<Frame> stack;
    std::string        pendingKey;
    JsonValue          root;

    // Place a completed value into the current parent container (or root).
    // key is used only when the parent is an object.
    void place(JsonValue val, std::string key) {
        if (stack.empty()) {
            root = std::move(val);
            return;
        }
        auto& top = stack.back().value;
        if (top.isArray()) {
            top.array.push_back(std::move(val));
        } else {
            top.object.emplace_back(std::move(key), std::move(val));
        }
    }

    void setData(std::string_view data, bool is_key) override {
        if (is_key) {
            pendingKey = unquote(data);
        } else {
            place(parseAtom(data), std::move(pendingKey));
            pendingKey.clear();
        }
    }

    void openContainer(JsonValue container) {
        // Snapshot the current pendingKey so that when this container closes
        // it can be added to its parent under the right key.
        stack.push_back({std::move(container), std::move(pendingKey)});
        pendingKey.clear();
    }

    void closeContainer() {
        auto frame = std::move(stack.back());
        stack.pop_back();
        place(std::move(frame.value), std::move(frame.parentKey));
    }

    void startObject() override { openContainer(JsonValue::makeObject()); }
    void endObject()   override { closeContainer(); }
    void startArray()  override { openContainer(JsonValue::makeArray()); }
    void endArray()    override { closeContainer(); }
    void startValue()  override {}
    void endValue()    override {}
};

// ── JsonDomParser ─────────────────────────────────────────────────────────────

JsonDomParser::JsonDomParser()  : mImpl{std::make_unique<Impl>()} {}
JsonDomParser::~JsonDomParser() = default;
JsonDomParser::JsonDomParser(JsonDomParser&&) noexcept            = default;
JsonDomParser& JsonDomParser::operator=(JsonDomParser&&) noexcept = default;

void JsonDomParser::feed(std::string_view data) {
    mImpl->parser.feed(data);
}

JsonValue JsonDomParser::finish() {
    mImpl->parser.close();
    return std::move(mImpl->root);
}

// ── JsonGenerator ─────────────────────────────────────────────────────────────

namespace {

void writeIndent(std::string& out, int indent, int depth) {
    out += '\n';
    out.append(static_cast<std::size_t>(indent * depth), ' ');
}

} // namespace

void JsonGenerator::writeString(std::string_view s, std::string& out) {
    out += '"';
    for (const unsigned char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            case '\b': out += "\\b";  break;
            case '\f': out += "\\f";  break;
            default:
                if (c < 0x20u) {
                    char buf[7];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += static_cast<char>(c);
                }
        }
    }
    out += '"';
}

void JsonGenerator::write(const JsonValue& val, std::string& out, int indent, int depth) {
    switch (val.type) {

        case JsonValue::Type::Null:
            out += "null";
            break;

        case JsonValue::Type::Bool:
            out += val.boolean ? "true" : "false";
            break;

        case JsonValue::Type::Number: {
            // %.17g gives a round-trip-safe representation and removes trailing zeros.
            char buf[64];
            const int n = std::snprintf(buf, sizeof(buf), "%.17g", val.number);
            if (n > 0 && n < static_cast<int>(sizeof(buf)))
                out.append(buf, static_cast<std::size_t>(n));
            break;
        }

        case JsonValue::Type::String:
            writeString(val.string, out);
            break;

        case JsonValue::Type::Array: {
            out += '[';
            bool first = true;
            for (const auto& elem : val.array) {
                if (!first) out += ',';
                if (indent >= 0) writeIndent(out, indent, depth + 1);
                write(elem, out, indent, depth + 1);
                first = false;
            }
            if (!val.array.empty() && indent >= 0) writeIndent(out, indent, depth);
            out += ']';
            break;
        }

        case JsonValue::Type::Object: {
            out += '{';
            bool first = true;
            for (const auto& [key, value] : val.object) {
                if (!first) out += ',';
                if (indent >= 0) writeIndent(out, indent, depth + 1);
                writeString(key, out);
                out += ':';
                if (indent >= 0) out += ' ';
                write(value, out, indent, depth + 1);
                first = false;
            }
            if (!val.object.empty() && indent >= 0) writeIndent(out, indent, depth);
            out += '}';
            break;
        }
    }
}

std::string JsonGenerator::serialize(const JsonValue& val, int indent) {
    std::string out;
    out.reserve(256);
    write(val, out, indent, 0);
    return out;
}
