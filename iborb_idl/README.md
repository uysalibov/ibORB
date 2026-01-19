# iborb_idl - CORBA IDL to C++11 Compiler

A standalone CORBA IDL compiler that parses IDL files and generates modern C++11/17 code following the "IDL to C++11 Language Mapping" standard.

## Features

- **Pure C++17 Implementation**: No external dependencies (no Boost, ACE, ANTLR, or Yacc/Bison)
- **Cross-Platform**: Works on Windows (MSVC, MinGW), Linux, and macOS
- **Modern C++ Output**: Generates clean C++11/17 code using STL containers
- **Error Recovery**: Parser attempts to recover and report multiple errors
- **Preprocessing Support**: Optionally invokes system preprocessor for `#include` and macros

## IDL to C++ Mapping

| IDL Type | C++ Type |
|----------|----------|
| `module` | `namespace` |
| `interface` | Abstract class with pure virtual functions |
| `struct` | `struct` with equality operators |
| `enum` | `enum class` |
| `exception` | Class derived from `std::exception` |
| `sequence<T>` | `std::vector<T>` |
| `string` | `std::string` |
| `wstring` | `std::wstring` |
| `long` | `int32_t` |
| `short` | `int16_t` |
| `long long` | `int64_t` |
| `unsigned long` | `uint32_t` |
| `float` | `float` |
| `double` | `double` |
| `boolean` | `bool` |
| `octet` | `uint8_t` |
| `char` | `char` |
| `wchar` | `wchar_t` |

## Building

### Prerequisites

- CMake 3.16 or higher
- C++17 compatible compiler (GCC 7+, Clang 5+, MSVC 2017+)

### Build Steps

```bash
# Create build directory
mkdir build && cd build

# Configure
cmake ..

# Build
cmake --build .

# Or on Linux/macOS with make
make
```

### Windows (Visual Studio)

```batch
mkdir build && cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

## Usage

```bash
# Basic usage
iborb_idl interface.idl

# Specify output directory
iborb_idl -o generated/ interface.idl

# Add include paths
iborb_idl -I /path/to/includes -o out/ *.idl

# Define preprocessor macros
iborb_idl -D DEBUG -D VERSION=2 interface.idl

# Skip preprocessing (process raw IDL)
iborb_idl -E interface.idl

# Parse only (syntax check)
iborb_idl -p interface.idl

# Verbose output
iborb_idl --verbose -o out/ interface.idl
```

### Command Line Options

| Option | Description |
|--------|-------------|
| `-h, --help` | Show help message |
| `-v, --version` | Show version information |
| `-o, --output <dir>` | Output directory for generated files |
| `-I, --include <path>` | Add include search path |
| `-D, --define <name>[=<value>]` | Define preprocessor macro |
| `-E, --no-preprocess` | Skip preprocessor |
| `-p, --parse-only` | Parse only, don't generate code |
| `--verbose` | Enable verbose output |

## Example

Given this IDL file (`demo.idl`):

```idl
module Demo {
    struct UserData {
        long id;
        string name;
    };

    interface Calculator {
        long add(in long a, in long b);
        void processUser(in UserData data);
    };
};
```

Running `iborb_idl -o out/ demo.idl` generates:

**demo.hpp**:
```cpp
#ifndef IBORB_GENERATED_DEMO_HPP
#define IBORB_GENERATED_DEMO_HPP

#include <cstdint>
#include <string>
#include <vector>
#include <memory>

namespace Demo {

struct UserData {
    int32_t id;
    std::string name;

    bool operator==(const UserData& other) const {
        return id == other.id && name == other.name;
    }
    bool operator!=(const UserData& other) const {
        return !(*this == other);
    }
};

class Calculator {
public:
    virtual ~Calculator() = default;

    virtual int32_t add(int32_t a, int32_t b) = 0;
    virtual void processUser(const UserData& data) = 0;
};

using CalculatorPtr = std::shared_ptr<Calculator>;

} // namespace Demo

#endif // IBORB_GENERATED_DEMO_HPP
```

## Architecture

```
iborb_idl/
├── src/
│   ├── main.cpp              # CLI entry point
│   ├── ast/
│   │   ├── ast.hpp           # AST node definitions
│   │   └── visitor.hpp       # Visitor interface
│   ├── lexer/
│   │   ├── lexer.hpp         # Tokenizer interface
│   │   └── lexer.cpp         # Tokenizer implementation
│   ├── parser/
│   │   ├── parser.hpp        # Parser interface
│   │   └── parser.cpp        # Recursive descent parser
│   ├── semantic/
│   │   ├── symbol_table.hpp  # Symbol table interface
│   │   └── symbol_table.cpp  # Scope management
│   ├── preprocessor/
│   │   ├── preprocessor.hpp  # Preprocessor wrapper interface
│   │   └── preprocessor.cpp  # Cross-platform popen wrapper
│   └── generator/
│       ├── cpp11_generator.hpp
│       └── cpp11_generator.cpp
├── examples/
│   └── demo.idl              # Example IDL file
└── CMakeLists.txt
```

## Supported IDL Constructs

- ✅ Modules (with nesting)
- ✅ Interfaces (including abstract, local, forward declarations)
- ✅ Interface inheritance
- ✅ Operations with `in`, `out`, `inout` parameters
- ✅ `oneway` operations
- ✅ `raises` clauses
- ✅ Attributes (`readonly` and read-write)
- ✅ Structs
- ✅ Enums
- ✅ Constants
- ✅ Typedefs
- ✅ Exceptions
- ✅ Unions (with discriminator)
- ✅ Sequences (bounded and unbounded)
- ✅ Strings (bounded and unbounded, including wide strings)
- ✅ Arrays (fixed-size)
- ✅ Basic types (boolean, char, octet, short, long, float, double, etc.)
- ✅ Scoped names (qualified type references)

## License

MIT License - See LICENSE file for details.

## Contributing

Contributions are welcome! Please feel free to submit issues and pull requests.
