#ifndef IBORB_IDL_PARSER_HPP
#define IBORB_IDL_PARSER_HPP

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include "ast/ast.hpp"
#include "lexer/lexer.hpp"
#include "semantic/symbol_table.hpp"

namespace iborb::parser {

/**
 * @brief Parser error information
 */
struct ParserError {
    std::string message;
    ast::SourceLocation location;
    bool isWarning = false;
};

/**
 * @brief IDL Recursive Descent Parser
 * 
 * Parses IDL tokens into an Abstract Syntax Tree while performing
 * basic semantic analysis (symbol table management).
 */
class Parser {
public:
    /**
     * @brief Construct parser from source code
     * @param source IDL source code
     * @param filename Filename for error reporting
     */
    Parser(const std::string& source, const std::string& filename = "<input>");

    /**
     * @brief Parse the entire translation unit
     * @return The root AST node
     */
    ast::TranslationUnit parse();

    /**
     * @brief Get all collected errors
     */
    const std::vector<ParserError>& getErrors() const { return errors_; }

    /**
     * @brief Get all collected warnings
     */
    std::vector<ParserError> getWarnings() const;

    /**
     * @brief Check if any errors occurred
     */
    bool hasErrors() const;

    /**
     * @brief Get the symbol table (for semantic analysis)
     */
    semantic::SymbolTable& getSymbolTable() { return symbolTable_; }
    const semantic::SymbolTable& getSymbolTable() const { return symbolTable_; }

private:
    lexer::Lexer lexer_;
    lexer::Token currentToken_;
    lexer::Token previousToken_;
    std::vector<ParserError> errors_;
    semantic::SymbolTable symbolTable_;
    bool hadError_ = false;
    bool panicMode_ = false;

    // Token helpers
    void advance();
    bool check(lexer::TokenType type) const;
    bool match(lexer::TokenType type);
    bool matchAny(std::initializer_list<lexer::TokenType> types);
    void expect(lexer::TokenType type, const std::string& message);
    void expectSemicolon();

    // Error handling
    void error(const std::string& message);
    void errorAt(const lexer::Token& token, const std::string& message);
    void warning(const std::string& message);
    void synchronize();

    // ========================================================================
    // Grammar Rules (Recursive Descent)
    // ========================================================================

    // Top-level definitions
    ast::ASTPtr<ast::DefinitionNode> parseDefinition();
    ast::ASTPtr<ast::ModuleNode> parseModule();
    ast::ASTPtr<ast::InterfaceNode> parseInterface(bool isAbstract = false, bool isLocal = false);
    ast::ASTPtr<ast::StructNode> parseStruct();
    ast::ASTPtr<ast::UnionNode> parseUnion();
    ast::ASTPtr<ast::EnumNode> parseEnum();
    ast::ASTPtr<ast::TypedefNode> parseTypedef();
    ast::ASTPtr<ast::ConstNode> parseConst();
    ast::ASTPtr<ast::ExceptionNode> parseException();

    // Interface contents
    ast::ASTPtr<ast::OperationNode> parseOperation(ast::ASTPtr<ast::TypeNode> returnType,
                                                    const std::string& name,
                                                    bool isOneway = false);
    ast::ASTPtr<ast::AttributeNode> parseAttribute(bool isReadonly = false);
    ast::ASTPtr<ast::ParameterNode> parseParameter();

    // Struct/Union members
    ast::ASTPtr<ast::StructMemberNode> parseStructMember();
    ast::ASTPtr<ast::UnionCaseNode> parseUnionCase();

    // Type specifications
    ast::ASTPtr<ast::TypeNode> parseTypeSpec();
    ast::ASTPtr<ast::TypeNode> parseSimpleTypeSpec();
    ast::ASTPtr<ast::TypeNode> parseBaseTypeSpec();
    ast::ASTPtr<ast::TypeNode> parseTemplateTypeSpec();
    ast::ASTPtr<ast::TypeNode> parseScopedName();
    ast::ASTPtr<ast::SequenceTypeNode> parseSequenceType();
    ast::ASTPtr<ast::StringTypeNode> parseStringType(bool isWide = false);

    // Declarators (for typedef with arrays)
    struct Declarator {
        std::string name;
        std::vector<size_t> arrayDimensions;
    };
    Declarator parseDeclarator();
    std::vector<Declarator> parseDeclarators();

    // Expressions (for constants and array bounds)
    ast::ConstValue parseConstExpr();
    ast::ConstValue parseOrExpr();
    ast::ConstValue parseXorExpr();
    ast::ConstValue parseAndExpr();
    ast::ConstValue parseShiftExpr();
    ast::ConstValue parseAddExpr();
    ast::ConstValue parseMulExpr();
    ast::ConstValue parseUnaryExpr();
    ast::ConstValue parsePrimaryExpr();

    // Helpers
    std::vector<std::string> parseInheritanceSpec();
    std::vector<std::string> parseRaisesExpr();
    ast::ParamDirection parseParamDirection();
    ast::BasicType parseBasicType();

    // Utility
    bool isTypeKeyword(lexer::TokenType type) const;
    bool isDefinitionStart() const;
    std::string getTokenText() const;
};

} // namespace iborb::parser

#endif // IBORB_IDL_PARSER_HPP
