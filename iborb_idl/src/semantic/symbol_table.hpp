#ifndef IBORB_IDL_SYMBOL_TABLE_HPP
#define IBORB_IDL_SYMBOL_TABLE_HPP

#include <string>
#include <unordered_map>
#include <vector>
#include <memory>
#include <optional>
#include "ast/ast.hpp"

namespace iborb::semantic {

/**
 * @brief Type of symbol stored in the symbol table
 */
enum class SymbolKind {
    Module,
    Interface,
    Struct,
    Union,
    Enum,
    Typedef,
    Exception,
    Constant,
    Operation,
    Attribute,
    Parameter,
    EnumValue
};

/**
 * @brief Symbol information stored in the symbol table
 */
struct Symbol {
    std::string name;
    std::string fullyQualifiedName;
    SymbolKind kind;
    ast::ASTNode* node = nullptr;  // Non-owning pointer to AST node
    std::string scope;  // Parent scope

    Symbol() = default;
    Symbol(std::string n, std::string fqn, SymbolKind k, ast::ASTNode* astNode = nullptr)
        : name(std::move(n)), fullyQualifiedName(std::move(fqn)), kind(k), node(astNode) {}
};

/**
 * @brief Scope in the symbol table (module, interface, etc.)
 */
class Scope {
public:
    std::string name;
    std::string fullyQualifiedName;
    Scope* parent = nullptr;
    std::unordered_map<std::string, Symbol> symbols;
    std::vector<std::unique_ptr<Scope>> children;

    explicit Scope(std::string scopeName = "", Scope* parentScope = nullptr);

    /**
     * @brief Add a symbol to this scope
     * @return true if added successfully, false if duplicate
     */
    bool addSymbol(const Symbol& symbol);

    /**
     * @brief Look up a symbol in this scope only (not parent scopes)
     */
    std::optional<Symbol> lookupLocal(const std::string& name) const;

    /**
     * @brief Look up a symbol in this scope and parent scopes
     */
    std::optional<Symbol> lookup(const std::string& name) const;

    /**
     * @brief Create a child scope
     */
    Scope* createChildScope(const std::string& childName);

    /**
     * @brief Get a child scope by name
     */
    Scope* getChildScope(const std::string& childName) const;
};

/**
 * @brief Symbol table managing nested scopes for IDL semantic analysis
 */
class SymbolTable {
public:
    SymbolTable();

    /**
     * @brief Enter a new scope (module, interface, etc.)
     */
    void enterScope(const std::string& scopeName);

    /**
     * @brief Leave the current scope and return to parent
     */
    void leaveScope();

    /**
     * @brief Add a symbol to the current scope
     * @return true if added, false if duplicate
     */
    bool addSymbol(const std::string& name, SymbolKind kind, ast::ASTNode* node = nullptr);

    /**
     * @brief Look up a symbol by simple name (searches current and parent scopes)
     */
    std::optional<Symbol> lookup(const std::string& name) const;

    /**
     * @brief Look up a symbol by scoped name (e.g., "ModuleA::StructB")
     */
    std::optional<Symbol> lookupScoped(const std::vector<std::string>& parts, 
                                        bool isAbsolute = false) const;

    /**
     * @brief Look up a symbol by fully qualified name string (e.g., "::ModuleA::StructB")
     */
    std::optional<Symbol> lookupQualified(const std::string& qualifiedName) const;

    /**
     * @brief Get the current scope's fully qualified name
     */
    std::string getCurrentScopeName() const;

    /**
     * @brief Get the current scope
     */
    Scope* getCurrentScope() const { return currentScope_; }

    /**
     * @brief Get the global (root) scope
     */
    Scope* getGlobalScope() const { return globalScope_.get(); }

    /**
     * @brief Check if a symbol exists in the current scope (local only)
     */
    bool existsInCurrentScope(const std::string& name) const;

    /**
     * @brief Build fully qualified name for a symbol in current scope
     */
    std::string buildFullyQualifiedName(const std::string& name) const;

private:
    std::unique_ptr<Scope> globalScope_;
    Scope* currentScope_;

    /**
     * @brief Parse a qualified name string into parts
     */
    static std::vector<std::string> parseQualifiedName(const std::string& name);
};

/**
 * @brief Utility to convert SymbolKind to string for debugging
 */
std::string symbolKindToString(SymbolKind kind);

} // namespace iborb::semantic

#endif // IBORB_IDL_SYMBOL_TABLE_HPP
