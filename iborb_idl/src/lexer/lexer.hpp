#ifndef IBORB_IDL_LEXER_HPP
#define IBORB_IDL_LEXER_HPP

#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <variant>
#include "ast/ast.hpp"

namespace iborb::lexer {

/**
 * @brief IDL token types
 */
enum class TokenType {
    // End of file
    Eof,

    // Literals
    Identifier,
    IntegerLiteral,
    FloatLiteral,
    StringLiteral,
    CharLiteral,
    WideStringLiteral,
    WideCharLiteral,

    // Keywords
    KwModule,
    KwInterface,
    KwStruct,
    KwUnion,
    KwSwitch,
    KwCase,
    KwDefault,
    KwEnum,
    KwConst,
    KwTypedef,
    KwException,
    KwAttribute,
    KwReadonly,
    KwIn,
    KwOut,
    KwInout,
    KwOneway,
    KwRaises,
    KwContext,
    KwSequence,
    KwString,
    KwWstring,
    KwFixed,
    KwAbstract,
    KwLocal,
    KwNative,
    KwValuetype,
    KwTruncatable,
    KwSupports,
    KwPublic,
    KwPrivate,
    KwFactory,
    KwCustom,

    // Basic types
    KwVoid,
    KwBoolean,
    KwChar,
    KwWchar,
    KwOctet,
    KwShort,
    KwLong,
    KwFloat,
    KwDouble,
    KwUnsigned,
    KwAny,
    KwObject,
    KwTrue,
    KwFalse,

    // Punctuation
    Semicolon,      // ;
    Colon,          // :
    DoubleColon,    // ::
    Comma,          // ,
    LeftBrace,      // {
    RightBrace,     // }
    LeftParen,      // (
    RightParen,     // )
    LeftBracket,    // [
    RightBracket,   // ]
    LeftAngle,      // <
    RightAngle,     // >
    Equals,         // =
    Plus,           // +
    Minus,          // -
    Star,           // *
    Slash,          // /
    Percent,        // %
    Ampersand,      // &
    Pipe,           // |
    Caret,          // ^
    Tilde,          // ~
    LeftShift,      // <<
    RightShift,     // >>

    // Special
    Pragma,
    LineDirective,  // #line directive from preprocessor
    Unknown
};

/**
 * @brief Token value type (variant for different literal types)
 */
using TokenValue = std::variant<
    std::monostate,     // No value
    int64_t,            // Integer value
    uint64_t,           // Unsigned integer value
    double,             // Float value
    std::string,        // String/identifier value
    char                // Character value
>;

/**
 * @brief Token structure
 */
struct Token {
    TokenType type = TokenType::Unknown;
    TokenValue value;
    std::string text;  // Original text
    ast::SourceLocation location;

    Token() = default;
    Token(TokenType t, ast::SourceLocation loc)
        : type(t), location(std::move(loc)) {}
    Token(TokenType t, std::string txt, ast::SourceLocation loc)
        : type(t), text(std::move(txt)), location(std::move(loc)) {}
    Token(TokenType t, TokenValue val, std::string txt, ast::SourceLocation loc)
        : type(t), value(std::move(val)), text(std::move(txt)), location(std::move(loc)) {}

    bool is(TokenType t) const { return type == t; }
    bool isNot(TokenType t) const { return type != t; }
    bool isKeyword() const;
    bool isLiteral() const;
    bool isOperator() const;

    std::string_view typeName() const;
};

/**
 * @brief Lexical error information
 */
struct LexerError {
    std::string message;
    ast::SourceLocation location;
};

/**
 * @brief IDL Lexer (Tokenizer)
 * 
 * Converts IDL source text into a stream of tokens.
 */
class Lexer {
public:
    /**
     * @brief Construct lexer from source code
     * @param source IDL source code
     * @param filename Filename for error reporting
     */
    Lexer(std::string source, std::string filename = "<input>");

    /**
     * @brief Get the next token
     */
    Token nextToken();

    /**
     * @brief Peek at the next token without consuming it
     */
    Token peekToken();

    /**
     * @brief Peek at a token N positions ahead
     */
    Token peekToken(size_t n);

    /**
     * @brief Check if there are more tokens
     */
    bool hasMore() const;

    /**
     * @brief Get all collected errors
     */
    const std::vector<LexerError>& getErrors() const { return errors_; }

    /**
     * @brief Check if any errors occurred
     */
    bool hasErrors() const { return !errors_.empty(); }

    /**
     * @brief Get current source location
     */
    ast::SourceLocation currentLocation() const;

private:
    std::string source_;
    std::string filename_;
    size_t pos_ = 0;
    size_t line_ = 1;
    size_t column_ = 1;
    std::vector<Token> lookahead_;
    std::vector<LexerError> errors_;

    // Character helpers
    char peek() const;
    char peek(size_t n) const;
    char advance();
    bool isAtEnd() const;
    bool match(char expected);

    // Scanning methods
    Token scanToken();
    void skipWhitespaceAndComments();
    void skipLineComment();
    void skipBlockComment();
    
    Token scanIdentifierOrKeyword();
    Token scanNumber();
    Token scanString(bool isWide = false);
    Token scanChar(bool isWide = false);
    Token scanPragma();
    Token scanLineDirective();

    // Error handling
    void addError(const std::string& message);

    // Utility
    static bool isDigit(char c);
    static bool isHexDigit(char c);
    static bool isOctalDigit(char c);
    static bool isAlpha(char c);
    static bool isAlphaNumeric(char c);
    static bool isIdentifierStart(char c);
    static bool isIdentifierChar(char c);

    // Keyword lookup
    static std::optional<TokenType> lookupKeyword(const std::string& name);
};

/**
 * @brief Convert token type to string for debugging
 */
std::string tokenTypeToString(TokenType type);

} // namespace iborb::lexer

#endif // IBORB_IDL_LEXER_HPP
