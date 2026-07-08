// example1_pretty_print.cpp
// Read a JSON file (or stdin) and print it with 2-space indentation.
//
// Usage:  example1_pretty_print  <file.json>

#include <hugon/DJSON.h>

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

int main(int argc, char* argv[]) {
    std::string src;
    if (argc == 2) {
        std::ifstream f{argv[1]};
        if (!f) { std::cerr << "Cannot open: " << argv[1] << '\n'; return 1; }
        std::ostringstream ss;
        ss << f.rdbuf();
        src = ss.str();
    } else {
        std::ostringstream ss;
        ss << std::cin.rdbuf();
        src = ss.str();
    }

    try {
        JsonDomParser parser;
        parser.feed(src);
        const JsonValue root = parser.finish();
        std::cout << JsonGenerator::serialize(root, /*indent=*/2) << '\n';
    } catch (const std::exception& e) {
        std::cerr << "Parse error: " << e.what() << '\n';
        return 1;
    }
}
