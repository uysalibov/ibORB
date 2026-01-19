/**
 * @file main.cpp
 * @brief iborb_idl - CORBA IDL to C++11 Compiler
 * 
 * A standalone IDL compiler that parses CORBA IDL files and generates
 * C++ code following the "IDL to C++11 Language Mapping" standard.
 * 
 * @copyright (c) 2024 ibORB Project
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <filesystem>

#include "preprocessor/preprocessor.hpp"
#include "lexer/lexer.hpp"
#include "parser/parser.hpp"
#include "generator/cpp11_generator.hpp"

namespace fs = std::filesystem;

/**
 * @brief Command line options
 */
struct Options {
    std::vector<std::string> inputFiles;
    std::string outputDir = ".";
    std::vector<std::string> includePaths;
    std::vector<std::pair<std::string, std::string>> defines;
    bool usePreprocessor = true;
    bool verbose = false;
    bool help = false;
    bool version = false;
    bool parseOnly = false;  // Don't generate code
};

/**
 * @brief Print usage information
 */
void printUsage(const char* program) {
    std::cout << "Usage: " << program << " [options] <idl-files...>\n"
              << "\n"
              << "Options:\n"
              << "  -h, --help            Show this help message\n"
              << "  -v, --version         Show version information\n"
              << "  -o, --output <dir>    Output directory for generated files (default: .)\n"
              << "  -I, --include <path>  Add include search path\n"
              << "  -D, --define <name>[=<value>]  Define preprocessor macro\n"
              << "  -E, --no-preprocess   Skip preprocessor (process raw IDL)\n"
              << "  -p, --parse-only      Parse only, don't generate code\n"
              << "  --verbose             Enable verbose output\n"
              << "\n"
              << "Examples:\n"
              << "  " << program << " -o generated/ interface.idl\n"
              << "  " << program << " -I /usr/local/idl -o out/ *.idl\n"
              << "\n";
}

/**
 * @brief Print version information
 */
void printVersion() {
    std::cout << "iborb_idl version 1.0.0\n"
              << "CORBA IDL to C++11 Compiler\n"
              << "Part of the ibORB project\n";
}

/**
 * @brief Parse command line arguments
 */
Options parseArguments(int argc, char* argv[]) {
    Options opts;

    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];

        if (arg == "-h" || arg == "--help") {
            opts.help = true;
        }
        else if (arg == "-v" || arg == "--version") {
            opts.version = true;
        }
        else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                opts.outputDir = argv[++i];
            } else {
                std::cerr << "Error: -o requires an argument\n";
            }
        }
        else if (arg == "-I" || arg == "--include") {
            if (i + 1 < argc) {
                opts.includePaths.push_back(argv[++i]);
            } else {
                std::cerr << "Error: -I requires an argument\n";
            }
        }
        else if (arg == "-D" || arg == "--define") {
            if (i + 1 < argc) {
                std::string def = argv[++i];
                size_t eq = def.find('=');
                if (eq != std::string::npos) {
                    opts.defines.emplace_back(def.substr(0, eq), def.substr(eq + 1));
                } else {
                    opts.defines.emplace_back(def, "1");
                }
            } else {
                std::cerr << "Error: -D requires an argument\n";
            }
        }
        else if (arg == "-E" || arg == "--no-preprocess") {
            opts.usePreprocessor = false;
        }
        else if (arg == "-p" || arg == "--parse-only") {
            opts.parseOnly = true;
        }
        else if (arg == "--verbose") {
            opts.verbose = true;
        }
        else if (arg[0] == '-') {
            std::cerr << "Warning: Unknown option: " << arg << "\n";
        }
        else {
            opts.inputFiles.push_back(arg);
        }
    }

    return opts;
}

/**
 * @brief Read file contents
 */
std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Cannot open file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

/**
 * @brief Process a single IDL file
 */
