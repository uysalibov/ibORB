#ifndef IBORB_IDL_AST_HPP
#define IBORB_IDL_AST_HPP

#include <string>
#include <vector>
#include <memory>
#include <optional>
#include <variant>
#include "visitor.hpp"

namespace iborb::ast {

/**
 * @brief Source location information for error reporting
 */
struct SourceLocation {
    std::string filename;
    size_t line = 1;
    size_t column = 1;

    std::string toString() const {
        return filename + ":" + std::to_string(line) + ":" + std::to_string(column);
    }
};

/**
 * @brief Base class for all AST nodes
 */
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(ASTVisitor& visitor) = 0;
    virtual void accept(ConstASTVisitor& visitor) const = 0;

    SourceLocation location;

protected:
    ASTNode() = default;
    explicit ASTNode(SourceLocation loc) : location(std::move(loc)) {}
};

// Smart pointer type for AST nodes
template<typename T>
using ASTPtr = std::unique_ptr<T>;

template<typename T>
using ASTList = std::vector<ASTPtr<T>>;

// ============================================================================
// Type Nodes
// ============================================================================

/**
 * @brief Base class for type specification nodes
 */
class TypeNode : public ASTNode {
public:
    using ASTNode::ASTNode;
    
    // Returns the fully qualified C++ type name (set during semantic analysis)
    std::string resolvedCppType;
    std::string resolvedScope;  // Scope where this type is defined
};

/**
 * @brief IDL basic types (long, short, float, double, boolean, char, octet, etc.)
 */
enum class BasicType {
    Void,
    Boolean,
    Char,
    WChar,
    Octet,
    Short,
    UShort,
    Long,
    ULong,
    LongLong,
    ULongLong,
    Float,
    Double,
    LongDouble,
    Any,
    Object
};

class BasicTypeNode : public TypeNode {
public:
    BasicType type;

    explicit BasicTypeNode(BasicType t, SourceLocation loc = {})
        : TypeNode(std::move(loc)), type(t) {}

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }

    static std::string typeToString(BasicType t) {
        switch (t) {
            case BasicType::Void: return "void";
            case BasicType::Boolean: return "boolean";
            case BasicType::Char: return "char";
            case BasicType::WChar: return "wchar";
            case BasicType::Octet: return "octet";
            case BasicType::Short: return "short";
            case BasicType::UShort: return "unsigned short";
            case BasicType::Long: return "long";
            case BasicType::ULong: return "unsigned long";
            case BasicType::LongLong: return "long long";
            case BasicType::ULongLong: return "unsigned long long";
            case BasicType::Float: return "float";
            case BasicType::Double: return "double";
            case BasicType::LongDouble: return "long double";
            case BasicType::Any: return "any";
            case BasicType::Object: return "Object";
        }
        return "unknown";
    }
};

/**
 * @brief Sequence type: sequence<T> or sequence<T, bound>
 */
class SequenceTypeNode : public TypeNode {
public:
    ASTPtr<TypeNode> elementType;
    std::optional<size_t> bound;  // Max size if bounded

    explicit SequenceTypeNode(ASTPtr<TypeNode> elemType, 
                              std::optional<size_t> maxSize = std::nullopt,
                              SourceLocation loc = {})
        : TypeNode(std::move(loc)), elementType(std::move(elemType)), bound(maxSize) {}

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief String type: string or string<bound>
 */
class StringTypeNode : public TypeNode {
public:
    std::optional<size_t> bound;  // Max length if bounded
    bool isWide = false;  // true for wstring

    explicit StringTypeNode(std::optional<size_t> maxLen = std::nullopt,
                           bool wide = false,
                           SourceLocation loc = {})
        : TypeNode(std::move(loc)), bound(maxLen), isWide(wide) {}

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Scoped name for user-defined types (e.g., ModuleA::StructB)
 */
class ScopedNameNode : public TypeNode {
public:
    std::vector<std::string> parts;  // ["ModuleA", "StructB"]
    bool isAbsolute = false;  // true if starts with ::

