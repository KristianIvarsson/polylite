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
At the moment, CBOR, JSON, PACK (MSGPACK/MessagePack), TOML and YAML are supported. The implementation may be somewhat naive but aims to be strict when writing and more relaxed when parsing.

### Note

`poly::node` is a superset of what any single format can represent. In strict mode (the default), writing a node containing an unsupported type will throw. In gentle mode, unsupported types are written as strings.

`poly::node::integer` is `std::int64_t` and `poly::node::decimal` is `std::float64_t` (or `double` where `std::float64_t` isn't available), regardless of platform.

#### CBOR
- `zoned_datetime` is written with its zone offset in *verbose* mode and as UTC in *compact* mode, but is always parsed as UTC
- `local_time` and `local_datetime` inside `instant` have no standard CBOR tag but are written as strings in *gentle* mode

#### JSON
- `instant` (_time_) is not a native type
- `binary` is not a native type

#### PACK
- `zoned_datetime` is written and parsed as UTC, since the timestamp extension has no zone offset
- `local_date`, `local_time` and `local_datetime` inside `instant` are not native MSGPACK types but are written as strings in *gentle* mode

#### TOML
- `nothing` (_null_) is not a native type
- `binary` is not a native type

#### YAML
- `local_time` inside `instant` is not a native type

### Known Limitations

#### CBOR

The following are detected and rejected with an error:
- Bignums (tags 2 and 3) — including values that would otherwise fit in `node::integer`
- Decimal fractions (tag 4) and bigfloats (tag 5)
- All other unregistered or unrecognised tags
- Integer values outside `node::integer` range
- Reserved additional-info values (28–30) and reserved simple values
- Tagged items using indefinite-length encoding
- Nested indefinite-length string chunks (RFC 8949 §3.2.3 violation)
- Indefinite-length string chunks whose major type doesn't match the outer string

#### PACK
The following are detected and rejected with an error:
- Integer values outside `node::integer` range
- Extension types other than _timestamp_ (due to the nature of poly::node)
- Map keys that aren't strings (due to the nature of poly::node::object)

#### YAML
- The `%YAML` and `%TAG` directives are accepted but ignored
- Multi-line plain scalars are not supported — use block scalars (`|` or `>`) instead

The following are detected and rejected with an error:
- Merge keys (for anchors and aliases)
- Explicit keys (`? key` / `: value` syntax) — use `key: value` form
- Custom tags (e.g. `!mytag`) — `!!` core tags only

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
// write to a vector of bytes (default for cbor)
auto cbor = poly::cbor::write( node);
```

```cpp
// same as above (i.e. elegant)
auto json = poly::json::v4_3_0::elegant::strict::write( node);
```

```cpp
// write to a compact string
auto json = poly::json::compact::write( node);
```

```cpp
// same as above (i.e. compact and strict)
auto json = poly::json::compact::strict::write( node);
```

```cpp
// write to a string (accepts types not valid for the protocol)
auto json = poly::json::gentle::write( node);
```

```cpp
// same as above (i.e. elegant and gentle)
auto json = poly::json::elegant::gentle::write( node);
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
