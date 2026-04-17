>
> Copyright (c) 2025 Kristian Ivarsson
>
> Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
>

### Objective
PolyLite is a multiprotocol lightweight, header-only C++ library designed for ease of use and maintainability. Its primary goal is to parse and write various data formats, with support for access and mutation, using standard C++ features as much as possible.

### Current Status
At the moment, JSON, TOML and YAML is supported. The implementation may be somewhat naive but aims to be strict when writing and more relaxed when parsing.

#### Known Limitations

##### TOML
- Datetime types (`1979-05-27`, `07:32:00`, etc.) are not supported — parsed as strings

##### YAML
- Multi-line plain scalars are not supported — use block scalars (`|` or `>`) instead
- Anchors and aliases (`&anchor`, `*alias`) are not supported
- Custom tags (e.g. `!mytag`) are not supported — `!!` core tags only
- The `%TAG` directive is accepted but ignored

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
