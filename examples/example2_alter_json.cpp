// example2_alter_json.cpp
// Read a JSON file, perform several in-memory edits, then write the result to
// a new file.
//
// Edits performed on schemaBig.json:
//   • Add a new top-level key  "title"   with a string value.
//   • Add a new top-level key  "version" with a numeric value.
//   • Remove the property entry "$schema" if it exists (by rebuilding the object).
//   • Append an extra required field "auditInfo" to the top-level "required" array.
//   • Inside properties.ueContext, add a new property "tenantId" of type "string".
//
// Usage:  example2_alter_json  <input.json>  <output.json>

#include <hugon/DJSON.h>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

// ── helpers ───────────────────────────────────────────────────────────────────

static JsonValue readFile(const char* path) {
    std::ifstream f{path};
    if (!f) throw std::runtime_error{std::string{"Cannot open: "} + path};
    std::ostringstream ss;
    ss << f.rdbuf();
    JsonDomParser p;
    p.feed(ss.str());
    return p.finish();
}

static void writeFile(const char* path, const JsonValue& val) {
    std::ofstream f{path};
    if (!f) throw std::runtime_error{std::string{"Cannot write: "} + path};
    f << JsonGenerator::serialize(val, /*indent=*/2) << '\n';
}

// Remove a key from an Object by rebuilding the pairs vector.
static void removeKey(JsonValue& obj, std::string_view key) {
    if (!obj.isObject()) return;
    auto& pairs = obj.object;
    pairs.erase(std::remove_if(pairs.begin(), pairs.end(),
        [&](const auto& kv){ return kv.first == key; }), pairs.end());
}

int main(int argc, char* argv[]) {
    if (argc != 3) {
        std::cerr << "Usage: " << argv[0] << " <input.json> <output.json>\n";
        return 1;
    }

    try {
        JsonValue root = readFile(argv[1]);

        if (!root.isObject()) {
            std::cerr << "Expected a top-level JSON object.\n";
            return 1;
        }

        // 1. Add / overwrite scalar keys -------------------------------------------
        root["title"]   = JsonValue::from("Hugon Example Schema");
        root["version"] = JsonValue::from(2.0);

        // 2. Drop the "$schema" meta-key if present --------------------------------
        removeKey(root, "$schema");

        // 3. Append to the "required" array ----------------------------------------
        if (root.find("required") && root.find("required")->isArray())
            root["required"].array.push_back(JsonValue::from("auditInfo"));

        // 4. Add a new property to ueContext.properties ----------------------------
        if (root.find("properties") && root["properties"].find("ueContext") &&
            root["properties"]["ueContext"].find("properties")) {
            // Build  {"type": "string", "description": "Tenant identifier"}
            JsonValue tenantId = JsonValue::makeObject();
            tenantId["type"]        = JsonValue::from("string");
            tenantId["description"] = JsonValue::from("Tenant identifier");

            root["properties"]["ueContext"]["properties"]["tenantId"] = std::move(tenantId);
        }

        writeFile(argv[2], root);
        std::cout << "Written to " << argv[2] << '\n';

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return 1;
    }
}