    explicit ScopedNameNode(std::vector<std::string> nameParts,
                           bool absolute = false,
                           SourceLocation loc = {})
        : TypeNode(std::move(loc)), parts(std::move(nameParts)), isAbsolute(absolute) {}

    std::string toString() const {
        std::string result;
        if (isAbsolute) result = "::";
        for (size_t i = 0; i < parts.size(); ++i) {
            if (i > 0) result += "::";
            result += parts[i];
        }
        return result;
    }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Array type with fixed dimensions
 */
class ArrayTypeNode : public TypeNode {
public:
    ASTPtr<TypeNode> elementType;
    std::vector<size_t> dimensions;

    ArrayTypeNode(ASTPtr<TypeNode> elemType, std::vector<size_t> dims,
                  SourceLocation loc = {})
        : TypeNode(std::move(loc)), elementType(std::move(elemType)), 
          dimensions(std::move(dims)) {}

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

// ============================================================================
// Definition Nodes
// ============================================================================

/**
 * @brief Base class for definitions that can appear in a module/interface
 */
class DefinitionNode : public ASTNode {
public:
    using ASTNode::ASTNode;
    std::string name;
    std::string fullyQualifiedName;  // Set during semantic analysis
};

/**
 * @brief Constant value types
 */
using ConstValue = std::variant<
    int64_t,           // Integer constants
    uint64_t,          // Unsigned integer constants
    double,            // Floating point constants
    std::string,       // String/char constants
    bool               // Boolean constants
>;

/**
 * @brief Constant declaration: const <type> <name> = <value>
 */
class ConstNode : public DefinitionNode {
public:
    ASTPtr<TypeNode> type;
    ConstValue value;

    ConstNode(std::string constName, ASTPtr<TypeNode> constType,
              ConstValue val, SourceLocation loc = {})
        : DefinitionNode(std::move(loc)), type(std::move(constType)), value(std::move(val)) {
        name = std::move(constName);
    }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Struct member
 */
class StructMemberNode : public ASTNode {
public:
    ASTPtr<TypeNode> type;
    std::string name;

    StructMemberNode(ASTPtr<TypeNode> memberType, std::string memberName,
                     SourceLocation loc = {})
        : ASTNode(std::move(loc)), type(std::move(memberType)), 
          name(std::move(memberName)) {}

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Struct definition
 */
class StructNode : public DefinitionNode {
public:
    ASTList<StructMemberNode> members;

    explicit StructNode(std::string structName, SourceLocation loc = {})
        : DefinitionNode(std::move(loc)) {
        name = std::move(structName);
    }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Enum definition
 */
class EnumNode : public DefinitionNode {
public:
    std::vector<std::string> enumerators;

    EnumNode(std::string enumName, std::vector<std::string> values,
             SourceLocation loc = {})
        : DefinitionNode(std::move(loc)), enumerators(std::move(values)) {
        name = std::move(enumName);
    }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Typedef declarator with optional array dimensions
 */
struct TypedefDeclarator {
    std::string name;
    std::vector<size_t> arrayDimensions;  // Empty for non-array typedefs
};

/**
 * @brief Typedef definition
 */
class TypedefNode : public DefinitionNode {
public:
    ASTPtr<TypeNode> originalType;
    std::vector<TypedefDeclarator> declarators;  // Can have multiple: typedef long X, Y[10];

    TypedefNode(ASTPtr<TypeNode> origType, std::vector<TypedefDeclarator> decls,
                SourceLocation loc = {})
        : DefinitionNode(std::move(loc)), originalType(std::move(origType)),
          declarators(std::move(decls)) {
        if (!declarators.empty()) {
            name = declarators[0].name;
        }
    }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Union case label
 */
struct CaseLabel {
    bool isDefault = false;
    ConstValue value;
};

/**
 * @brief Union case (branch)
 */
class UnionCaseNode : public ASTNode {
public:
    std::vector<CaseLabel> labels;
    ASTPtr<TypeNode> type;
    std::string name;

