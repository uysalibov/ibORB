cmake_minimum_required(VERSION 3.16)
project(iborb_idl VERSION 1.0.0 LANGUAGES CXX)

# Require C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific options
if(MSVC)
    add_compile_options(/W4 /WX- /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Source files
set(SOURCES
    src/main.cpp
    src/lexer/lexer.cpp
    src/parser/parser.cpp
    src/semantic/symbol_table.cpp
    src/preprocessor/preprocessor.cpp
    src/generator/cpp11_generator.cpp
)

# Header files (for IDE support)
set(HEADERS
    src/ast/ast.hpp
    src/ast/visitor.hpp
    src/lexer/lexer.hpp
    src/parser/parser.hpp
    src/semantic/symbol_table.hpp
    src/preprocessor/preprocessor.hpp
    src/generator/cpp11_generator.hpp
)

# Create executable
add_executable(${PROJECT_NAME} ${SOURCES} ${HEADERS})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Installation
install(TARGETS ${PROJECT_NAME} RUNTIME DESTINATION bin)

# Add a test target for convenience
add_custom_target(run
    COMMAND ${PROJECT_NAME}
    DEPENDS ${PROJECT_NAME}
    WORKING_DIRECTORY ${CMAKE_PROJECT_DIR}
    COMMENT "Running iborb_idl compiler"
)
