#include <hugon/DJSON.h>
#include <hugon/Hugon.h>

#include <cassert>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

// ── Minimal test runner ───────────────────────────────────────────────────────

namespace {

int gPassed = 0;
int gFailed = 0;

void check(bool cond, const char* expr, const char* file, int line) {
    if (cond) {
        ++gPassed;
    } else {
        ++gFailed;
        std::cerr << "  FAIL  " << file << ":" << line << "  " << expr << '\n';
    }
}

} // namespace

#define CHECK(expr) check(!!(expr), #expr, __FILE__, __LINE__)

// ── Utility ───────────────────────────────────────────────────────────────────

static std::string readFile(const char* path) {
    std::ifstream f{path};
    if (!f) throw std::runtime_error{std::string{"Cannot open: "} + path};
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

static JsonValue parseString(std::string_view src) {
    JsonDomParser p;
    p.feed(src);
    return p.finish();
}

static JsonValue parseFile(const char* path) {
    const std::string src = readFile(path);
    JsonDomParser p;
    p.feed(src);
    return p.finish();
}

// ── Test groups ───────────────────────────────────────────────────────────────

// 1. Primitive values ──────────────────────────────────────────────────────────
void testPrimitives() {
    std::cout << "[primitives]\n";

    auto jNull = parseString("null");
    CHECK(jNull.isNull());

    auto jTrue = parseString("true");
    CHECK(jTrue.isBool());
    CHECK(jTrue.getBool() == true);

    auto jFalse = parseString("false");
    CHECK(jFalse.isBool());
    CHECK(jFalse.getBool() == false);

    auto jInt = parseString("42");
    CHECK(jInt.isNumber());
    CHECK(jInt.getNumber() == 42.0);

    auto jNeg = parseString("-7");
    CHECK(jNeg.isNumber());
    CHECK(jNeg.getNumber() == -7.0);

    auto jFloat = parseString("3.14");
    CHECK(jFloat.isNumber());
    CHECK(jFloat.getNumber() > 3.13 && jFloat.getNumber() < 3.15);

    auto jExp = parseString("1e3");
    CHECK(jExp.isNumber());
    CHECK(jExp.getNumber() == 1000.0);

    auto jStr = parseString(R"("hello world")");
    CHECK(jStr.isString());
    CHECK(jStr.getString() == "hello world");

    auto jEsc = parseString(R"("line1\nline2")");
    CHECK(jEsc.isString());
    CHECK(jEsc.getString() == "line1\nline2");
}

// 2. Arrays ───────────────────────────────────────────────────────────────────
void testArrays() {
    std::cout << "[arrays]\n";

    auto jArr = parseString(R"([1, 2, 3])");
    CHECK(jArr.isArray());
    CHECK(jArr.getArray().size() == 3);
    CHECK(jArr[0u].getNumber() == 1.0);
    CHECK(jArr[1u].getNumber() == 2.0);
    CHECK(jArr[2u].getNumber() == 3.0);

    auto jMixed = parseString(R"([null, true, "x", 9])");
    CHECK(jMixed.isArray());
    CHECK(jMixed.getArray().size() == 4);
    CHECK(jMixed[0u].isNull());
    CHECK(jMixed[1u].getBool() == true);
    CHECK(jMixed[2u].getString() == "x");
    CHECK(jMixed[3u].getNumber() == 9.0);

    auto jNested = parseString(R"([[1,2],[3,4]])");
    CHECK(jNested.isArray());
    CHECK(jNested[0u].isArray());
    CHECK(jNested[0u][1u].getNumber() == 2.0);
    CHECK(jNested[1u][0u].getNumber() == 3.0);

    auto jEmpty = parseString("[]");
    CHECK(jEmpty.isArray());
    CHECK(jEmpty.getArray().empty());
}

// 3. Objects ───────────────────────────────────────────────────────────────────
void testObjects() {
    std::cout << "[objects]\n";

    auto jObj = parseString(R"({"a":1,"b":"two","c":true})");
    CHECK(jObj.isObject());
    CHECK(jObj.getObject().size() == 3);
    CHECK(jObj.find("a") != nullptr);
    CHECK(jObj.find("a")->getNumber() == 1.0);
    CHECK(jObj.find("b")->getString() == "two");
    CHECK(jObj.find("c")->getBool() == true);
    CHECK(jObj.find("missing") == nullptr);

    // Nested object
    auto jNested = parseString(R"({"outer":{"inner":42}})");
    CHECK(jNested.find("outer") != nullptr);
    CHECK(jNested.find("outer")->find("inner") != nullptr);
    CHECK(jNested.find("outer")->find("inner")->getNumber() == 42.0);

    auto jEmpty = parseString("{}");
    CHECK(jEmpty.isObject());
    CHECK(jEmpty.getObject().empty());
}

// 4. Streaming (chunked feed) ─────────────────────────────────────────────────
void testStreaming() {
    std::cout << "[streaming]\n";

    const std::string full = R"({"key":[1,2,3]})";

    // Feed one byte at a time
    {
        JsonDomParser p;
        for (char c : full) p.feed(std::string_view{&c, 1});
        auto root = p.finish();
        CHECK(root.isObject());
        CHECK(root.find("key") != nullptr);
        CHECK(root.find("key")->getArray().size() == 3);
    }

    // Feed in halves
    {
        JsonDomParser p;
        p.feed(std::string_view{full}.substr(0, full.size() / 2));
        p.feed(std::string_view{full}.substr(full.size() / 2));
        auto root = p.finish();
        CHECK(root.find("key")->getArray().size() == 3);
    }
}

// 5. Round-trip (parse → serialize → parse) ───────────────────────────────────
void testRoundTrip() {
    std::cout << "[round-trip]\n";

    const std::string src = R"({"nums":[1,2,3],"flag":true,"name":"test","empty":null})";
    const auto root  = parseString(src);
    const auto json1 = JsonGenerator::serialize(root);
    const auto root2 = parseString(json1);
    const auto json2 = JsonGenerator::serialize(root2);

    CHECK(json1 == json2);
    CHECK(root2.find("nums")->getArray().size() == 3);
    CHECK(root2.find("flag")->getBool() == true);
    CHECK(root2.find("name")->getString() == "test");
    CHECK(root2.find("empty")->isNull());
}

// 6. Generator: compact vs pretty ─────────────────────────────────────────────
void testGenerator() {
    std::cout << "[generator]\n";

    const auto root = parseString(R"({"a":1,"b":[2,3]})");

    const auto compact = JsonGenerator::serialize(root, -1);
    CHECK(compact.find('\n') == std::string::npos);
    CHECK(compact == R"({"a":1,"b":[2,3]})");

    const auto pretty = JsonGenerator::serialize(root, 2);
    CHECK(pretty.find('\n') != std::string::npos);
    // Re-parse pretty output and check values are preserved
    const auto reparsed = parseString(pretty);
    CHECK(reparsed.find("a")->getNumber() == 1.0);
    CHECK(reparsed.find("b")->getArray().size() == 2);

    // Special characters in strings are escaped
    const auto special = JsonGenerator::serialize(JsonValue::from("tab\there\nnewline"));
    CHECK(special.find("\\t") != std::string::npos);
    CHECK(special.find("\\n") != std::string::npos);
}

// 7. Error handling ────────────────────────────────────────────────────────────
void testErrors() {
    std::cout << "[errors]\n";

    auto throws = [](std::string_view src) -> bool {
        try { parseString(src); return false; }
        catch (...) { return true; }
    };

    CHECK(throws(R"({"a":1)"));       // unclosed object
    CHECK(throws(R"([1,2,3)"));       // unclosed array
    CHECK(throws(R"({"a":[}))"));     // mismatched brackets

    // Type mismatch accessors throw
    bool caught = false;
    try {
        (void)parseString("42").getString();
    } catch (const std::runtime_error&) { caught = true; }
    CHECK(caught);
}

// 8. schemaBig.json: structural assertions ─────────────────────────────────────
void testSchemaBig() {
    std::cout << "[schemaBig.json]\n";

    const auto root = parseFile("schemaBig.json");

    // Top-level shape
    CHECK(root.isObject());
    CHECK(root.find("type") != nullptr);
    CHECK(root.find("type")->getString() == "object");
    CHECK(root.find("properties") != nullptr);
    CHECK(root.find("required") != nullptr);

    // "required" array has exactly 3 entries
    const auto* req = root.find("required");
    CHECK(req->isArray());
    CHECK(req->getArray().size() == 3);
    CHECK((*req)[0u].getString() == "ueContext");
    CHECK((*req)[1u].getString() == "targetToSourceData");
    CHECK((*req)[2u].getString() == "pduSessionList");

    // "properties" has 6 top-level property definitions
    const auto* props = root.find("properties");
    CHECK(props->isObject());
    CHECK(props->getObject().size() == 6);

    // ueContext is an object type
    const auto* ueCtx = props->find("ueContext");
    CHECK(ueCtx != nullptr);
    CHECK(ueCtx->find("type")->getString() == "object");
    CHECK(ueCtx->find("properties") != nullptr);

    // pduSessionList is an array type with minItems = 1
    const auto* pduList = props->find("pduSessionList");
    CHECK(pduList != nullptr);
    CHECK(pduList->find("type")->getString() == "array");
    CHECK(pduList->find("minItems") != nullptr);
    CHECK(pduList->find("minItems")->getNumber() == 1.0);

    // Deep navigation: pduSessionList.items.properties.sNssai.required[0] == "sst"
    const auto* items  = pduList->find("items");
    CHECK(items != nullptr);
    const auto* sessProps = items->find("properties");
    CHECK(sessProps != nullptr);
    const auto* sNssai = sessProps->find("sNssai");
    CHECK(sNssai != nullptr);
    const auto* sNssaiReq = sNssai->find("required");
    CHECK(sNssaiReq != nullptr);
    CHECK(sNssaiReq->isArray());
    CHECK((*sNssaiReq)[0u].getString() == "sst");

    // sNssai.properties.sst has min=0 max=255
    const auto* sstDef = sNssai->find("properties")->find("sst");
    CHECK(sstDef->find("minimum")->getNumber() == 0.0);
    CHECK(sstDef->find("maximum")->getNumber() == 255.0);

    // Verify ueContext.properties.subRfsp has integer type with bounds
    const auto* ueProps  = ueCtx->find("properties");
    const auto* subRfsp  = ueProps->find("subRfsp");
    CHECK(subRfsp != nullptr);
    CHECK(subRfsp->find("type")->getString() == "integer");
    CHECK(subRfsp->find("minimum")->getNumber() == 1.0);
    CHECK(subRfsp->find("maximum")->getNumber() == 256.0);

    // Verify ueContext.properties.supiUnauthInd is boolean type
    const auto* supiUnauthInd = ueProps->find("supiUnauthInd");
    CHECK(supiUnauthInd != nullptr);
    CHECK(supiUnauthInd->find("type")->getString() == "boolean");

    // ueContext.properties.subUeAmbr has nested required [uplink, downlink]
    const auto* subUeAmbr  = ueProps->find("subUeAmbr");
    const auto* ambrReq    = subUeAmbr->find("required");
    CHECK(ambrReq->isArray());
    CHECK(ambrReq->getArray().size() == 2);
    CHECK((*ambrReq)[0u].getString() == "uplink");
    CHECK((*ambrReq)[1u].getString() == "downlink");

    // n2InfoContent.required == ["ngapData"]
    const auto* n2 = sessProps->find("n2InfoContent");
    CHECK(n2 != nullptr);
    const auto* n2Req = n2->find("required");
    CHECK(n2Req->isArray());
    CHECK((*n2Req)[0u].getString() == "ngapData");
}

// 9. schemaBig.json: streaming (chunked) parse ────────────────────────────────
void testSchemaBigStreaming() {
    std::cout << "[schemaBig.json streaming]\n";

    const std::string src = readFile("schemaBig.json");
    constexpr std::size_t chunkSize = 512;

    JsonDomParser p;
    for (std::size_t offset = 0; offset < src.size(); offset += chunkSize)
        p.feed(std::string_view{src}.substr(offset, chunkSize));
    const auto root = p.finish();

    CHECK(root.isObject());
    CHECK(root.find("type")->getString() == "object");
    CHECK(root.find("required")->getArray().size() == 3);
}

// 10. schemaBig.json: round-trip fidelity ─────────────────────────────────────
void testSchemaBigRoundTrip() {
    std::cout << "[schemaBig.json round-trip]\n";

    const auto root  = parseFile("schemaBig.json");
    const auto json1 = JsonGenerator::serialize(root);
    const auto root2 = parseString(json1);
    const auto json2 = JsonGenerator::serialize(root2);

    CHECK(json1 == json2);

    // Spot-check a deep value survives the round-trip
    const auto* pduList  = root2.find("properties")->find("pduSessionList");
    CHECK(pduList->find("minItems")->getNumber() == 1.0);
}

// ── Entry point ───────────────────────────────────────────────────────────────

int main() {
    testPrimitives();
    testArrays();
    testObjects();
    testStreaming();
    testRoundTrip();
    testGenerator();
    testErrors();
    testSchemaBig();
    testSchemaBigStreaming();
    testSchemaBigRoundTrip();

    std::cout << '\n';
    if (gFailed == 0) {
        std::cout << "All " << gPassed << " checks passed.\n";
        return 0;
    } else {
        std::cout << gPassed << " passed, " << gFailed << " FAILED.\n";
        return 1;
    }
}
