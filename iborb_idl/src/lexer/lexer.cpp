#include "lexer/lexer.hpp"
#include <unordered_map>
#include <cctype>
#include <cstdlib>
#include <charconv>

namespace iborb::lexer {

// ============================================================================
// Token Methods
// ============================================================================

bool Token::isKeyword() const {
    return type >= TokenType::KwModule && type <= TokenType::KwFalse;
}

bool Token::isLiteral() const {
    return type >= TokenType::Identifier && type <= TokenType::WideCharLiteral;
}

bool Token::isOperator() const {
    return type >= TokenType::Plus && type <= TokenType::RightShift;
}

std::string_view Token::typeName() const {
    return tokenTypeToString(type);
}

// ============================================================================
// Keyword Lookup Table
// ============================================================================

static const std::unordered_map<std::string, TokenType> keywords = {
    {"module", TokenType::KwModule},
    {"interface", TokenType::KwInterface},
    {"struct", TokenType::KwStruct},
    {"union", TokenType::KwUnion},
    {"switch", TokenType::KwSwitch},
    {"case", TokenType::KwCase},
    {"default", TokenType::KwDefault},
    {"enum", TokenType::KwEnum},
    {"const", TokenType::KwConst},
    {"typedef", TokenType::KwTypedef},
    {"exception", TokenType::KwException},
    {"attribute", TokenType::KwAttribute},
    {"readonly", TokenType::KwReadonly},
    {"in", TokenType::KwIn},
    {"out", TokenType::KwOut},
    {"inout", TokenType::KwInout},
    {"oneway", TokenType::KwOneway},
    {"raises", TokenType::KwRaises},
    {"context", TokenType::KwContext},
    {"sequence", TokenType::KwSequence},
    {"string", TokenType::KwString},
    {"wstring", TokenType::KwWstring},
    {"fixed", TokenType::KwFixed},
    {"abstract", TokenType::KwAbstract},
    {"local", TokenType::KwLocal},
    {"native", TokenType::KwNative},
    {"valuetype", TokenType::KwValuetype},
    {"truncatable", TokenType::KwTruncatable},
    {"supports", TokenType::KwSupports},
    {"public", TokenType::KwPublic},
    {"private", TokenType::KwPrivate},
    {"factory", TokenType::KwFactory},
    {"custom", TokenType::KwCustom},
    {"void", TokenType::KwVoid},
    {"boolean", TokenType::KwBoolean},
    {"char", TokenType::KwChar},
    {"wchar", TokenType::KwWchar},
    {"octet", TokenType::KwOctet},
    {"short", TokenType::KwShort},
    {"long", TokenType::KwLong},
    {"float", TokenType::KwFloat},
    {"double", TokenType::KwDouble},
    {"unsigned", TokenType::KwUnsigned},
    {"any", TokenType::KwAny},
    {"Object", TokenType::KwObject},
    {"TRUE", TokenType::KwTrue},
    {"FALSE", TokenType::KwFalse},
    {"true", TokenType::KwTrue},
    {"false", TokenType::KwFalse}
};

std::optional<TokenType> Lexer::lookupKeyword(const std::string& name) {
    auto it = keywords.find(name);
    if (it != keywords.end()) {
        return it->second;
    }
    return std::nullopt;
}

// ============================================================================
// Lexer Implementation
// ============================================================================

Lexer::Lexer(std::string source, std::string filename)
    : source_(std::move(source)), filename_(std::move(filename)) {
}

char Lexer::peek() const {
    if (isAtEnd()) return '\0';
    return source_[pos_];
}

char Lexer::peek(size_t n) const {
    if (pos_ + n >= source_.size()) return '\0';
    return source_[pos_ + n];
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source_[pos_] != expected) {
        return false;
    }
    advance();
    return true;
}

Token Lexer::nextToken() {
    if (!lookahead_.empty()) {
        Token tok = std::move(lookahead_.front());
        lookahead_.erase(lookahead_.begin());
        return tok;
    }
    return scanToken();
}

Token Lexer::peekToken() {
    return peekToken(0);
}

Token Lexer::peekToken(size_t n) {
    while (lookahead_.size() <= n) {
        lookahead_.push_back(scanToken());
    }
    return lookahead_[n];
}

bool Lexer::hasMore() const {
    return !isAtEnd() || !lookahead_.empty();
}

ast::SourceLocation Lexer::currentLocation() const {
    return {filename_, line_, column_};
}

void Lexer::skipWhitespaceAndComments() {
    while (!isAtEnd()) {
        char c = peek();
        switch (c) {
            case ' ':
            case '\t':
            case '\r':
            case '\n':
                advance();
                break;
            case '/':
                if (peek(1) == '/') {
                    skipLineComment();
                } else if (peek(1) == '*') {
                    skipBlockComment();
                } else {
                    return;
                }
                break;
            default:
                return;
        }
    }
}

