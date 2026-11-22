>
> Copyright (c) 2025 Kristian Ivarsson
>
> Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
>

## Objective
PolyLite is a multiprotocol lightweight, header-only C++ library designed for ease of use and maintainability. Its primary goal is to parse and write various data formats, with support for access and mutation, using standard C++ features as much as possible.

### Disclaimer
Due to the complexity of the supported formats, parts of the implementation are intentionally simple or naive. The primary goal is correctness and a minimal memory footprint — the parser operates directly on input buffers without intermediate allocations where possible. Elegance has occasionally been sacrificed for practicality.

## Current Status

| Format | Description 
|--------|-------------
| CBOR   | Concise Binary Object Representation, a binary data format
| JSON   | JavaScript Object Notation, a text-based data format
| PACK   | MessagePack, a binary data format
| PLIST  | Apple property list, using its XML representation
| TOML   | Tom's Obvious Minimal Language, a configuration format
| YAML   | YAML Ain't Markup Language, a text-based data format
| XML    | Extensible Markup Language, a markup format

### Note

`object` keeps its keys in insertion order and may contain duplicate keys. Key access (`find`, `contains`, `at`, `[]` and `try_emplace`) uses the first match, while `emplace` always appends and `count`, `equal_range` and `erase( key)` apply to every match. Parsers keep potential duplicate keys (except TOML, which has to reject them because tables and dotted keys are merged by key).

