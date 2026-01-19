#ifndef IBORB_IDL_VISITOR_HPP
#define IBORB_IDL_VISITOR_HPP

namespace iborb::ast {

// Forward declarations of all AST node types
class ModuleNode;
class InterfaceNode;
class OperationNode;
class ParameterNode;
class AttributeNode;
class StructNode;
class StructMemberNode;
class TypedefNode;
class EnumNode;
class ConstNode;
class ExceptionNode;
class UnionNode;
class UnionCaseNode;

// Type nodes
class BasicTypeNode;
class SequenceTypeNode;
class StringTypeNode;
class ScopedNameNode;
class ArrayTypeNode;

/**
 * @brief Visitor interface for AST traversal
 * 
 * Implements the Visitor design pattern to separate algorithms
 * from the object structure they operate on.
 */
class ASTVisitor {
public:
    virtual ~ASTVisitor() = default;

    // Definition nodes
    virtual void visit(ModuleNode& node) = 0;
    virtual void visit(InterfaceNode& node) = 0;
    virtual void visit(OperationNode& node) = 0;
    virtual void visit(ParameterNode& node) = 0;
    virtual void visit(AttributeNode& node) = 0;
    virtual void visit(StructNode& node) = 0;
    virtual void visit(StructMemberNode& node) = 0;
    virtual void visit(TypedefNode& node) = 0;
    virtual void visit(EnumNode& node) = 0;
    virtual void visit(ConstNode& node) = 0;
    virtual void visit(ExceptionNode& node) = 0;
    virtual void visit(UnionNode& node) = 0;
    virtual void visit(UnionCaseNode& node) = 0;

    // Type nodes
    virtual void visit(BasicTypeNode& node) = 0;
    virtual void visit(SequenceTypeNode& node) = 0;
    virtual void visit(StringTypeNode& node) = 0;
    virtual void visit(ScopedNameNode& node) = 0;
    virtual void visit(ArrayTypeNode& node) = 0;
};

/**
 * @brief Const visitor interface for read-only AST traversal
 */
class ConstASTVisitor {
public:
    virtual ~ConstASTVisitor() = default;

    // Definition nodes
    virtual void visit(const ModuleNode& node) = 0;
    virtual void visit(const InterfaceNode& node) = 0;
    virtual void visit(const OperationNode& node) = 0;
    virtual void visit(const ParameterNode& node) = 0;
    virtual void visit(const AttributeNode& node) = 0;
    virtual void visit(const StructNode& node) = 0;
    virtual void visit(const StructMemberNode& node) = 0;
    virtual void visit(const TypedefNode& node) = 0;
    virtual void visit(const EnumNode& node) = 0;
    virtual void visit(const ConstNode& node) = 0;
    virtual void visit(const ExceptionNode& node) = 0;
    virtual void visit(const UnionNode& node) = 0;
    virtual void visit(const UnionCaseNode& node) = 0;

    // Type nodes
    virtual void visit(const BasicTypeNode& node) = 0;
    virtual void visit(const SequenceTypeNode& node) = 0;
    virtual void visit(const StringTypeNode& node) = 0;
    virtual void visit(const ScopedNameNode& node) = 0;
    virtual void visit(const ArrayTypeNode& node) = 0;
};

} // namespace iborb::ast

#endif // IBORB_IDL_VISITOR_HPP
