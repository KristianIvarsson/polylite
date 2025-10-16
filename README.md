>
> Copyright (c) 2025 Kristian Ivarsson
>
> Licensed under the MIT License. See https://opensource.org/licenses/MIT for details.
>

### Objective
PolyData is a lightweight, header-only C++ library designed for ease of use and maintainability. Its primary goal is to parse and write various data formats, with support for access and mutation, using standard C++ features as much as possible.

### Current Status
At the moment, only JSON is supported. The implementation may be somewhat naive but aims to be strict when writing and more relaxed when parsing.

### Usage & Reference
There is currently no formal user documentation. However, some basic unit test-like code is available, which may serve as a useful reference for how to use the library. You can build and run it using:

```
g++ -std=c++23 -Iinclude ./source/poly/test.cpp && ./a.out
```
