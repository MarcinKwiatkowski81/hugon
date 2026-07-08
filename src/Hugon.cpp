#include <hugon/Hugon.h>

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

void ltrim(std::string& s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char c) {
        return !std::isspace(c);
    }));
}

void rtrim(std::string& s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char c) {
        return !std::isspace(c);
    }).base(), s.end());
}

constexpr bool charIn(char c, std::string_view set) noexcept {
    return set.find(c) != std::string_view::npos;
}

} // namespace

void HugonParser::feed(std::string_view data) {
    mString += data;
    while (getToken())
        pushToken();
}

void HugonParser::close() {
    rtrim(mString);
    if (!mString.empty() || mStack != "?")
        throw std::runtime_error{"Stream cut"};
    mToken.clear();
    mLastToken.clear();
}

void HugonParser::pushToken() {
    bool is_key = false;

    // Key end
    if (mStack.back() == '{' && (mToken == "," || mToken == "}"))
        if (mLastToken != "{")
            sendToken(")");

    // Null element in array
    if (mStack.back() == '[' && (mToken == "," || mToken == "]")) {
        if (mToken == "," && (mLastToken == "," || mLastToken == "["))
            sendToken("null");
        else if (mToken == "]" && mLastToken == ",")
            sendToken("null");
    }

    // Start key
    if (mStack.back() == '{' && !charIn(mToken[0], "{}[],:"))
        if (mLastToken == "{" || mLastToken == ",")
            is_key = true;

    mLastToken = mToken;
    sendToken(mToken, is_key);
}

void HugonParser::sendToken(std::string_view token, bool is_key) {
    if (token == ",")
        return;

    if      (token == "{") mHandler.startObject();
    else if (token == "}") mHandler.endObject();
    else if (token == "[") mHandler.startArray();
    else if (token == "]") mHandler.endArray();
    else if (token == ":") mHandler.startValue();
    else if (token == ")") mHandler.endValue();
    else {
        if (!token.empty() && token.front() == 'd')
            token.remove_prefix(1);
        mHandler.setData(token, is_key);
    }
}

bool HugonParser::getToken() {
    ltrim(mString);
    rtrim(mString);

    if (mString.empty())
        return false;

    const char first = mString.front();

    if (charIn(first, "{}[],:")) {
        if (charIn(first, "{[")) {
            mStack += first;
        } else if (charIn(first, "}]")) {
            if ((mStack.back() == '[' && first == '}') ||
                (mStack.back() == '{' && first == ']'))
                throw std::runtime_error{"Attribute error"};
            mStack.pop_back();
        }
        mToken.assign(1, first);
        mString.erase(0, 1);
        return true;
    }

    if (first == '\'' || first == '"') {
        bool escaped = false;
        for (size_t idx = 1; idx < mString.size(); ++idx) {
            if (escaped) { escaped = false; continue; }
            if (mString[idx] == '\\') { escaped = true; continue; }
            if (mString[idx] == first) {
                mToken = mString.substr(0, idx + 1);
                mString.erase(0, idx + 1);
                return true;
            }
        }
        return false;
    }

    for (std::string_view kw : kKeywords) {
        if (std::string_view{mString}.substr(0, kw.size()) == kw) {
            mToken = kw;
            mString.erase(0, kw.size());
            return true;
        }
    }

    // Parse number (leading +/- is non-standard but tolerated)
    size_t idx = 0;
    if (idx < mString.size() && charIn(mString[idx], "+-")) ++idx;
    while (idx < mString.size() && std::isdigit(static_cast<unsigned char>(mString[idx]))) ++idx;
    if (idx < mString.size() && mString[idx] == '.') {
        ++idx;
        while (idx < mString.size() && std::isdigit(static_cast<unsigned char>(mString[idx]))) ++idx;
    }
    if (idx < mString.size() && std::tolower(static_cast<unsigned char>(mString[idx])) == 'e') {
        ++idx;
        if (idx < mString.size() && charIn(mString[idx], "+-")) ++idx;
        while (idx < mString.size() && std::isdigit(static_cast<unsigned char>(mString[idx]))) ++idx;
    }
    mToken = 'd' + mString.substr(0, idx);
    mString.erase(0, idx);

    return true;
}
