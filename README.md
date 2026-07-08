# Hugon

Hugon is a lightweight C++17 JSON library. It based on implementation by Bohdan R. Rau <ethanak[at]polip.com> in Python to parse huge JSON data that doesn't fit into memory. It consists of 3 parts:

- SAX-style streaming parser (`HugonParser` + `HugonHandler`)
- DOM-style parser (`JsonDomParser`)
- JSON generator / serializer (`JsonGenerator`)


The project is organized as a reusable CMake library target: `hugon::hugon`.

## Requirements

- C++17 compiler (GCC/Clang/MSVC)
- CMake 3.16+

## Project Layout

- `include/hugon/` public headers
- `src/` library implementation
- `tests/` test suite + `schemaBig.json`
- `examples/` usage examples
- `cmake/` package config template

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## Run Tests

```bash
ctest --test-dir build --output-on-failure
```

## Build Options

- `HUGON_BUILD_TESTS` (default: `ON`)
- `HUGON_BUILD_EXAMPLES` (default: `ON`)

Example:

```bash
cmake -S . -B build -DHUGON_BUILD_TESTS=OFF -DHUGON_BUILD_EXAMPLES=ON
```

## Examples

After build:

- Pretty-print JSON:

```bash
./build/examples/example1_pretty_print tests/schemaBig.json
```

- Read, alter, and write JSON:

```bash
./build/examples/example2_alter_json tests/schemaBig.json /tmp/altered.json
```

- Create nested JSON from scratch:

```bash
./build/examples/example3_create_json
```

## Quick API Usage

### DOM parse and pretty print

```cpp
#include <hugon/DJSON.h>

JsonDomParser parser;
parser.feed("{\"a\":1,\"b\":[2,3]}");
JsonValue root = parser.finish();

std::string pretty = JsonGenerator::serialize(root, 2);
```

### SAX parse

```cpp
#include <hugon/Hugon.h>

struct MyHandler : HugonHandler {
    void setData(std::string_view, bool) override {}
    void startObject() override {}
    void endObject() override {}
    void startArray() override {}
    void endArray() override {}
    void startValue() override {}
    void endValue() override {}
};

MyHandler h;
HugonParser p{h};
p.feed("{\"x\":1}");
p.close();
```

## Install / Package

The project exports CMake package files, so it can be used with `find_package(hugon CONFIG REQUIRED)` after install.

Typical install command:

```bash
cmake --install build --prefix /usr/local
```

Then in another CMake project:

```cmake
find_package(hugon CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE hugon::hugon)
```

## License

BSD 3-Clause License. See `LICENSE`.
