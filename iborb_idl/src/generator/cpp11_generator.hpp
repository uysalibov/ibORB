#ifndef IBORB_IDL_CPP11_GENERATOR_HPP
#define IBORB_IDL_CPP11_GENERATOR_HPP

#include <string>
#include <sstream>
#include <fstream>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include "ast/ast.hpp"
#include "semantic/symbol_table.hpp"

namespace iborb::generator {

/**
 * @brief Configuration options for code generation
 */
struct GeneratorConfig {
    std::string outputDir = ".";
    std::string headerExtension = ".hpp";
    std::string sourceExtension = ".cpp";
    std::string namespacePrefix = "";
    bool generateImplementation = true;
    bool useSmartPointers = true;
    bool addIncludeGuards = true;
    bool addDoxygen = true;
    std::string indent = "    ";  // 4 spaces
};

/**
 * @brief C++11 Code Generator
 * 
 * Implements the Visitor pattern to traverse the AST and generate
 * C++ code following the IDL to C++11 Language Mapping standard.
 */
class Cpp11Generator : public ast::ASTVisitor {
public:
    /**
     * @brief Construct generator with configuration
     */
    explicit Cpp11Generator(GeneratorConfig config = {});

    /**
     * @brief Set the symbol table for type resolution
     */
    void setSymbolTable(const semantic::SymbolTable* symTable);

    /**
     * @brief Generate code from a translation unit
     * @param unit The parsed AST
     * @return true if generation succeeded
     */
    bool generate(const ast::TranslationUnit& unit);

    /**
     * @brief Get the generated header content
     */
    const std::string& getHeaderContent() const { return headerContent_; }

    /**
     * @brief Get the generated source content
     */
    const std::string& getSourceContent() const { return sourceContent_; }

    /**
     * @brief Get any generation errors
     */
    const std::vector<std::string>& getErrors() const { return errors_; }

    // Visitor interface implementation
    void visit(ast::ModuleNode& node) override;
    void visit(ast::InterfaceNode& node) override;
    void visit(ast::OperationNode& node) override;
    void visit(ast::ParameterNode& node) override;
    void visit(ast::AttributeNode& node) override;
    void visit(ast::StructNode& node) override;
    void visit(ast::StructMemberNode& node) override;
    void visit(ast::TypedefNode& node) override;
    void visit(ast::EnumNode& node) override;
    void visit(ast::ConstNode& node) override;
    void visit(ast::ExceptionNode& node) override;
    void visit(ast::UnionNode& node) override;
    void visit(ast::UnionCaseNode& node) override;
    void visit(ast::BasicTypeNode& node) override;
    void visit(ast::SequenceTypeNode& node) override;
    void visit(ast::StringTypeNode& node) override;
    void visit(ast::ScopedNameNode& node) override;
    void visit(ast::ArrayTypeNode& node) override;

private:
    GeneratorConfig config_;
    const semantic::SymbolTable* symbolTable_ = nullptr;
    std::ostringstream header_;
    std::ostringstream source_;
    std::string headerContent_;
    std::string sourceContent_;
    std::vector<std::string> errors_;
    
    // State tracking
    int indentLevel_ = 0;
    std::vector<std::string> namespaceStack_;
    std::string currentTypeName_;
    bool inInterfaceDecl_ = false;

    // Output helpers
    void indent();
    void outdent();
    std::string getIndent() const;
    void writeHeader(const std::string& text);
    void writeHeaderLine(const std::string& line = "");
    void writeSource(const std::string& text);
    void writeSourceLine(const std::string& line = "");

    // Type mapping
    std::string mapBasicType(ast::BasicType type) const;
    std::string mapType(ast::TypeNode* node);
    std::string mapTypeForParameter(ast::TypeNode* node, ast::ParamDirection dir);
    std::string mapTypeForReturn(ast::TypeNode* node);

    // Code generation helpers
    void generateIncludeGuardBegin(const std::string& filename);
    void generateIncludeGuardEnd();
    void generateIncludes();
    void generateNamespaceBegin(const std::string& name);
    void generateNamespaceEnd();
    void generateStruct(ast::StructNode& node);
    void generateInterface(ast::InterfaceNode& node);
    void generateEnum(ast::EnumNode& node);
    void generateTypedef(ast::TypedefNode& node);
    void generateConst(ast::ConstNode& node);
    void generateException(ast::ExceptionNode& node);
    void generateUnion(ast::UnionNode& node);

    // Utility
    std::string sanitizeIdentifier(const std::string& name) const;
    std::string constValueToString(const ast::ConstValue& value) const;
    std::string makeIncludeGuard(const std::string& filename) const;
    std::string formatSourceLocation(const ast::SourceLocation& loc) const;
    void addError(const std::string& message);
};

} // namespace iborb::generator

#endif // IBORB_IDL_CPP11_GENERATOR_HPP
