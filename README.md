>
> Copyright (c) 2025 Kristian Ivarsson
>
> Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
>

### Objective
PolyLite is a multiprotocol lightweight, header-only C++ library designed for ease of use and maintainability. Its primary goal is to parse and write various data formats, with support for access and mutation, using standard C++ features as much as possible.

#### Disclaimer

Due to the complexity of the supported formats, parts of the implementation are intentionally simple or naive. The primary goal is correctness and a minimal memory footprint — the parser operates directly on input buffers without intermediate allocations where possible. Elegance has occasionally been sacrificed for practicality.

### Current Status
At the moment, JSON, TOML and YAML are supported. The implementation may be somewhat naive but aims to be strict when writing and more relaxed when parsing.

#### Note

`poly::node` is a superset of what any single format can represent. Writing a node containing an unsupported type will throw.

##### JSON
- `instant` is not a native type

##### TOML
- `nothing` is not a native type

##### YAML
- `local_time` inside `instant` is not a native type

#### Known Limitations

##### YAML
- The `%YAML` and `%TAG` directives are accepted but ignored
- Flow style (`{...}` and `[...]`) only supports strict JSON
- Compact writing uses flow style JSON and thus have the same limitations as JSON
- Multi-line plain scalars are not supported — use block scalars (`|` or `>`) instead
- Merge keys (for anchors and aliases) are not implemented
- Custom tags (e.g. `!mytag`) are not supported — `!!` core tags only
### Roadmap

#### YAML
- !!binary

#### General
- Remove compiler specific code

### Usage & Reference
There is currently no formal user documentation. However, some basic unit test-like code is available, which may serve as a useful reference for how to use the library. You can build and run it using:

```bash
g++ -std=c++23 -I include -o build/test ./source/poly/test.cpp && ./build/test
```

### Samples

[show.cpp](source/poly/show.cpp)

```bash
g++ -std=c++23 -I include ./source/poly/show.cpp && ./a.out
```