`integer` is `std::int64_t` and `decimal` is `std::float64_t` (or `double` where `std::float64_t` isn't available), regardless of platform.

The following describe how values are represented and are mostly of interest when converting between formats.

#### CBOR
- `zoned_datetime` is written with its zone offset in *verbose* mode and as UTC in *compact* mode, but is always parsed as UTC

#### JSON
- integers outside `integer` range are parsed as `decimal` (possibly losing precision)

#### PACK
- `zoned_datetime` is written and parsed as UTC, since the timestamp extension has no zone offset
- `zoned_datetime` is written without fractions, since the specification mandates that

#### PLIST
- `zoned_datetime` is written as UTC without fractions, since `<date>` only allows `YYYY-MM-DDTHH:MM:SSZ`
- `write()` produces only the value, while `whole()` also adds the XML declaration, the DOCTYPE and the `<plist>` element

#### XML
- `object` entries containing `nothing` or an empty `string`/`binary`/`object`/`array` are written as self-closing elements, such as `<name/>`, so their original types cannot be distinguished
- Element text is parsed as `string`, including empty elements
- Scalar values are written as text and parsed back as `string`
- The writer and parser support fragments with multiple top-level elements; these are not well-formed XML documents

### Unsupported Values

`poly::node` is a superset of what any single format can represent. Writers always write, even values a format can't represent, which are written as follows.

| Format | Value             | Result
|--------|-------            |--------
| CBOR   | `local_time`      | text string (no tag)
|        | `local_datetime`  | text string (no tag)
| JSON   | `instant`         | string
|        | `binary`          | base64-encoded string
| PACK   | `local_date`      | string
|        | `local_time`      | string
|        | `local_datetime`  | string
| PLIST  | `nothing`         | `<string>null</string>`
|        | `local_date`      | `<string>`
|        | `local_time`      | `<string>`
|        | `local_datetime`  | `<string>`
| TOML   | `nothing`         | `"null"`
|        | `binary`          | base64-encoded string
|        | non `object` root | the value under the key `""`
| YAML   | `local_time`      | plain scalar (parsed back as a string)
| XML    | `array`           | items written in order, without array structure

Calling `<format>::check( node)` before writing or after parsing is optional: it rejects these values and otherwise returns the node, e.g. `poly::toml::write( poly::toml::check( node))` or `poly::yaml::check( poly::yaml::parse( text))`.

### Duplicate Keys

Duplicate keys are written and parsed _as is_, but `<format>::check( node)` rejects them for CBOR, PLIST, TOML and YAML.

### Known Limitations

The following are corner cases that rarely occur in practice and several of them are not handled by other libraries either. They are listed for completeness and most of them are detected and rejected with an error rather than silently misread.

#### CBOR

The following are detected and rejected with an error:
- Bignums (tags 2 and 3) — including values that would otherwise fit in `integer`
- Decimal fractions (tag 4) and bigfloats (tag 5)
- All other unregistered or unrecognised tags
- Integer values outside `integer` range
- Reserved additional-info values (28–30) and reserved simple values
- Tagged items using indefinite-length encoding
- Nested indefinite-length string chunks (RFC 8949 §3.2.3 violation)
- Indefinite-length string chunks whose major type doesn't match the outer string

#### JSON
- Numbers longer than 128 characters may be rejected with an error when parsed from a stream or a non-contiguous container (e.g. `std::istream`, `std::deque` or `std::list`)
- Object keys are written without escaping, so keys containing `"`, `\` or control characters produce invalid JSON

#### PACK
The following are detected and rejected with an error:
- Integer values outside `integer` range
- Extension types other than _timestamp_ (due to the nature of poly::node)
- Map keys that aren't strings (due to the nature of poly::node::object)

#### PLIST
- The DOCTYPE is accepted but ignored

The following are detected and rejected with an error:
- CDATA sections
- Comments and processing instructions inside text (e.g. `<string>a<!-- c -->b</string>`)
- XML versions other than 1.0 and encodings other than UTF-8
- PLIST versions other than 1.0

#### YAML
- The `%YAML` and `%TAG` directives are accepted but ignored
- Multi-line plain scalars are not supported — use block scalars (`|` or `>`) instead

The following are detected and rejected with an error:
- Merge keys (for anchors and aliases)
- Explicit keys (`? key` / `: value` syntax) — use `key: value` form
- Custom tags (e.g. `!mytag`) — `!!` core tags only

#### XML
- The `poly::xml::check()` validates element names using ASCII rules only
- CDATA sections are rejected
- Mixed content (text interleaved with child elements) is rejected
- Attributes are discarded

## Roadmap

### General
- Remove compiler specific code
- Perhaps have a node_view for writing to avoid copying expensive stuff

## Usage & Reference
There is currently no formal user documentation. However, some basic unit test-like code is available, which may serve as a useful reference for how to use the library. You can build and run it using:

### Test
```bash
g++ -std=c++23 -I include -o build/test ./source/poly/test.cpp && ./build/test
```

### Samples

[show.cpp](source/poly/show.cpp)

```bash
g++ -std=c++23 -I include ./source/poly/show.cpp && ./a.out
```

### Examples

#### Parsing

Every format can be parsed from any byte oriented container or stream

```cpp
// parse either an input stream, a string or some other byte oriented container
auto node = poly::json::parse( input);
```

```cpp
// parse some stream or string and skip potential utf8-bom
auto node = poly::json::parse( poly::tool::bom::skip( input));
```

#### Writing

Every format can be written to any byte oriented container or stream and each writer has its default (usually `std::string` or `std::vector< std::byte>`)

```cpp
// write to an output stream
poly::json::write( node, stream);
```

```cpp
// write to a string
std::string json;
poly::json::write( node, json);
```

```cpp
// write some bytes
std::vector< std::byte> json;
poly::json::write( node, json);
```

```cpp
// write to a some other type
std::list< uint8_t> json;
poly::json::write( node, json);
```

```cpp
// write to a string (default for json)
auto json = poly::json::write( node);
```

```cpp
// make sure the node is valid for the format before writing
auto toml = poly::toml::write( poly::toml::check( node));
```

```cpp
// write to a vector of bytes (default for cbor)
auto cbor = poly::cbor::write( node);
```

```cpp
// same as above (i.e. elegant) but with explicit version
auto json = poly::v6_0_0::json::elegant::write( node);
```

```cpp
// write to a compact string
auto json = poly::json::compact::write( node);
```

```cpp
// write to a string (default for plist)
auto xml = poly::xml::plist::write( node);
```

```cpp
// same as above but with xml declaration, doctype and plist root element
auto xml = poly::xml::plist::whole( node);
```

#### Converting
```cpp
// convert yaml to json and make sure nothing has to be degraded on the way
auto json = poly::json::write( poly::json::check( poly::yaml::parse( input)));
```

#### Access
```cpp
// access some data
auto integer = node.as_object().at( "a").as_array().at( 1).as_integer();
```

```cpp
// same as above (almost)
auto integer = node( "a")( 1)->as_integer();
```

```cpp
// iterate over every value of a key, in insertion order
for( const auto& [ name, data] : node.as_object().equal_range( "foo"))
   std::println( "{}", data.as_integer());
```

#### Mutate
```cpp
// mutate some data
node[ "x"][ 4] = std::chrono::system_clock::now();
```

```cpp
// create an object
poly::node node = poly::node::object{
   { "foo", 123},
   { "bar", poly::node::array{ 1, false, "oups"}}
};
```

```cpp
// keep the first "foo" and remove the rest
if( auto [ head, tail] = node.as_object().equal_range( "foo"); head != tail)
   node.as_object().erase( std::next( head), tail);
```

#### Lookup
```cpp
// lookup
if( node.is_integer())
   std::println( "{}", node.as_integer());
```

```cpp
// lookup
if( auto integer = node.to_integer())
   std::println( "{}", *integer);
```

```cpp
// lookup
std::visit( some_functor{}, node);
```