void Lexer::skipLineComment() {
    // Skip the //
    advance();
    advance();
    while (!isAtEnd() && peek() != '\n') {
        advance();
    }
}

void Lexer::skipBlockComment() {
    // Skip the /*
    advance();
    advance();
    while (!isAtEnd()) {
        if (peek() == '*' && peek(1) == '/') {
            advance();
            advance();
            return;
        }
        advance();
    }
    addError("Unterminated block comment");
}

Token Lexer::scanToken() {
    skipWhitespaceAndComments();

    if (isAtEnd()) {
        return Token(TokenType::Eof, currentLocation());
    }

    auto loc = currentLocation();
    char c = peek();

    // Check for preprocessor directive
    if (c == '#') {
        if (source_.compare(pos_, 5, "#line") == 0 || 
            (source_.compare(pos_, 2, "# ") == 0 && isDigit(peek(2)))) {
            return scanLineDirective();
        }
        if (source_.compare(pos_, 7, "#pragma") == 0) {
            return scanPragma();
        }
        // Skip other preprocessor directives
        while (!isAtEnd() && peek() != '\n') {
            advance();
        }
        return scanToken();
    }

    // Identifiers and keywords
    if (isIdentifierStart(c)) {
        return scanIdentifierOrKeyword();
    }

    // Numbers
    if (isDigit(c)) {
        return scanNumber();
    }

    // Wide string/char literals
    if (c == 'L' && (peek(1) == '"' || peek(1) == '\'')) {
        advance(); // consume 'L'
        if (peek() == '"') {
            return scanString(true);
        } else {
            return scanChar(true);
        }
    }

    // String literals
    if (c == '"') {
        return scanString();
    }

    // Character literals
    if (c == '\'') {
        return scanChar();
    }

    // Operators and punctuation
    advance();
    switch (c) {
        case ';': return Token(TokenType::Semicolon, ";", loc);
        case ',': return Token(TokenType::Comma, ",", loc);
        case '{': return Token(TokenType::LeftBrace, "{", loc);
        case '}': return Token(TokenType::RightBrace, "}", loc);
        case '(': return Token(TokenType::LeftParen, "(", loc);
        case ')': return Token(TokenType::RightParen, ")", loc);
        case '[': return Token(TokenType::LeftBracket, "[", loc);
        case ']': return Token(TokenType::RightBracket, "]", loc);
        case '=': return Token(TokenType::Equals, "=", loc);
        case '+': return Token(TokenType::Plus, "+", loc);
        case '-': return Token(TokenType::Minus, "-", loc);
        case '*': return Token(TokenType::Star, "*", loc);
        case '/': return Token(TokenType::Slash, "/", loc);
        case '%': return Token(TokenType::Percent, "%", loc);
        case '&': return Token(TokenType::Ampersand, "&", loc);
        case '|': return Token(TokenType::Pipe, "|", loc);
        case '^': return Token(TokenType::Caret, "^", loc);
        case '~': return Token(TokenType::Tilde, "~", loc);
        case ':':
            if (match(':')) {
                return Token(TokenType::DoubleColon, "::", loc);
            }
            return Token(TokenType::Colon, ":", loc);
        case '<':
            if (match('<')) {
                return Token(TokenType::LeftShift, "<<", loc);
            }
            return Token(TokenType::LeftAngle, "<", loc);
        case '>':
            if (match('>')) {
                return Token(TokenType::RightShift, ">>", loc);
            }
            return Token(TokenType::RightAngle, ">", loc);
        default:
            addError("Unexpected character: " + std::string(1, c));
            return Token(TokenType::Unknown, std::string(1, c), loc);
    }
}

Token Lexer::scanIdentifierOrKeyword() {
    auto loc = currentLocation();
    std::string text;

    while (!isAtEnd() && isIdentifierChar(peek())) {
        text += advance();
    }

    // Check if it's a keyword
    if (auto kw = lookupKeyword(text)) {
        return Token(*kw, text, loc);
    }

    return Token(TokenType::Identifier, text, text, loc);
}

