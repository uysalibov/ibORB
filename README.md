# ibORB

A modern, lightweight CORBA 3.4 implementation in C++17.

## Overview

ibORB is a from-scratch implementation of the Common Object Request Broker Architecture (CORBA) specification, designed for simplicity, performance, and modern C++ standards. The project aims to provide a clean, dependency-free CORBA stack suitable for embedded systems and modern applications.

## Components

| Component | Description | Status |
|-----------|-------------|--------|
| [iborb_idl](./iborb_idl/) | IDL to C++11 Compiler | ✅ Ready |
| iborb_core | ORB Core Runtime | 🚧 Planned |
| iborb_naming | Naming Service | 🚧 Planned |

## iborb_idl - IDL Compiler

The IDL compiler is the first completed component. It parses CORBA IDL files and generates modern C++11/17 code.

**Key Features:**
- Pure C++17 implementation (no external dependencies)
- Cross-platform (Windows, Linux, macOS)
- Hand-written recursive descent parser
- IDL to C++11 language mapping

**Quick Example:**
```bash
cd iborb_idl/build
./iborb_idl -o generated/ ../examples/demo.idl
```

👉 See [iborb_idl/README.md](./iborb_idl/README.md) for full documentation.

## Building

### Prerequisites

- CMake 3.16+
- C++17 compiler (GCC 7+, Clang 5+, MSVC 2017+)

### Build iborb_idl

```bash
cd iborb_idl
mkdir build && cd build
cmake ..
cmake --build .
```

## Project Goals

- **Zero Dependencies**: Only C++17 standard library
- **Modern C++**: Smart pointers, move semantics, `std::variant`, etc.
- **Clean API**: Simple and intuitive interfaces
- **Portable**: Windows, Linux, macOS support
- **Lightweight**: Suitable for embedded systems

## License

Apache License 2.0 - See [LICENSE](./LICENSE) for details.
