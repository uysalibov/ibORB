#include "preprocessor/preprocessor.hpp"
#include <cstdio>
#include <array>
#include <sstream>
#include <fstream>
#include <cstdlib>
#include <filesystem>

// Cross-platform popen/pclose wrappers
#ifdef _WIN32
    #define WIN32_LEAN_AND_MEAN
    #include <windows.h>
    #define popen _popen
    #define pclose _pclose
#else
    #include <unistd.h>
    #include <sys/wait.h>
#endif

namespace iborb::preprocessor {

Preprocessor::Preprocessor() 
    : compilerPath_(detectCompiler()) {
}

Preprocessor::Preprocessor(std::string compilerPath)
    : compilerPath_(std::move(compilerPath)) {
}

void Preprocessor::addIncludePath(const std::string& path) {
    includePaths_.push_back(path);
}

void Preprocessor::addDefine(const std::string& name, const std::string& value) {
    defines_.emplace_back(name, value);
}

PreprocessResult Preprocessor::preprocessFile(const std::string& inputFile) const {
    PreprocessResult result;

    if (!isAvailable()) {
        result.success = false;
        result.errorMessage = "No suitable C preprocessor found (tried gcc, clang, cl)";
        return result;
    }

    // Check if input file exists
    if (!std::filesystem::exists(inputFile)) {
        result.success = false;
        result.errorMessage = "Input file not found: " + inputFile;
        return result;
    }

    std::string command = buildCommand(inputFile);
    return executeCommand(command);
}

PreprocessResult Preprocessor::preprocessString(const std::string& content,
                                                 const std::string& filename) const {
    PreprocessResult result;

    if (!isAvailable()) {
        result.success = false;
        result.errorMessage = "No suitable C preprocessor found";
        return result;
    }

    // Create a temporary file with the content
    std::string tempFile = std::filesystem::temp_directory_path().string();
#ifdef _WIN32
    tempFile += "\\iborb_idl_temp_";
#else
    tempFile += "/iborb_idl_temp_";
#endif
    tempFile += std::to_string(std::hash<std::string>{}(content));
    tempFile += ".idl";

    {
        std::ofstream ofs(tempFile);
        if (!ofs) {
            result.success = false;
            result.errorMessage = "Failed to create temporary file";
            return result;
        }
        // Add #line directive to preserve original filename in error messages
        ofs << "#line 1 \"" << filename << "\"\n";
        ofs << content;
    }

    result = executeCommand(buildCommand(tempFile));

    // Clean up temporary file
    std::filesystem::remove(tempFile);

    return result;
}

bool Preprocessor::isAvailable() const {
    return !compilerPath_.empty() && commandExists(compilerPath_);
}

std::string Preprocessor::buildCommand(const std::string& inputFile) const {
    std::ostringstream cmd;

    // Helper lambda to quote paths only if they contain spaces
    auto quotePath = [](const std::string& path) -> std::string {
        if (path.find(' ') != std::string::npos) {
            return "\"" + path + "\"";
        }
        return path;
    };

#ifdef _WIN32
    // Check if using MSVC cl.exe
    if (compilerPath_.find("cl") != std::string::npos) {
        cmd << quotePath(compilerPath_) << " /E /nologo";
        for (const auto& path : includePaths_) {
            cmd << " /I" << quotePath(path);
        }
        for (const auto& [name, value] : defines_) {
            cmd << " /D" << name;
            if (!value.empty()) {
                cmd << "=" << value;
            }
        }
        cmd << " " << quotePath(inputFile) << " 2>nul";
    } else {
        // GCC/Clang on Windows (MinGW, MSYS2, etc.)
        cmd << quotePath(compilerPath_) << " -E -x c";
        for (const auto& path : includePaths_) {
            cmd << " -I" << quotePath(path);
        }
        for (const auto& [name, value] : defines_) {
            cmd << " -D" << name;
            if (!value.empty()) {
                cmd << "=" << value;
            }
        }
        cmd << " " << quotePath(inputFile) << " 2>&1";
    }
#else
    // Linux/macOS - use gcc or clang
    cmd << compilerPath_ << " -E -x c";
    for (const auto& path : includePaths_) {
        cmd << " -I\"" << path << "\"";
    }
    for (const auto& [name, value] : defines_) {
        cmd << " -D" << name;
        if (!value.empty()) {
            cmd << "=" << value;
        }
    }
    cmd << " \"" << inputFile << "\" 2>&1";
#endif

    return cmd.str();
}

PreprocessResult Preprocessor::executeCommand(const std::string& command) const {
    PreprocessResult result;
    std::string output;
    std::array<char, 4096> buffer;

    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) {
        result.success = false;
        result.errorMessage = "Failed to execute preprocessor command";
        return result;
    }

    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        output += buffer.data();
    }

    int status = pclose(pipe);

#ifdef _WIN32
    result.exitCode = status;
#else
    result.exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : -1;
#endif

    if (result.exitCode == 0) {
        result.success = true;
        result.output = std::move(output);
    } else {
        result.success = false;
        result.errorMessage = "Preprocessor failed with exit code " + 
                              std::to_string(result.exitCode);
        // Output may contain error messages
        result.output = std::move(output);
    }

    return result;
}

std::string Preprocessor::detectCompiler() {
    // Try compilers in order of preference
    static const std::vector<std::string> candidates = {
#ifdef _WIN32
        "gcc",      // MinGW
        "clang",    // LLVM/Clang
        "cl"        // MSVC (last resort)
#else
        "gcc",
        "clang",
        "cc"
#endif
    };

    for (const auto& compiler : candidates) {
        if (commandExists(compiler)) {
            return compiler;
        }
    }

    return "";
}

bool Preprocessor::commandExists(const std::string& command) {
    std::string testCmd;

#ifdef _WIN32
    testCmd = "where " + command + " >nul 2>&1";
#else
    testCmd = "which " + command + " >/dev/null 2>&1";
#endif

    return std::system(testCmd.c_str()) == 0;
}

} // namespace iborb::preprocessor
