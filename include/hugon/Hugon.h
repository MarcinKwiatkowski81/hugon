#pragma once

#include <array>
#include <string>
#include <string_view>

inline constexpr std::array<std::string_view, 3> kKeywords = {"true", "false", "null"};

// Virtual base class for event handlers
class HugonHandler {
public:
    HugonHandler() = default;
    virtual ~HugonHandler() = default;
    virtual void setData(std::string_view data, bool is_key) = 0;
    virtual void startObject() = 0;
    virtual void endObject() = 0;
    virtual void startArray() = 0;
    virtual void endArray() = 0;
    virtual void startValue() = 0;
    virtual void endValue() = 0;
};

// SAX-style JSON stream parser
class HugonParser {
public:
    explicit HugonParser(HugonHandler& handler) : mHandler{handler} {}

    void feed(std::string_view data);
    void close();

private:
    std::string   mString;
    std::string   mToken;
    std::string   mStack{"?"};
    std::string   mLastToken;
    HugonHandler& mHandler;

    void pushToken();
    void sendToken(std::string_view token, bool is_key = false);
    [[nodiscard]] bool getToken();
};

