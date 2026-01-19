#include "semantic/symbol_table.hpp"
#include <sstream>
#include <algorithm>

namespace iborb::semantic {

// ============================================================================
// Scope Implementation
// ============================================================================

Scope::Scope(std::string scopeName, Scope* parentScope)
    : name(std::move(scopeName)), parent(parentScope) {
    if (parent) {
        if (parent->fullyQualifiedName.empty()) {
            fullyQualifiedName = name;
        } else {
            fullyQualifiedName = parent->fullyQualifiedName + "::" + name;
        }
    } else {
        fullyQualifiedName = name;
    }
}

bool Scope::addSymbol(const Symbol& symbol) {
    auto [it, inserted] = symbols.try_emplace(symbol.name, symbol);
    return inserted;
}

std::optional<Symbol> Scope::lookupLocal(const std::string& symbolName) const {
    auto it = symbols.find(symbolName);
    if (it != symbols.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<Symbol> Scope::lookup(const std::string& symbolName) const {
    // First check this scope
    if (auto sym = lookupLocal(symbolName)) {
        return sym;
    }
    // Then check parent scope
    if (parent) {
        return parent->lookup(symbolName);
    }
    return std::nullopt;
}

Scope* Scope::createChildScope(const std::string& childName) {
    auto child = std::make_unique<Scope>(childName, this);
    Scope* ptr = child.get();
    children.push_back(std::move(child));
    return ptr;
}

Scope* Scope::getChildScope(const std::string& childName) const {
    for (const auto& child : children) {
        if (child->name == childName) {
            return child.get();
        }
    }
    return nullptr;
}

// ============================================================================
// SymbolTable Implementation
// ============================================================================

SymbolTable::SymbolTable() {
    globalScope_ = std::make_unique<Scope>("", nullptr);
    currentScope_ = globalScope_.get();
}

void SymbolTable::enterScope(const std::string& scopeName) {
    // Check if the scope already exists (for reopening modules)
    Scope* existing = currentScope_->getChildScope(scopeName);
    if (existing) {
        currentScope_ = existing;
    } else {
        currentScope_ = currentScope_->createChildScope(scopeName);
    }
}

void SymbolTable::leaveScope() {
    if (currentScope_->parent) {
        currentScope_ = currentScope_->parent;
    }
}

bool SymbolTable::addSymbol(const std::string& name, SymbolKind kind, ast::ASTNode* node) {
    Symbol sym;
    sym.name = name;
    sym.kind = kind;
    sym.node = node;
    sym.scope = currentScope_->fullyQualifiedName;
    sym.fullyQualifiedName = buildFullyQualifiedName(name);
    return currentScope_->addSymbol(sym);
}

std::optional<Symbol> SymbolTable::lookup(const std::string& name) const {
    return currentScope_->lookup(name);
}

std::optional<Symbol> SymbolTable::lookupScoped(const std::vector<std::string>& parts,
                                                 bool isAbsolute) const {
    if (parts.empty()) {
        return std::nullopt;
    }

    // Start from global scope if absolute, otherwise from current scope
    const Scope* searchScope = isAbsolute ? globalScope_.get() : currentScope_;

    // If not absolute, first try to find the first part as a symbol in current/parent scopes
    if (!isAbsolute && parts.size() == 1) {
        return searchScope->lookup(parts[0]);
    }

    // For multi-part names, we need to traverse scopes
    if (!isAbsolute) {
        // Try to find the first part in current scope hierarchy
        const Scope* scope = currentScope_;
        while (scope) {
            // Check if this scope has a child matching the first part
            Scope* child = scope->getChildScope(parts[0]);
            if (child) {
                searchScope = child;
                break;
            }
            // Or if it's a symbol
            if (parts.size() == 1) {
                if (auto sym = scope->lookupLocal(parts[0])) {
                    return sym;
                }
            }
            scope = scope->parent;
        }
        if (!scope && parts.size() > 1) {
            // Couldn't find the first part
            return std::nullopt;
        }
    }

    // Navigate through scopes
    const Scope* currentSearch = isAbsolute ? globalScope_.get() : searchScope;
    for (size_t i = (isAbsolute ? 0 : 1); i < parts.size() - 1; ++i) {
        Scope* child = currentSearch->getChildScope(parts[i]);
        if (!child) {
            return std::nullopt;
        }
        currentSearch = child;
    }

    // Look up the final name
    return currentSearch->lookupLocal(parts.back());
}

std::optional<Symbol> SymbolTable::lookupQualified(const std::string& qualifiedName) const {
    auto parts = parseQualifiedName(qualifiedName);
    bool isAbsolute = qualifiedName.size() >= 2 && 
                      qualifiedName[0] == ':' && qualifiedName[1] == ':';
    return lookupScoped(parts, isAbsolute);
}

std::string SymbolTable::getCurrentScopeName() const {
    return currentScope_->fullyQualifiedName;
}

bool SymbolTable::existsInCurrentScope(const std::string& name) const {
    return currentScope_->lookupLocal(name).has_value();
}

std::string SymbolTable::buildFullyQualifiedName(const std::string& name) const {
    if (currentScope_->fullyQualifiedName.empty()) {
        return name;
    }
    return currentScope_->fullyQualifiedName + "::" + name;
}

std::vector<std::string> SymbolTable::parseQualifiedName(const std::string& name) {
    std::vector<std::string> parts;
    std::string current;
    
    size_t i = 0;
    // Skip leading ::
    if (name.size() >= 2 && name[0] == ':' && name[1] == ':') {
        i = 2;
    }

    while (i < name.size()) {
        if (i + 1 < name.size() && name[i] == ':' && name[i + 1] == ':') {
            if (!current.empty()) {
                parts.push_back(current);
                current.clear();
            }
            i += 2;
        } else {
            current += name[i];
            ++i;
        }
    }

    if (!current.empty()) {
        parts.push_back(current);
    }

    return parts;
}

std::string symbolKindToString(SymbolKind kind) {
    switch (kind) {
        case SymbolKind::Module:    return "module";
        case SymbolKind::Interface: return "interface";
        case SymbolKind::Struct:    return "struct";
        case SymbolKind::Union:     return "union";
        case SymbolKind::Enum:      return "enum";
        case SymbolKind::Typedef:   return "typedef";
        case SymbolKind::Exception: return "exception";
        case SymbolKind::Constant:  return "constant";
        case SymbolKind::Operation: return "operation";
        case SymbolKind::Attribute: return "attribute";
        case SymbolKind::Parameter: return "parameter";
        case SymbolKind::EnumValue: return "enum value";
    }
    return "unknown";
}

} // namespace iborb::semantic