bool processFile(const std::string& inputFile, const Options& opts) {
    if (opts.verbose) {
        std::cout << "Processing: " << inputFile << "\n";
    }

    std::string source;
    std::string filename = fs::path(inputFile).filename().string();

    // Step 1: Preprocessing
    if (opts.usePreprocessor) {
        if (opts.verbose) {
            std::cout << "  Running preprocessor...\n";
        }

        iborb::preprocessor::Preprocessor pp;
        
        if (!pp.isAvailable()) {
            // No preprocessor found on system - use raw IDL
            if (opts.verbose) {
                std::cout << "  No C preprocessor found, using raw IDL...\n";
            }
            source = readFile(inputFile);
        } else {
            for (const auto& path : opts.includePaths) {
                pp.addIncludePath(path);
            }
            for (const auto& [name, value] : opts.defines) {
                pp.addDefine(name, value);
            }

            auto result = pp.preprocessFile(inputFile);
            if (!result.success) {
                // Preprocessor failed - fall back to raw IDL
                if (opts.verbose) {
                    std::cout << "  Preprocessor failed, using raw IDL...\n";
                }
                source = readFile(inputFile);
            } else {
                source = std::move(result.output);
            }
        }
    } else {
        source = readFile(inputFile);
    }

    // Step 2: Parsing
    if (opts.verbose) {
        std::cout << "  Parsing...\n";
    }

    iborb::parser::Parser parser(source, inputFile);
    auto ast = parser.parse();

    // Report errors
    bool hasErrors = false;
    for (const auto& error : parser.getErrors()) {
        if (error.isWarning) {
            std::cerr << error.message << "\n";
        } else {
            std::cerr << error.message << "\n";
            hasErrors = true;
        }
    }

    if (hasErrors) {
        std::cerr << "Parsing failed with errors.\n";
        return false;
    }

    if (opts.verbose) {
        std::cout << "  Parsed " << ast.definitions.size() << " top-level definitions.\n";
    }

    // Step 3: Code Generation
    if (!opts.parseOnly) {
        if (opts.verbose) {
            std::cout << "  Generating C++11 code...\n";
        }

        iborb::generator::GeneratorConfig genConfig;
        genConfig.outputDir = opts.outputDir;
        genConfig.generateImplementation = true;

        iborb::generator::Cpp11Generator generator(genConfig);
        generator.setSymbolTable(&parser.getSymbolTable());

        if (!generator.generate(ast)) {
            for (const auto& err : generator.getErrors()) {
                std::cerr << "Generator error: " << err << "\n";
            }
            return false;
        }

        // Get output filename
        fs::path inputPath(inputFile);
        std::string baseName = inputPath.stem().string();
        fs::path outputPath(opts.outputDir);

        if (opts.verbose) {
            std::cout << "  Generated: " << (outputPath / (baseName + ".hpp")).string() << "\n";
        }
    }

    return true;
}

/**
 * @brief Main entry point
 */
int main(int argc, char* argv[]) {
    Options opts = parseArguments(argc, argv);

    if (opts.help) {
        printUsage(argv[0]);
        return 0;
    }

    if (opts.version) {
        printVersion();
        return 0;
    }

    if (opts.inputFiles.empty()) {
        std::cerr << "Error: No input files specified.\n";
        printUsage(argv[0]);
        return 1;
    }

    // Create output directory if needed
    if (!opts.parseOnly) {
        try {
            fs::create_directories(opts.outputDir);
        } catch (const std::exception& e) {
            std::cerr << "Error creating output directory: " << e.what() << "\n";
            return 1;
        }
    }

    // Process each input file
    int failures = 0;
    for (const auto& inputFile : opts.inputFiles) {
        try {
            if (!processFile(inputFile, opts)) {
                ++failures;
            }
        } catch (const std::exception& e) {
            std::cerr << "Error processing " << inputFile << ": " << e.what() << "\n";
            ++failures;
        }
    }

    if (failures > 0) {
        std::cerr << failures << " file(s) failed to process.\n";
        return 1;
    }

    if (opts.verbose) {
        std::cout << "Successfully processed " << opts.inputFiles.size() << " file(s).\n";
    }

    return 0;
}
