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

#### Known Limitations

##### TOML
- Datetime types (`1970-05-02`, `19:32:00`, etc.) are not supported — parsed as strings

##### YAML
- Flow style (`{...}` and `[...]`) only supports strict JSON
- Multi-line plain scalars are not supported — use block scalars (`|` or `>`) instead
- Merge keys (for anchors and aliases) are not implemented
- Custom tags (e.g. `!mytag`) are not supported — `!!` core tags only
- The `%YAML` and `%TAG` directives are accepted but ignored

### Roadmap

#### TOML
- timestamp (instant)

#### YAML
- !!timestamp (instant)
- !!binary

### Usage & Reference
There is currently no formal user documentation. However, some basic unit test-like code is available, which may serve as a useful reference for how to use the library. You can build and run it using:

```bash
g++ -std=c++23 -I include ./source/poly/test.cpp && ./a.out
```

### Samples

[show.cpp](source/poly/show.cpp)

```bash
g++ -std=c++23 -I include ./source/poly/show.cpp && ./a.out
```
