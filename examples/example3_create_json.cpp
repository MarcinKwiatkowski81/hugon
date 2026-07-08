// example3_create_json.cpp
// Build a deeply-nested JSON document completely from scratch and print it.
//
// The document represents a small "library catalogue" with:
//   • top-level metadata (string, number, boolean, null)
//   • an "address" object with nested "geo" coordinates
//   • a "books" array whose items are objects containing:
//       – scalar fields
//       – an "authors" array of objects
//       – a "tags" array of strings
//       – a "editions" array of objects each containing a nested "publisher" object
//   • a "stats" object with mixed number and boolean values
//
// Usage:  example3_create_json

#include <hugon/DJSON.h>
#include <iostream>

// ── Helpers for building objects / arrays concisely ───────────────────────────

// Append a key-value pair to an Object node and return the modified node.
static JsonValue& set(JsonValue& obj, std::string_view key, JsonValue val) {
    obj[key] = std::move(val);
    return obj;
}

// Append a value to an Array node and return the modified node.
static JsonValue& push(JsonValue& arr, JsonValue val) {
    arr.array.push_back(std::move(val));
    return arr;
}

int main() {
    // ── Root object ──────────────────────────────────────────────────────────
    JsonValue root = JsonValue::makeObject();

    set(root, "name",        JsonValue::from("Biblioteka Miejska"));
    set(root, "established", JsonValue::from(1948.0));
    set(root, "open",        JsonValue::from(true));
    set(root, "motto",       JsonValue::null());           // intentional null

    // ── Nested address ───────────────────────────────────────────────────────
    JsonValue geo = JsonValue::makeObject();
    set(geo, "lat",  JsonValue::from(52.2297));
    set(geo, "lon",  JsonValue::from(21.0122));
    set(geo, "alt",  JsonValue::from(100.0));

    JsonValue address = JsonValue::makeObject();
    set(address, "street",  JsonValue::from("ul. Marszalkowska 1"));
    set(address, "city",    JsonValue::from("Warszawa"));
    set(address, "country", JsonValue::from("Polska"));
    set(address, "geo",     std::move(geo));

    set(root, "address", std::move(address));

    // ── Books array ──────────────────────────────────────────────────────────
    JsonValue books = JsonValue::makeArray();

    // --- Book 1 ---------------------------------------------------------------
    {
        JsonValue book = JsonValue::makeObject();
        set(book, "isbn",  JsonValue::from("978-83-240-2670-8"));
        set(book, "title", JsonValue::from("Lalka"));
        set(book, "year",  JsonValue::from(1890.0));
        set(book, "pages", JsonValue::from(760.0));
        set(book, "inStock", JsonValue::from(true));

        // authors
        JsonValue authors = JsonValue::makeArray();
        JsonValue author1 = JsonValue::makeObject();
        set(author1, "firstName", JsonValue::from("Boleslaw"));
        set(author1, "lastName",  JsonValue::from("Prus"));
        set(author1, "nationality", JsonValue::from("Polish"));
        push(authors, std::move(author1));
        set(book, "authors", std::move(authors));

        // tags
        JsonValue tags = JsonValue::makeArray();
        push(tags, JsonValue::from("powiesc"));
        push(tags, JsonValue::from("klasyka"));
        push(tags, JsonValue::from("pozytywizm"));
        set(book, "tags", std::move(tags));

        // editions
        JsonValue editions = JsonValue::makeArray();

        JsonValue ed1 = JsonValue::makeObject();
        set(ed1, "number", JsonValue::from(1.0));
        set(ed1, "year",   JsonValue::from(1890.0));
        JsonValue pub1 = JsonValue::makeObject();
        set(pub1, "name",    JsonValue::from("Gebethner i Wolff"));
        set(pub1, "country", JsonValue::from("Polska"));
        set(pub1, "imprint", JsonValue::null());
        set(ed1, "publisher", std::move(pub1));
        push(editions, std::move(ed1));

        JsonValue ed2 = JsonValue::makeObject();
        set(ed2, "number", JsonValue::from(2.0));
        set(ed2, "year",   JsonValue::from(2023.0));
        JsonValue pub2 = JsonValue::makeObject();
        set(pub2, "name",    JsonValue::from("Wydawnictwo Literackie"));
        set(pub2, "country", JsonValue::from("Polska"));
        set(pub2, "imprint", JsonValue::from("Klasyka Polska"));
        set(ed2, "publisher", std::move(pub2));
        push(editions, std::move(ed2));

        set(book, "editions", std::move(editions));
        push(books, std::move(book));
    }

    // --- Book 2 ---------------------------------------------------------------
    {
        JsonValue book = JsonValue::makeObject();
        set(book, "isbn",    JsonValue::from("978-83-240-1125-4"));
        set(book, "title",   JsonValue::from("Pan Tadeusz"));
        set(book, "year",    JsonValue::from(1834.0));
        set(book, "pages",   JsonValue::from(496.0));
        set(book, "inStock", JsonValue::from(false));

        JsonValue authors = JsonValue::makeArray();

        JsonValue a1 = JsonValue::makeObject();
        set(a1, "firstName",   JsonValue::from("Adam"));
        set(a1, "lastName",    JsonValue::from("Mickiewicz"));
        set(a1, "nationality", JsonValue::from("Polish"));
        push(authors, std::move(a1));

        JsonValue a2 = JsonValue::makeObject();
        set(a2, "firstName",   JsonValue::from("Stanislaw"));
        set(a2, "lastName",    JsonValue::from("Pigon"));
        set(a2, "nationality", JsonValue::from("Polish"));
        push(authors, std::move(a2));

        set(book, "authors", std::move(authors));

        JsonValue tags = JsonValue::makeArray();
        push(tags, JsonValue::from("epopeja"));
        push(tags, JsonValue::from("romantyzm"));
        push(tags, JsonValue::from("lektura"));
        set(book, "tags", std::move(tags));

        JsonValue editions = JsonValue::makeArray();
        JsonValue ed = JsonValue::makeObject();
        set(ed, "number", JsonValue::from(1.0));
        set(ed, "year",   JsonValue::from(1834.0));
        JsonValue pub = JsonValue::makeObject();
        set(pub, "name",    JsonValue::from("Drukarnia Uniwersytetu Wilenskiego"));
        set(pub, "country", JsonValue::from("Polska"));
        set(pub, "imprint", JsonValue::null());
        set(ed, "publisher", std::move(pub));
        push(editions, std::move(ed));
        set(book, "editions", std::move(editions));

        push(books, std::move(book));
    }

    set(root, "books", std::move(books));

    // ── Stats object ─────────────────────────────────────────────────────────
    JsonValue stats = JsonValue::makeObject();
    set(stats, "totalBooks",    JsonValue::from(42115.0));
    set(stats, "totalMembers",  JsonValue::from(2890.0));
    set(stats, "activeLoans",   JsonValue::from(731.0));
    set(stats, "digitalAccess", JsonValue::from(true));
    set(stats, "overdueRate",   JsonValue::from(0.041));
    set(root, "stats", std::move(stats));

    // ── Print ─────────────────────────────────────────────────────────────────
    std::cout << JsonGenerator::serialize(root, /*indent=*/2) << '\n';
}
