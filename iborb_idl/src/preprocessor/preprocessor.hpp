#ifndef IBORB_IDL_PREPROCESSOR_HPP
#define IBORB_IDL_PREPROCESSOR_HPP

#include <string>
#include <vector>
#include <optional>

namespace iborb::preprocessor {

/**
 * @brief Result of preprocessing operation
 */
struct PreprocessResult {
    bool success = false;
    std::string output;
    std::string errorMessage;
    int exitCode = 0;
};

/**
 * @brief Cross-platform preprocessor wrapper
 * 
 * Uses the system's GCC or Clang preprocessor to handle #include
 * directives and macro expansion before IDL parsing.
 */
class Preprocessor {
public:
    /**
     * @brief Default constructor using auto-detected compiler
     */
    Preprocessor();

    /**
     * @brief Constructor with explicit compiler path
     * @param compilerPath Path to gcc or clang executable
     */
    explicit Preprocessor(std::string compilerPath);

    /**
     * @brief Add an include directory for the preprocessor
     */
    void addIncludePath(const std::string& path);

    /**
     * @brief Add a macro definition
     * @param name Macro name
     * @param value Optional macro value (empty for flag macros)
     */
    void addDefine(const std::string& name, const std::string& value = "");

    /**
     * @brief Preprocess an IDL file
     * @param inputFile Path to the input IDL file
     * @return PreprocessResult containing the preprocessed output or error
     */
    PreprocessResult preprocessFile(const std::string& inputFile) const;

    /**
     * @brief Preprocess IDL content from a string
     * @param content IDL content to preprocess
     * @param filename Virtual filename for #line directives
     * @return PreprocessResult containing the preprocessed output or error
     */
    PreprocessResult preprocessString(const std::string& content,
                                       const std::string& filename = "<stdin>") const;

    /**
     * @brief Check if a suitable preprocessor is available
     */
    bool isAvailable() const;

    /**
     * @brief Get the compiler path being used
     */
    const std::string& getCompilerPath() const { return compilerPath_; }

private:
    std::string compilerPath_;
    std::vector<std::string> includePaths_;
    std::vector<std::pair<std::string, std::string>> defines_;

    /**
     * @brief Build the preprocessor command line
     */
    std::string buildCommand(const std::string& inputFile) const;

    /**
     * @brief Execute a command and capture output (cross-platform)
     */
    PreprocessResult executeCommand(const std::string& command) const;

    /**
     * @brief Auto-detect available compiler
     */
    static std::string detectCompiler();

    /**
     * @brief Check if a command exists on the system
     */
    static bool commandExists(const std::string& command);
};

} // namespace iborb::preprocessor

#endif // IBORB_IDL_PREPROCESSOR_HPP
