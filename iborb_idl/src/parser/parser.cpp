#include "parser/parser.hpp"
#include <stdexcept>
#include <sstream>

namespace iborb::parser {

using namespace ast;
using namespace lexer;
using namespace semantic;

Parser::Parser(const std::string& source, const std::string& filename)
    : lexer_(source, filename) {
    advance(); // Prime the first token
}

TranslationUnit Parser::parse() {
    TranslationUnit unit;
    unit.filename = currentToken_.location.filename;

    while (!check(TokenType::Eof)) {
        // Skip any line directives at top level
        if (check(TokenType::LineDirective)) {
            advance();
            continue;
        }

        if (auto def = parseDefinition()) {
            unit.definitions.push_back(std::move(def));
        } else {
            // Error recovery - skip to next definition
            synchronize();
        }
    }

    return unit;
}

std::vector<ParserError> Parser::getWarnings() const {
    std::vector<ParserError> warnings;
    for (const auto& err : errors_) {
        if (err.isWarning) {
            warnings.push_back(err);
        }
    }
    return warnings;
}

bool Parser::hasErrors() const {
    for (const auto& err : errors_) {
        if (!err.isWarning) {
            return true;
        }
    }
    return false;
}

// ============================================================================
// Token Helpers
// ============================================================================

void Parser::advance() {
    previousToken_ = currentToken_;
    
    while (true) {
        currentToken_ = lexer_.nextToken();
        
        // Skip line directives but update location info
        if (currentToken_.type == TokenType::LineDirective) {
            continue;
        }
        
        if (currentToken_.type != TokenType::Unknown) {
            break;
        }
        
        // Report lexer errors
        for (const auto& err : lexer_.getErrors()) {
            errors_.push_back({err.message, err.location, false});
        }
    }
}

bool Parser::check(TokenType type) const {
    return currentToken_.type == type;
}

bool Parser::match(TokenType type) {
    if (check(type)) {
        advance();
        return true;
    }
    return false;
}

bool Parser::matchAny(std::initializer_list<TokenType> types) {
    for (auto type : types) {
        if (match(type)) {
            return true;
        }
    }
    return false;
}

void Parser::expect(TokenType type, const std::string& message) {
    if (check(type)) {
        advance();
        return;
    }
    errorAt(currentToken_, message);
}

void Parser::expectSemicolon() {
    expect(TokenType::Semicolon, "Expected ';'");
}

// ============================================================================
// Error Handling
// ============================================================================

void Parser::error(const std::string& message) {
    errorAt(currentToken_, message);
}

void Parser::errorAt(const Token& token, const std::string& message) {
    if (panicMode_) return; // Don't cascade errors
    panicMode_ = true;
    hadError_ = true;

    std::ostringstream oss;
    oss << token.location.toString() << ": error: " << message;
    if (token.type == TokenType::Eof) {
        oss << " at end of file";
    } else if (token.type != TokenType::Unknown) {
        oss << " (got '" << token.text << "')";
    }

    errors_.push_back({oss.str(), token.location, false});
}

void Parser::warning(const std::string& message) {
    std::ostringstream oss;
    oss << currentToken_.location.toString() << ": warning: " << message;
    errors_.push_back({oss.str(), currentToken_.location, true});
}

void Parser::synchronize() {
    panicMode_ = false;

    while (!check(TokenType::Eof)) {
        if (previousToken_.type == TokenType::Semicolon) {
            return;
        }
        if (previousToken_.type == TokenType::RightBrace) {
            // Check if followed by semicolon
            if (check(TokenType::Semicolon)) {
                advance();
            }
            return;
        }

        // Start of a new definition
        if (isDefinitionStart()) {
            return;
        }

        advance();
    }
}

bool Parser::isDefinitionStart() const {
    switch (currentToken_.type) {
        case TokenType::KwModule:
        case TokenType::KwInterface:
        case TokenType::KwStruct:
        case TokenType::KwUnion:
        case TokenType::KwEnum:
        case TokenType::KwTypedef:
        case TokenType::KwConst:
        case TokenType::KwException:
        case TokenType::KwAbstract:
        case TokenType::KwLocal:
            return true;
        default:
            return false;
    }
}

// ============================================================================
// Definition Parsing
// ============================================================================

ASTPtr<DefinitionNode> Parser::parseDefinition() {
    auto loc = currentToken_.location;

    // Handle modifiers
    bool isAbstract = match(TokenType::KwAbstract);
    bool isLocal = match(TokenType::KwLocal);

    if (check(TokenType::KwModule)) {
        if (isAbstract || isLocal) {
            error("'abstract' and 'local' cannot be applied to modules");
        }
        return parseModule();
    }
    
    if (check(TokenType::KwInterface)) {
        return parseInterface(isAbstract, isLocal);
    }
    
    if (isAbstract || isLocal) {
        error("'abstract' and 'local' can only be applied to interfaces");
    }

    if (check(TokenType::KwStruct)) return parseStruct();
    if (check(TokenType::KwUnion)) return parseUnion();
    if (check(TokenType::KwEnum)) return parseEnum();
    if (check(TokenType::KwTypedef)) return parseTypedef();
    if (check(TokenType::KwConst)) return parseConst();
    if (check(TokenType::KwException)) return parseException();

    // Forward declaration
    if (check(TokenType::Identifier) || isTypeKeyword(currentToken_.type)) {
        // Could be a forward interface declaration without 'interface' keyword
        // Not standard but some parsers allow it
    }

    error("Expected definition (module, interface, struct, etc.)");
    return nullptr;
}

ASTPtr<ModuleNode> Parser::parseModule() {
    auto loc = currentToken_.location;
    expect(TokenType::KwModule, "Expected 'module'");

    if (!check(TokenType::Identifier)) {
        error("Expected module name");
        return nullptr;
    }
    std::string name = currentToken_.text;
    advance();

    // Register module in symbol table
    symbolTable_.addSymbol(name, SymbolKind::Module);
    symbolTable_.enterScope(name);

    auto node = std::make_unique<ModuleNode>(name, loc);
    node->fullyQualifiedName = symbolTable_.getCurrentScopeName();

    expect(TokenType::LeftBrace, "Expected '{' after module name");

    while (!check(TokenType::RightBrace) && !check(TokenType::Eof)) {
        if (auto def = parseDefinition()) {
            node->definitions.push_back(std::move(def));
        } else {
            synchronize();
        }
    }

    expect(TokenType::RightBrace, "Expected '}' at end of module");
    expectSemicolon();

    symbolTable_.leaveScope();
    return node;
}

ASTPtr<InterfaceNode> Parser::parseInterface(bool isAbstract, bool isLocal) {
    auto loc = currentToken_.location;
    expect(TokenType::KwInterface, "Expected 'interface'");

    if (!check(TokenType::Identifier)) {
        error("Expected interface name");
        return nullptr;
    }
    std::string name = currentToken_.text;
    advance();

    auto node = std::make_unique<InterfaceNode>(name, loc);
    node->isAbstract = isAbstract;
    node->isLocal = isLocal;

    // Check for forward declaration
    if (check(TokenType::Semicolon)) {
        advance();
        node->isForward = true;
        symbolTable_.addSymbol(name, SymbolKind::Interface, node.get());
        node->fullyQualifiedName = symbolTable_.buildFullyQualifiedName(name);
        return node;
    }

    // Parse inheritance
    if (check(TokenType::Colon)) {
        node->baseInterfaces = parseInheritanceSpec();
    }

    // Register in symbol table and enter scope
    symbolTable_.addSymbol(name, SymbolKind::Interface, node.get());
    node->fullyQualifiedName = symbolTable_.buildFullyQualifiedName(name);
    symbolTable_.enterScope(name);

    expect(TokenType::LeftBrace, "Expected '{' after interface name");

    // Parse interface body
    while (!check(TokenType::RightBrace) && !check(TokenType::Eof)) {
        // Parse interface member
        bool readonly = match(TokenType::KwReadonly);
        bool oneway = match(TokenType::KwOneway);

        if (check(TokenType::KwAttribute)) {
            if (oneway) {
                error("'oneway' cannot be applied to attributes");
            }
            auto attr = parseAttribute(readonly);
            if (attr) {
                node->contents.push_back(std::move(attr));
            }
        } else if (isDefinitionStart()) {
            // Nested type definition
            if (readonly || oneway) {
                error("'readonly' and 'oneway' can only be applied to attributes and operations");
            }
            if (auto def = parseDefinition()) {
                node->contents.push_back(std::move(def));
            }
        } else {
            // Operation
            if (readonly) {
                error("'readonly' can only be applied to attributes");
            }
            auto retType = parseTypeSpec();
            if (!retType) {
                synchronize();
                continue;
            }

            if (!check(TokenType::Identifier)) {
                error("Expected operation name");
                synchronize();
                continue;
            }
            std::string opName = currentToken_.text;
            advance();

            auto op = parseOperation(std::move(retType), opName, oneway);
            if (op) {
                node->contents.push_back(std::move(op));
            }
        }
    }

    expect(TokenType::RightBrace, "Expected '}' at end of interface");
    expectSemicolon();

    symbolTable_.leaveScope();
    return node;
}

ASTPtr<StructNode> Parser::parseStruct() {
    auto loc = currentToken_.location;
    expect(TokenType::KwStruct, "Expected 'struct'");

    if (!check(TokenType::Identifier)) {
        error("Expected struct name");
        return nullptr;
    }
    std::string name = currentToken_.text;
    advance();

    // Check for forward declaration
    if (check(TokenType::Semicolon)) {
        advance();
        auto node = std::make_unique<StructNode>(name, loc);
        symbolTable_.addSymbol(name, SymbolKind::Struct, node.get());
        node->fullyQualifiedName = symbolTable_.buildFullyQualifiedName(name);
        return node;
    }

    symbolTable_.addSymbol(name, SymbolKind::Struct);
    symbolTable_.enterScope(name);

    auto node = std::make_unique<StructNode>(name, loc);
    node->fullyQualifiedName = symbolTable_.getCurrentScopeName();

    expect(TokenType::LeftBrace, "Expected '{' after struct name");

    while (!check(TokenType::RightBrace) && !check(TokenType::Eof)) {
        if (auto member = parseStructMember()) {
            node->members.push_back(std::move(member));
        } else {
            synchronize();
        }
    }

    expect(TokenType::RightBrace, "Expected '}' at end of struct");
    expectSemicolon();

    symbolTable_.leaveScope();
    return node;
}

ASTPtr<UnionNode> Parser::parseUnion() {
    auto loc = currentToken_.location;
    expect(TokenType::KwUnion, "Expected 'union'");

    if (!check(TokenType::Identifier)) {
        error("Expected union name");
        return nullptr;
    }
    std::string name = currentToken_.text;
    advance();

    expect(TokenType::KwSwitch, "Expected 'switch' after union name");
    expect(TokenType::LeftParen, "Expected '(' after 'switch'");

    auto discType = parseTypeSpec();
    if (!discType) {
        error("Expected discriminator type");
        return nullptr;
    }

    expect(TokenType::RightParen, "Expected ')' after discriminator type");

    symbolTable_.addSymbol(name, SymbolKind::Union);
    symbolTable_.enterScope(name);

    auto node = std::make_unique<UnionNode>(name, std::move(discType), loc);
    node->fullyQualifiedName = symbolTable_.getCurrentScopeName();

    expect(TokenType::LeftBrace, "Expected '{' after union switch");

    while (!check(TokenType::RightBrace) && !check(TokenType::Eof)) {
        if (auto caseNode = parseUnionCase()) {
            node->cases.push_back(std::move(caseNode));
        } else {
            synchronize();
        }
    }

    expect(TokenType::RightBrace, "Expected '}' at end of union");
    expectSemicolon();

    symbolTable_.leaveScope();
    return node;
}

ASTPtr<EnumNode> Parser::parseEnum() {
    auto loc = currentToken_.location;
    expect(TokenType::KwEnum, "Expected 'enum'");

    if (!check(TokenType::Identifier)) {
        error("Expected enum name");
        return nullptr;
    }
    std::string name = currentToken_.text;
    advance();

    expect(TokenType::LeftBrace, "Expected '{' after enum name");

    std::vector<std::string> values;
    do {
        if (!check(TokenType::Identifier)) {
            error("Expected enumerator name");
            break;
        }
        values.push_back(currentToken_.text);
        advance();
    } while (match(TokenType::Comma));

    expect(TokenType::RightBrace, "Expected '}' at end of enum");
    expectSemicolon();

    auto node = std::make_unique<EnumNode>(name, std::move(values), loc);
    symbolTable_.addSymbol(name, SymbolKind::Enum, node.get());
    node->fullyQualifiedName = symbolTable_.buildFullyQualifiedName(name);

    // Register enum values
    for (const auto& val : node->enumerators) {
        symbolTable_.addSymbol(val, SymbolKind::EnumValue);
    }

    return node;
}

ASTPtr<TypedefNode> Parser::parseTypedef() {
    auto loc = currentToken_.location;
    expect(TokenType::KwTypedef, "Expected 'typedef'");

    auto type = parseTypeSpec();
    if (!type) {
        error("Expected type specification");
        return nullptr;
    }

    auto declarators = parseDeclarators();
    if (declarators.empty()) {
        error("Expected declarator");
        return nullptr;
    }

    expectSemicolon();

    std::vector<ast::TypedefDeclarator> astDeclarators;
    for (const auto& decl : declarators) {
        ast::TypedefDeclarator astDecl;
        astDecl.name = decl.name;
        astDecl.arrayDimensions = decl.arrayDimensions;
        astDeclarators.push_back(std::move(astDecl));
        symbolTable_.addSymbol(decl.name, SymbolKind::Typedef);
    }

    auto node = std::make_unique<TypedefNode>(std::move(type), std::move(astDeclarators), loc);
    node->fullyQualifiedName = symbolTable_.buildFullyQualifiedName(node->name);
    return node;
}

ASTPtr<ConstNode> Parser::parseConst() {
    auto loc = currentToken_.location;
    expect(TokenType::KwConst, "Expected 'const'");

    auto type = parseTypeSpec();
    if (!type) {
        error("Expected const type");
        return nullptr;
    }

    if (!check(TokenType::Identifier)) {
        error("Expected const name");
        return nullptr;
    }
    std::string name = currentToken_.text;
    advance();

    expect(TokenType::Equals, "Expected '=' after const name");

    auto value = parseConstExpr();

    expectSemicolon();

    auto node = std::make_unique<ConstNode>(name, std::move(type), std::move(value), loc);
    symbolTable_.addSymbol(name, SymbolKind::Constant, node.get());
    node->fullyQualifiedName = symbolTable_.buildFullyQualifiedName(name);
    return node;
}

ASTPtr<ExceptionNode> Parser::parseException() {
    auto loc = currentToken_.location;
    expect(TokenType::KwException, "Expected 'exception'");

    if (!check(TokenType::Identifier)) {
        error("Expected exception name");
        return nullptr;
    }
    std::string name = currentToken_.text;
    advance();

    symbolTable_.addSymbol(name, SymbolKind::Exception);
    symbolTable_.enterScope(name);

    auto node = std::make_unique<ExceptionNode>(name, loc);
    node->fullyQualifiedName = symbolTable_.getCurrentScopeName();

    expect(TokenType::LeftBrace, "Expected '{' after exception name");

    while (!check(TokenType::RightBrace) && !check(TokenType::Eof)) {
        if (auto member = parseStructMember()) {
            node->members.push_back(std::move(member));
        } else {
            synchronize();
        }
    }

    expect(TokenType::RightBrace, "Expected '}' at end of exception");
    expectSemicolon();

    symbolTable_.leaveScope();
    return node;
}

// ============================================================================
// Interface Members
// ============================================================================

ASTPtr<OperationNode> Parser::parseOperation(ASTPtr<TypeNode> returnType,
                                              const std::string& name,
                                              bool isOneway) {
    auto loc = previousToken_.location;

    auto node = std::make_unique<OperationNode>(name, std::move(returnType), loc);
    node->isOneway = isOneway;
    node->fullyQualifiedName = symbolTable_.buildFullyQualifiedName(name);

    expect(TokenType::LeftParen, "Expected '(' after operation name");

    // Parse parameters
    if (!check(TokenType::RightParen)) {
        do {
            if (auto param = parseParameter()) {
                node->parameters.push_back(std::move(param));
            }
        } while (match(TokenType::Comma));
    }

    expect(TokenType::RightParen, "Expected ')' after parameters");

    // Parse raises clause
    if (check(TokenType::KwRaises)) {
        node->raises = parseRaisesExpr();
    }

    expectSemicolon();

    symbolTable_.addSymbol(name, SymbolKind::Operation, node.get());
    return node;
}

ASTPtr<AttributeNode> Parser::parseAttribute(bool isReadonly) {
    auto loc = currentToken_.location;
    expect(TokenType::KwAttribute, "Expected 'attribute'");

    auto type = parseTypeSpec();
    if (!type) {
        error("Expected attribute type");
        return nullptr;
    }

    if (!check(TokenType::Identifier)) {
        error("Expected attribute name");
        return nullptr;
    }
    std::string name = currentToken_.text;
    advance();

    expectSemicolon();

    auto node = std::make_unique<AttributeNode>(name, std::move(type), isReadonly, loc);
    symbolTable_.addSymbol(name, SymbolKind::Attribute, node.get());
    node->fullyQualifiedName = symbolTable_.buildFullyQualifiedName(name);
    return node;
}

ASTPtr<ParameterNode> Parser::parseParameter() {
    auto loc = currentToken_.location;
    
    auto direction = parseParamDirection();
    
    auto type = parseTypeSpec();
    if (!type) {
        error("Expected parameter type");
        return nullptr;
    }

    if (!check(TokenType::Identifier)) {
        error("Expected parameter name");
        return nullptr;
    }
    std::string name = currentToken_.text;
    advance();

    return std::make_unique<ParameterNode>(direction, std::move(type), name, loc);
}

ASTPtr<StructMemberNode> Parser::parseStructMember() {
    auto loc = currentToken_.location;

    auto type = parseTypeSpec();
    if (!type) {
        return nullptr;
    }

    auto declarators = parseDeclarators();
    if (declarators.empty()) {
        error("Expected member name");
        return nullptr;
    }

    expectSemicolon();

    // For simplicity, we create a member for each declarator
    // In a full implementation, you'd handle multiple declarators
    const auto& decl = declarators[0];

    // Handle array dimensions
    ASTPtr<TypeNode> memberType;
    if (!decl.arrayDimensions.empty()) {
        memberType = std::make_unique<ArrayTypeNode>(
            std::move(type), decl.arrayDimensions, loc);
    } else {
        memberType = std::move(type);
    }

    return std::make_unique<StructMemberNode>(std::move(memberType), decl.name, loc);
}

ASTPtr<UnionCaseNode> Parser::parseUnionCase() {
    auto loc = currentToken_.location;
    std::vector<CaseLabel> labels;

    // Parse case labels
    while (check(TokenType::KwCase) || check(TokenType::KwDefault)) {
        CaseLabel label;
        if (match(TokenType::KwDefault)) {
            label.isDefault = true;
            expect(TokenType::Colon, "Expected ':' after 'default'");
        } else {
            advance(); // consume 'case'
            label.value = parseConstExpr();
            expect(TokenType::Colon, "Expected ':' after case value");
        }
        labels.push_back(label);
    }

    if (labels.empty()) {
        error("Expected 'case' or 'default'");
        return nullptr;
    }

    auto type = parseTypeSpec();
    if (!type) {
        error("Expected type in union case");
        return nullptr;
    }

    if (!check(TokenType::Identifier)) {
        error("Expected member name in union case");
        return nullptr;
    }
    std::string name = currentToken_.text;
    advance();

    expectSemicolon();

    return std::make_unique<UnionCaseNode>(std::move(labels), std::move(type), name, loc);
}

// ============================================================================
// Type Parsing
// ============================================================================

ASTPtr<TypeNode> Parser::parseTypeSpec() {
    if (check(TokenType::KwSequence)) {
        return parseSequenceType();
    }
    if (check(TokenType::KwString)) {
        return parseStringType(false);
    }
    if (check(TokenType::KwWstring)) {
        return parseStringType(true);
    }
    return parseSimpleTypeSpec();
}

ASTPtr<TypeNode> Parser::parseSimpleTypeSpec() {
    // Check for basic types
    if (isTypeKeyword(currentToken_.type)) {
        return parseBaseTypeSpec();
    }

    // Scoped name (user-defined type)
    if (check(TokenType::Identifier) || check(TokenType::DoubleColon)) {
        return parseScopedName();
    }

    error("Expected type specification");
    return nullptr;
}

ASTPtr<TypeNode> Parser::parseBaseTypeSpec() {
    auto loc = currentToken_.location;
    auto basicType = parseBasicType();
    return std::make_unique<BasicTypeNode>(basicType, loc);
}

ASTPtr<SequenceTypeNode> Parser::parseSequenceType() {
    auto loc = currentToken_.location;
    expect(TokenType::KwSequence, "Expected 'sequence'");
    expect(TokenType::LeftAngle, "Expected '<' after 'sequence'");

    auto elemType = parseTypeSpec();
    if (!elemType) {
        error("Expected element type in sequence");
        return nullptr;
    }

    std::optional<size_t> bound;
    if (match(TokenType::Comma)) {
        auto boundExpr = parseConstExpr();
        if (auto* val = std::get_if<int64_t>(&boundExpr)) {
            bound = static_cast<size_t>(*val);
        } else if (auto* uval = std::get_if<uint64_t>(&boundExpr)) {
            bound = static_cast<size_t>(*uval);
        }
    }

    expect(TokenType::RightAngle, "Expected '>' at end of sequence type");

    return std::make_unique<SequenceTypeNode>(std::move(elemType), bound, loc);
}

ASTPtr<StringTypeNode> Parser::parseStringType(bool isWide) {
    auto loc = currentToken_.location;
    advance(); // consume 'string' or 'wstring'

    std::optional<size_t> bound;
    if (match(TokenType::LeftAngle)) {
        auto boundExpr = parseConstExpr();
        if (auto* val = std::get_if<int64_t>(&boundExpr)) {
            bound = static_cast<size_t>(*val);
        } else if (auto* uval = std::get_if<uint64_t>(&boundExpr)) {
            bound = static_cast<size_t>(*uval);
        }
        expect(TokenType::RightAngle, "Expected '>' at end of string bound");
    }

    return std::make_unique<StringTypeNode>(bound, isWide, loc);
}

ASTPtr<TypeNode> Parser::parseScopedName() {
    auto loc = currentToken_.location;
    std::vector<std::string> parts;
    bool isAbsolute = match(TokenType::DoubleColon);

    if (!check(TokenType::Identifier)) {
        error("Expected identifier in scoped name");
        return nullptr;
    }

    do {
        if (!check(TokenType::Identifier)) {
            error("Expected identifier after '::'");
            break;
        }
        parts.push_back(currentToken_.text);
        advance();
    } while (match(TokenType::DoubleColon));

    return std::make_unique<ScopedNameNode>(std::move(parts), isAbsolute, loc);
}

// ============================================================================
// Declarators
// ============================================================================

Parser::Declarator Parser::parseDeclarator() {
    Declarator decl;

    if (!check(TokenType::Identifier)) {
        error("Expected identifier");
        return decl;
    }
    decl.name = currentToken_.text;
    advance();

    // Parse array dimensions
    while (match(TokenType::LeftBracket)) {
        auto sizeExpr = parseConstExpr();
        size_t size = 0;
        if (auto* val = std::get_if<int64_t>(&sizeExpr)) {
            size = static_cast<size_t>(*val);
        } else if (auto* uval = std::get_if<uint64_t>(&sizeExpr)) {
            size = static_cast<size_t>(*uval);
        }
        decl.arrayDimensions.push_back(size);
        expect(TokenType::RightBracket, "Expected ']'");
    }

    return decl;
}

std::vector<Parser::Declarator> Parser::parseDeclarators() {
    std::vector<Declarator> decls;
    
    decls.push_back(parseDeclarator());
    
    while (match(TokenType::Comma)) {
        decls.push_back(parseDeclarator());
    }

    return decls;
}

// ============================================================================
// Expression Parsing (for constants)
// ============================================================================

ConstValue Parser::parseConstExpr() {
    return parseOrExpr();
}

ConstValue Parser::parseOrExpr() {
    auto left = parseXorExpr();

    while (match(TokenType::Pipe)) {
        auto right = parseXorExpr();
        // Compute result
        if (auto* l = std::get_if<int64_t>(&left)) {
            if (auto* r = std::get_if<int64_t>(&right)) {
                left = *l | *r;
            }
        }
    }

    return left;
}

ConstValue Parser::parseXorExpr() {
    auto left = parseAndExpr();

    while (match(TokenType::Caret)) {
        auto right = parseAndExpr();
        if (auto* l = std::get_if<int64_t>(&left)) {
            if (auto* r = std::get_if<int64_t>(&right)) {
                left = *l ^ *r;
            }
        }
    }

    return left;
}

ConstValue Parser::parseAndExpr() {
    auto left = parseShiftExpr();

    while (match(TokenType::Ampersand)) {
        auto right = parseShiftExpr();
        if (auto* l = std::get_if<int64_t>(&left)) {
            if (auto* r = std::get_if<int64_t>(&right)) {
                left = *l & *r;
            }
        }
    }

    return left;
}

ConstValue Parser::parseShiftExpr() {
    auto left = parseAddExpr();

    while (true) {
        if (match(TokenType::LeftShift)) {
            auto right = parseAddExpr();
            if (auto* l = std::get_if<int64_t>(&left)) {
                if (auto* r = std::get_if<int64_t>(&right)) {
                    left = *l << *r;
                }
            }
        } else if (match(TokenType::RightShift)) {
            auto right = parseAddExpr();
            if (auto* l = std::get_if<int64_t>(&left)) {
                if (auto* r = std::get_if<int64_t>(&right)) {
                    left = *l >> *r;
                }
            }
        } else {
            break;
        }
    }

    return left;
}

ConstValue Parser::parseAddExpr() {
    auto left = parseMulExpr();

    while (true) {
        if (match(TokenType::Plus)) {
            auto right = parseMulExpr();
            // Handle different types
            if (auto* l = std::get_if<int64_t>(&left)) {
                if (auto* r = std::get_if<int64_t>(&right)) {
                    left = *l + *r;
                }
            } else if (auto* l = std::get_if<double>(&left)) {
                if (auto* r = std::get_if<double>(&right)) {
                    left = *l + *r;
                }
            }
        } else if (match(TokenType::Minus)) {
            auto right = parseMulExpr();
            if (auto* l = std::get_if<int64_t>(&left)) {
                if (auto* r = std::get_if<int64_t>(&right)) {
                    left = *l - *r;
                }
            } else if (auto* l = std::get_if<double>(&left)) {
                if (auto* r = std::get_if<double>(&right)) {
                    left = *l - *r;
                }
            }
        } else {
            break;
        }
    }

    return left;
}

ConstValue Parser::parseMulExpr() {
    auto left = parseUnaryExpr();

    while (true) {
        if (match(TokenType::Star)) {
            auto right = parseUnaryExpr();
            if (auto* l = std::get_if<int64_t>(&left)) {
                if (auto* r = std::get_if<int64_t>(&right)) {
                    left = *l * *r;
                }
            } else if (auto* l = std::get_if<double>(&left)) {
                if (auto* r = std::get_if<double>(&right)) {
                    left = *l * *r;
                }
            }
        } else if (match(TokenType::Slash)) {
            auto right = parseUnaryExpr();
            if (auto* l = std::get_if<int64_t>(&left)) {
                if (auto* r = std::get_if<int64_t>(&right)) {
                    if (*r != 0) left = *l / *r;
                }
            } else if (auto* l = std::get_if<double>(&left)) {
                if (auto* r = std::get_if<double>(&right)) {
                    left = *l / *r;
                }
            }
        } else if (match(TokenType::Percent)) {
            auto right = parseUnaryExpr();
            if (auto* l = std::get_if<int64_t>(&left)) {
                if (auto* r = std::get_if<int64_t>(&right)) {
                    if (*r != 0) left = *l % *r;
                }
            }
        } else {
            break;
        }
    }

    return left;
}

ConstValue Parser::parseUnaryExpr() {
    if (match(TokenType::Minus)) {
        auto val = parseUnaryExpr();
        if (auto* v = std::get_if<int64_t>(&val)) {
            return -(*v);
        } else if (auto* v = std::get_if<double>(&val)) {
            return -(*v);
        }
        return val;
    }
    if (match(TokenType::Plus)) {
        return parseUnaryExpr();
    }
    if (match(TokenType::Tilde)) {
        auto val = parseUnaryExpr();
        if (auto* v = std::get_if<int64_t>(&val)) {
            return ~(*v);
        }
        return val;
    }

    return parsePrimaryExpr();
}

ConstValue Parser::parsePrimaryExpr() {
    // Parenthesized expression
    if (match(TokenType::LeftParen)) {
        auto val = parseConstExpr();
        expect(TokenType::RightParen, "Expected ')'");
        return val;
    }

    // Literals
    if (check(TokenType::IntegerLiteral)) {
        auto val = std::get<int64_t>(currentToken_.value);
        advance();
        return val;
    }
    if (check(TokenType::FloatLiteral)) {
        auto val = std::get<double>(currentToken_.value);
        advance();
        return val;
    }
    if (check(TokenType::StringLiteral) || check(TokenType::WideStringLiteral)) {
        auto val = std::get<std::string>(currentToken_.value);
        advance();
        return val;
    }
    if (check(TokenType::CharLiteral) || check(TokenType::WideCharLiteral)) {
        auto val = std::get<char>(currentToken_.value);
        advance();
        return std::string(1, val);
    }
    if (match(TokenType::KwTrue)) {
        return true;
    }
    if (match(TokenType::KwFalse)) {
        return false;
    }

    // Scoped name (constant reference)
    if (check(TokenType::Identifier) || check(TokenType::DoubleColon)) {
        std::vector<std::string> parts;
        bool isAbsolute = match(TokenType::DoubleColon);

        do {
            if (!check(TokenType::Identifier)) break;
            parts.push_back(currentToken_.text);
            advance();
        } while (match(TokenType::DoubleColon));

        // Look up the constant in symbol table
        if (auto sym = symbolTable_.lookupScoped(parts, isAbsolute)) {
            if (sym->kind == SymbolKind::Constant && sym->node) {
                auto* constNode = static_cast<ConstNode*>(sym->node);
                return constNode->value;
            }
            if (sym->kind == SymbolKind::EnumValue) {
                // Return the enum value index (simplified)
                return int64_t(0);
            }
        }

        // Return 0 if not found (simplified error handling)
        warning("Unknown constant: " + parts.back());
        return int64_t(0);
    }

    error("Expected expression");
    return int64_t(0);
}

// ============================================================================
// Helpers
// ============================================================================

std::vector<std::string> Parser::parseInheritanceSpec() {
    std::vector<std::string> bases;
    
    expect(TokenType::Colon, "Expected ':' for inheritance");

    do {
        std::string baseName;
        if (match(TokenType::DoubleColon)) {
            baseName = "::";
        }
        
        if (!check(TokenType::Identifier)) {
            error("Expected base interface name");
            break;
        }
        baseName += currentToken_.text;
        advance();

        while (match(TokenType::DoubleColon)) {
            baseName += "::";
            if (!check(TokenType::Identifier)) {
                error("Expected identifier after '::'");
                break;
            }
            baseName += currentToken_.text;
            advance();
        }

        bases.push_back(baseName);
    } while (match(TokenType::Comma));

    return bases;
}

std::vector<std::string> Parser::parseRaisesExpr() {
    std::vector<std::string> exceptions;
    
    expect(TokenType::KwRaises, "Expected 'raises'");
    expect(TokenType::LeftParen, "Expected '(' after 'raises'");

    if (!check(TokenType::RightParen)) {
        do {
            std::string excName;
            if (match(TokenType::DoubleColon)) {
                excName = "::";
            }
            
            if (!check(TokenType::Identifier)) {
                error("Expected exception name");
                break;
            }
            excName += currentToken_.text;
            advance();

            while (match(TokenType::DoubleColon)) {
                excName += "::";
                if (!check(TokenType::Identifier)) break;
                excName += currentToken_.text;
                advance();
            }

            exceptions.push_back(excName);
        } while (match(TokenType::Comma));
    }

    expect(TokenType::RightParen, "Expected ')' after raises list");

    return exceptions;
}

ParamDirection Parser::parseParamDirection() {
    if (match(TokenType::KwIn)) {
        return ParamDirection::In;
    }
    if (match(TokenType::KwOut)) {
        return ParamDirection::Out;
    }
    if (match(TokenType::KwInout)) {
        return ParamDirection::InOut;
    }
    // Default to 'in' if not specified
    return ParamDirection::In;
}

BasicType Parser::parseBasicType() {
    if (match(TokenType::KwVoid)) return BasicType::Void;
    if (match(TokenType::KwBoolean)) return BasicType::Boolean;
    if (match(TokenType::KwChar)) return BasicType::Char;
    if (match(TokenType::KwWchar)) return BasicType::WChar;
    if (match(TokenType::KwOctet)) return BasicType::Octet;
    if (match(TokenType::KwAny)) return BasicType::Any;
    if (match(TokenType::KwObject)) return BasicType::Object;
    if (match(TokenType::KwFloat)) return BasicType::Float;
    
    if (match(TokenType::KwDouble)) {
        return BasicType::Double;
    }

    bool isUnsigned = match(TokenType::KwUnsigned);

    if (match(TokenType::KwShort)) {
        return isUnsigned ? BasicType::UShort : BasicType::Short;
    }
    
    if (match(TokenType::KwLong)) {
        if (match(TokenType::KwLong)) {
            return isUnsigned ? BasicType::ULongLong : BasicType::LongLong;
        }
        if (match(TokenType::KwDouble)) {
            return BasicType::LongDouble;
        }
        return isUnsigned ? BasicType::ULong : BasicType::Long;
    }

    if (isUnsigned) {
        error("Expected 'short' or 'long' after 'unsigned'");
    }

    return BasicType::Void;
}

bool Parser::isTypeKeyword(TokenType type) const {
    switch (type) {
        case TokenType::KwVoid:
        case TokenType::KwBoolean:
        case TokenType::KwChar:
        case TokenType::KwWchar:
        case TokenType::KwOctet:
        case TokenType::KwShort:
        case TokenType::KwLong:
        case TokenType::KwFloat:
        case TokenType::KwDouble:
        case TokenType::KwUnsigned:
        case TokenType::KwAny:
        case TokenType::KwObject:
        case TokenType::KwString:
        case TokenType::KwWstring:
        case TokenType::KwSequence:
            return true;
        default:
            return false;
    }
}

std::string Parser::getTokenText() const {
    return currentToken_.text;
}

} // namespace iborb::parser