    UnionCaseNode(std::vector<CaseLabel> caseLabels, ASTPtr<TypeNode> memberType,
                  std::string memberName, SourceLocation loc = {})
        : ASTNode(std::move(loc)), labels(std::move(caseLabels)),
          type(std::move(memberType)), name(std::move(memberName)) {}

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Union definition
 */
class UnionNode : public DefinitionNode {
public:
    ASTPtr<TypeNode> discriminatorType;
    ASTList<UnionCaseNode> cases;

    UnionNode(std::string unionName, ASTPtr<TypeNode> discType,
              SourceLocation loc = {})
        : DefinitionNode(std::move(loc)), discriminatorType(std::move(discType)) {
        name = std::move(unionName);
    }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Exception definition (similar to struct)
 */
class ExceptionNode : public DefinitionNode {
public:
    ASTList<StructMemberNode> members;

    explicit ExceptionNode(std::string excName, SourceLocation loc = {})
        : DefinitionNode(std::move(loc)) {
        name = std::move(excName);
    }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Parameter direction
 */
enum class ParamDirection {
    In,
    Out,
    InOut
};

/**
 * @brief Operation parameter
 */
class ParameterNode : public ASTNode {
public:
    ParamDirection direction;
    ASTPtr<TypeNode> type;
    std::string name;

    ParameterNode(ParamDirection dir, ASTPtr<TypeNode> paramType,
                  std::string paramName, SourceLocation loc = {})
        : ASTNode(std::move(loc)), direction(dir), type(std::move(paramType)),
          name(std::move(paramName)) {}

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Interface operation (method)
 */
class OperationNode : public DefinitionNode {
public:
    ASTPtr<TypeNode> returnType;
    ASTList<ParameterNode> parameters;
    std::vector<std::string> raises;  // Exception names
    bool isOneway = false;

    OperationNode(std::string opName, ASTPtr<TypeNode> retType,
                  SourceLocation loc = {})
        : DefinitionNode(std::move(loc)), returnType(std::move(retType)) {
        name = std::move(opName);
    }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Interface attribute
 */
class AttributeNode : public DefinitionNode {
public:
    ASTPtr<TypeNode> type;
    bool isReadonly = false;

    AttributeNode(std::string attrName, ASTPtr<TypeNode> attrType,
                  bool readonly = false, SourceLocation loc = {})
        : DefinitionNode(std::move(loc)), type(std::move(attrType)), 
          isReadonly(readonly) {
        name = std::move(attrName);
    }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Interface definition
 */
class InterfaceNode : public DefinitionNode {
public:
    std::vector<std::string> baseInterfaces;  // Inheritance
    ASTList<DefinitionNode> contents;  // Operations, attributes, nested types
    bool isAbstract = false;
    bool isLocal = false;
    bool isForward = false;  // Forward declaration only

    explicit InterfaceNode(std::string ifaceName, SourceLocation loc = {})
        : DefinitionNode(std::move(loc)) {
        name = std::move(ifaceName);
    }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Module definition
 */
class ModuleNode : public DefinitionNode {
public:
    ASTList<DefinitionNode> definitions;

    explicit ModuleNode(std::string modName, SourceLocation loc = {})
        : DefinitionNode(std::move(loc)) {
        name = std::move(modName);
    }

    void accept(ASTVisitor& visitor) override { visitor.visit(*this); }
    void accept(ConstASTVisitor& visitor) const override { visitor.visit(*this); }
};

/**
 * @brief Root node containing all top-level definitions
 */
class TranslationUnit {
public:
    ASTList<DefinitionNode> definitions;
    std::string filename;

    void accept(ASTVisitor& visitor) {
        for (auto& def : definitions) {
            def->accept(visitor);
        }
    }

    void accept(ConstASTVisitor& visitor) const {
        for (const auto& def : definitions) {
            def->accept(visitor);
        }
    }
};

} // namespace iborb::ast

#endif // IBORB_IDL_AST_HPP