Token Lexer::scanNumber() {
    auto loc = currentLocation();
    std::string text;
    bool isFloat = false;
    bool isHex = false;
    bool isOctal = false;

    // Check for hex or octal prefix
    if (peek() == '0') {
        text += advance();
        if (peek() == 'x' || peek() == 'X') {
            isHex = true;
            text += advance();
            while (!isAtEnd() && isHexDigit(peek())) {
                text += advance();
            }
        } else if (isOctalDigit(peek())) {
            isOctal = true;
            while (!isAtEnd() && isOctalDigit(peek())) {
                text += advance();
            }
        }
    }

    if (!isHex && !isOctal) {
        // Decimal number
        while (!isAtEnd() && isDigit(peek())) {
            text += advance();
        }

        // Check for fractional part
        if (peek() == '.' && isDigit(peek(1))) {
            isFloat = true;
            text += advance(); // consume '.'
            while (!isAtEnd() && isDigit(peek())) {
                text += advance();
            }
        }

        // Check for exponent
        if (peek() == 'e' || peek() == 'E') {
            isFloat = true;
            text += advance();
            if (peek() == '+' || peek() == '-') {
                text += advance();
            }
            while (!isAtEnd() && isDigit(peek())) {
                text += advance();
            }
        }

        // Check for float suffix
        if (peek() == 'f' || peek() == 'F' || peek() == 'd' || peek() == 'D') {
            isFloat = true;
            text += advance();
        }
    }

    if (isFloat) {
        double value = std::stod(text);
        return Token(TokenType::FloatLiteral, value, text, loc);
    } else {
        int64_t value = 0;
        if (isHex) {
            value = std::stoll(text, nullptr, 16);
        } else if (isOctal) {
            value = std::stoll(text, nullptr, 8);
        } else {
            value = std::stoll(text);
        }
        return Token(TokenType::IntegerLiteral, value, text, loc);
    }
}

Token Lexer::scanString(bool isWide) {
    auto loc = currentLocation();
    std::string text;
    std::string value;

    advance(); // consume opening quote
    text += '"';

    while (!isAtEnd() && peek() != '"') {
        if (peek() == '\n') {
            addError("Unterminated string literal");
            break;
        }
        if (peek() == '\\') {
            text += advance();
            if (isAtEnd()) break;
            char escaped = advance();
            text += escaped;
            switch (escaped) {
                case 'n': value += '\n'; break;
                case 't': value += '\t'; break;
                case 'r': value += '\r'; break;
                case '\\': value += '\\'; break;
                case '"': value += '"'; break;
                case '\'': value += '\''; break;
                case '0': value += '\0'; break;
                case 'x': {
                    // Hex escape
                    std::string hex;
                    while (hex.size() < 2 && !isAtEnd() && isHexDigit(peek())) {
                        hex += advance();
                        text += hex.back();
                    }
                    if (!hex.empty()) {
                        value += static_cast<char>(std::stoi(hex, nullptr, 16));
                    }
                    break;
                }
                default:
                    value += escaped;
                    break;
            }
        } else {
            char c = advance();
            text += c;
            value += c;
        }
    }

    if (!isAtEnd()) {
        text += advance(); // consume closing quote
    }

    TokenType type = isWide ? TokenType::WideStringLiteral : TokenType::StringLiteral;
    return Token(type, value, text, loc);
}

Token Lexer::scanChar(bool isWide) {
    auto loc = currentLocation();
    std::string text;
    char value = '\0';

    advance(); // consume opening quote
    text += '\'';

    if (!isAtEnd() && peek() != '\'') {
        if (peek() == '\\') {
            text += advance();
            if (!isAtEnd()) {
                char escaped = advance();
                text += escaped;
                switch (escaped) {
                    case 'n': value = '\n'; break;
                    case 't': value = '\t'; break;
                    case 'r': value = '\r'; break;
                    case '\\': value = '\\'; break;
                    case '"': value = '"'; break;
                    case '\'': value = '\''; break;
                    case '0': value = '\0'; break;
                    default: value = escaped; break;
                }
            }
        } else {
            value = advance();
            text += value;
        }
    }

    if (!isAtEnd() && peek() == '\'') {
        text += advance();
    } else {
        addError("Unterminated character literal");
    }

    TokenType type = isWide ? TokenType::WideCharLiteral : TokenType::CharLiteral;
    return Token(type, value, text, loc);
}

Token Lexer::scanPragma() {
    auto loc = currentLocation();
    std::string text;

    while (!isAtEnd() && peek() != '\n') {
        text += advance();
    }

    return Token(TokenType::Pragma, text, text, loc);
}

Token Lexer::scanLineDirective() {
    auto loc = currentLocation();
    std::string text;

    // Skip #line or # 
    while (!isAtEnd() && peek() != '\n') {
        text += advance();
    }

    // Parse line number and filename from the directive
    // Format: #line <number> "<filename>" or # <number> "<filename>"
    size_t start = text.find_first_of("0123456789");
    if (start != std::string::npos) {
        size_t end = text.find_first_not_of("0123456789", start);
        std::string lineNumStr = text.substr(start, end - start);
        line_ = std::stoull(lineNumStr);

        // Look for filename
        size_t fnStart = text.find('"', end);
        if (fnStart != std::string::npos) {
            size_t fnEnd = text.find('"', fnStart + 1);
            if (fnEnd != std::string::npos) {
                filename_ = text.substr(fnStart + 1, fnEnd - fnStart - 1);
            }
        }
    }

    return Token(TokenType::LineDirective, text, text, loc);
}

void Lexer::addError(const std::string& message) {
    errors_.push_back({message, currentLocation()});
}

bool Lexer::isDigit(char c) {
    return c >= '0' && c <= '9';
}

bool Lexer::isHexDigit(char c) {
    return isDigit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

bool Lexer::isOctalDigit(char c) {
    return c >= '0' && c <= '7';
}

bool Lexer::isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

bool Lexer::isAlphaNumeric(char c) {
    return isAlpha(c) || isDigit(c);
}

bool Lexer::isIdentifierStart(char c) {
    return isAlpha(c) || c == '_';
}

bool Lexer::isIdentifierChar(char c) {
    return isAlphaNumeric(c) || c == '_';
}

std::string tokenTypeToString(TokenType type) {
    switch (type) {
        case TokenType::Eof: return "EOF";
        case TokenType::Identifier: return "identifier";
        case TokenType::IntegerLiteral: return "integer";
        case TokenType::FloatLiteral: return "float";
        case TokenType::StringLiteral: return "string";
        case TokenType::CharLiteral: return "char";
        case TokenType::WideStringLiteral: return "wstring";
        case TokenType::WideCharLiteral: return "wchar";
        case TokenType::KwModule: return "module";
        case TokenType::KwInterface: return "interface";
        case TokenType::KwStruct: return "struct";
        case TokenType::KwUnion: return "union";
        case TokenType::KwSwitch: return "switch";
        case TokenType::KwCase: return "case";
        case TokenType::KwDefault: return "default";
        case TokenType::KwEnum: return "enum";
        case TokenType::KwConst: return "const";
        case TokenType::KwTypedef: return "typedef";
        case TokenType::KwException: return "exception";
        case TokenType::KwAttribute: return "attribute";
        case TokenType::KwReadonly: return "readonly";
        case TokenType::KwIn: return "in";
        case TokenType::KwOut: return "out";
        case TokenType::KwInout: return "inout";
        case TokenType::KwOneway: return "oneway";
        case TokenType::KwRaises: return "raises";
        case TokenType::KwContext: return "context";
        case TokenType::KwSequence: return "sequence";
        case TokenType::KwString: return "string";
        case TokenType::KwWstring: return "wstring";
        case TokenType::KwFixed: return "fixed";
        case TokenType::KwAbstract: return "abstract";
        case TokenType::KwLocal: return "local";
        case TokenType::KwNative: return "native";
        case TokenType::KwValuetype: return "valuetype";
        case TokenType::KwTruncatable: return "truncatable";
        case TokenType::KwSupports: return "supports";
        case TokenType::KwPublic: return "public";
        case TokenType::KwPrivate: return "private";
        case TokenType::KwFactory: return "factory";
        case TokenType::KwCustom: return "custom";
        case TokenType::KwVoid: return "void";
        case TokenType::KwBoolean: return "boolean";
        case TokenType::KwChar: return "char";
        case TokenType::KwWchar: return "wchar";
        case TokenType::KwOctet: return "octet";
        case TokenType::KwShort: return "short";
        case TokenType::KwLong: return "long";
        case TokenType::KwFloat: return "float";
        case TokenType::KwDouble: return "double";
        case TokenType::KwUnsigned: return "unsigned";
        case TokenType::KwAny: return "any";
        case TokenType::KwObject: return "Object";
        case TokenType::KwTrue: return "TRUE";
        case TokenType::KwFalse: return "FALSE";
        case TokenType::Semicolon: return ";";
        case TokenType::Colon: return ":";
        case TokenType::DoubleColon: return "::";
        case TokenType::Comma: return ",";
        case TokenType::LeftBrace: return "{";
        case TokenType::RightBrace: return "}";
        case TokenType::LeftParen: return "(";
        case TokenType::RightParen: return ")";
        case TokenType::LeftBracket: return "[";
        case TokenType::RightBracket: return "]";
        case TokenType::LeftAngle: return "<";
        case TokenType::RightAngle: return ">";
        case TokenType::Equals: return "=";
        case TokenType::Plus: return "+";
        case TokenType::Minus: return "-";
        case TokenType::Star: return "*";
        case TokenType::Slash: return "/";
        case TokenType::Percent: return "%";
        case TokenType::Ampersand: return "&";
        case TokenType::Pipe: return "|";
        case TokenType::Caret: return "^";
        case TokenType::Tilde: return "~";
        case TokenType::LeftShift: return "<<";
        case TokenType::RightShift: return ">>";
        case TokenType::Pragma: return "#pragma";
        case TokenType::LineDirective: return "#line";
        case TokenType::Unknown: return "unknown";
    }
    return "unknown";
}

} // namespace iborb::lexer
