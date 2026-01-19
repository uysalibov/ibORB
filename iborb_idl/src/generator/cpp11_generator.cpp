#include "generator/cpp11_generator.hpp"
#include <algorithm>
#include <cctype>
#include <iomanip>

namespace iborb::generator {

using namespace ast;

Cpp11Generator::Cpp11Generator(GeneratorConfig config)
    : config_(std::move(config)) {
}

void Cpp11Generator::setSymbolTable(const semantic::SymbolTable* symTable) {
    symbolTable_ = symTable;
}

bool Cpp11Generator::generate(const TranslationUnit& unit) {
    errors_.clear();
    header_.str("");
    source_.str("");
    indentLevel_ = 0;
    namespaceStack_.clear();

    // Extract base filename
    std::filesystem::path path(unit.filename);
    std::string baseName = path.stem().string();

    // Generate header file
    if (config_.addIncludeGuards) {
        generateIncludeGuardBegin(baseName);
    }

    generateIncludes();
    writeHeaderLine();

    // Process all definitions
    for (const auto& def : unit.definitions) {
        def->accept(*this);
    }

    if (config_.addIncludeGuards) {
        generateIncludeGuardEnd();
    }

    headerContent_ = header_.str();
    sourceContent_ = source_.str();

    // Write files if output directory is specified
    if (!config_.outputDir.empty()) {
        std::filesystem::path outDir(config_.outputDir);
        std::filesystem::create_directories(outDir);

        std::string headerPath = (outDir / (baseName + config_.headerExtension)).string();
        std::ofstream headerFile(headerPath);
        if (headerFile) {
            headerFile << headerContent_;
        } else {
            addError("Failed to write header file: " + headerPath);
        }

        if (config_.generateImplementation && !sourceContent_.empty()) {
            std::string sourcePath = (outDir / (baseName + config_.sourceExtension)).string();
            std::ofstream sourceFile(sourcePath);
            if (sourceFile) {
                sourceFile << sourceContent_;
            } else {
                addError("Failed to write source file: " + sourcePath);
            }
        }
    }

    return errors_.empty();
}

// ============================================================================
// Visitor Implementation
// ============================================================================

void Cpp11Generator::visit(ModuleNode& node) {
    generateNamespaceBegin(node.name);

    for (auto& def : node.definitions) {
        def->accept(*this);
    }

    generateNamespaceEnd();
}

void Cpp11Generator::visit(InterfaceNode& node) {
    if (node.isForward) {
        // Forward declaration
        writeHeaderLine("class " + node.name + ";");
        writeHeaderLine();
        return;
    }

    generateInterface(node);
}

void Cpp11Generator::visit(OperationNode& node) {
    // Operations are handled within generateInterface
}

void Cpp11Generator::visit(ParameterNode& node) {
    // Parameters are handled within operation generation
}

void Cpp11Generator::visit(AttributeNode& node) {
    // Attributes are handled within generateInterface
}

void Cpp11Generator::visit(StructNode& node) {
    generateStruct(node);
}

void Cpp11Generator::visit(StructMemberNode& node) {
    // Members are handled within generateStruct
}

void Cpp11Generator::visit(TypedefNode& node) {
    generateTypedef(node);
}

void Cpp11Generator::visit(EnumNode& node) {
    generateEnum(node);
}

void Cpp11Generator::visit(ConstNode& node) {
    generateConst(node);
}

void Cpp11Generator::visit(ExceptionNode& node) {
    generateException(node);
}

void Cpp11Generator::visit(UnionNode& node) {
    generateUnion(node);
}

void Cpp11Generator::visit(UnionCaseNode& node) {
    // Union cases are handled within generateUnion
}

void Cpp11Generator::visit(BasicTypeNode& node) {
    currentTypeName_ = mapBasicType(node.type);
}

void Cpp11Generator::visit(SequenceTypeNode& node) {
    std::string elemType = mapType(node.elementType.get());
    currentTypeName_ = "std::vector<" + elemType + ">";
}

void Cpp11Generator::visit(StringTypeNode& node) {
    if (node.isWide) {
        currentTypeName_ = "std::wstring";
    } else {
        currentTypeName_ = "std::string";
    }
}

void Cpp11Generator::visit(ScopedNameNode& node) {
    // Build the C++ scoped name
    std::string result;
    if (node.isAbsolute) {
        result = "::";
    }
    for (size_t i = 0; i < node.parts.size(); ++i) {
        if (i > 0) result += "::";
        result += node.parts[i];
    }
    currentTypeName_ = result;
}

void Cpp11Generator::visit(ArrayTypeNode& node) {
    std::string elemType = mapType(node.elementType.get());
    // Use std::array for fixed-size arrays
    for (auto it = node.dimensions.rbegin(); it != node.dimensions.rend(); ++it) {
        elemType = "std::array<" + elemType + ", " + std::to_string(*it) + ">";
    }
    currentTypeName_ = elemType;
}

// ============================================================================
// Output Helpers
// ============================================================================

void Cpp11Generator::indent() {
    ++indentLevel_;
}

void Cpp11Generator::outdent() {
    if (indentLevel_ > 0) {
        --indentLevel_;
    }
}

std::string Cpp11Generator::getIndent() const {
    std::string result;
    for (int i = 0; i < indentLevel_; ++i) {
        result += config_.indent;
    }
    return result;
}

void Cpp11Generator::writeHeader(const std::string& text) {
    header_ << text;
}

void Cpp11Generator::writeHeaderLine(const std::string& line) {
    if (!line.empty()) {
        header_ << getIndent() << line;
    }
    header_ << "\n";
}

void Cpp11Generator::writeSource(const std::string& text) {
    source_ << text;
}

void Cpp11Generator::writeSourceLine(const std::string& line) {
    if (!line.empty()) {
        source_ << getIndent() << line;
    }
    source_ << "\n";
}

// ============================================================================
// Type Mapping
// ============================================================================

std::string Cpp11Generator::mapBasicType(BasicType type) const {
    switch (type) {
        case BasicType::Void:       return "void";
        case BasicType::Boolean:    return "bool";
        case BasicType::Char:       return "char";
        case BasicType::WChar:      return "wchar_t";
        case BasicType::Octet:      return "uint8_t";
        case BasicType::Short:      return "int16_t";
        case BasicType::UShort:     return "uint16_t";
        case BasicType::Long:       return "int32_t";
        case BasicType::ULong:      return "uint32_t";
        case BasicType::LongLong:   return "int64_t";
        case BasicType::ULongLong:  return "uint64_t";
        case BasicType::Float:      return "float";
        case BasicType::Double:     return "double";
        case BasicType::LongDouble: return "long double";
        case BasicType::Any:        return "std::any";
        case BasicType::Object:     return "Object";
    }
    return "void";
}

std::string Cpp11Generator::mapType(TypeNode* node) {
    if (!node) return "void";
    
    currentTypeName_.clear();
    node->accept(*this);
    return currentTypeName_;
}

std::string Cpp11Generator::mapTypeForParameter(TypeNode* node, ParamDirection dir) {
    std::string baseType = mapType(node);
    
    // For 'in' parameters, use const reference for complex types
    if (dir == ParamDirection::In) {
        // Check if it's a simple type (basic types that should be passed by value)
        if (auto* basic = dynamic_cast<BasicTypeNode*>(node)) {
            switch (basic->type) {
                case BasicType::Boolean:
                case BasicType::Char:
                case BasicType::WChar:
                case BasicType::Octet:
                case BasicType::Short:
                case BasicType::UShort:
                case BasicType::Long:
                case BasicType::ULong:
                case BasicType::LongLong:
                case BasicType::ULongLong:
                case BasicType::Float:
                case BasicType::Double:
                case BasicType::LongDouble:
                    return baseType;  // Pass by value
                default:
                    break;
            }
        }
        // Complex types: pass by const reference
        return "const " + baseType + "&";
    }
    
    // For 'out' and 'inout' parameters, use reference
    return baseType + "&";
}

std::string Cpp11Generator::mapTypeForReturn(TypeNode* node) {
    return mapType(node);
}

// ============================================================================
// Code Generation Helpers
// ============================================================================

void Cpp11Generator::generateIncludeGuardBegin(const std::string& filename) {
    std::string guard = makeIncludeGuard(filename);
    writeHeaderLine("#ifndef " + guard);
    writeHeaderLine("#define " + guard);
    writeHeaderLine();
}

void Cpp11Generator::generateIncludeGuardEnd() {
    writeHeaderLine();
    std::string guard = makeIncludeGuard("");
    writeHeaderLine("#endif // Include guard");
}

void Cpp11Generator::generateIncludes() {
    writeHeaderLine("#include <cstdint>");
    writeHeaderLine("#include <string>");
    writeHeaderLine("#include <vector>");
    writeHeaderLine("#include <array>");
    writeHeaderLine("#include <memory>");
    writeHeaderLine("#include <stdexcept>");
}

void Cpp11Generator::generateNamespaceBegin(const std::string& name) {
    writeHeaderLine();
    writeHeaderLine("namespace " + name + " {");
    writeHeaderLine();
    namespaceStack_.push_back(name);
    
    if (config_.generateImplementation) {
        writeSourceLine();
        writeSourceLine("namespace " + name + " {");
        writeSourceLine();
    }
}

void Cpp11Generator::generateNamespaceEnd() {
    if (!namespaceStack_.empty()) {
        writeHeaderLine();
        writeHeaderLine("} // namespace " + namespaceStack_.back());
        
        if (config_.generateImplementation) {
            writeSourceLine();
            writeSourceLine("} // namespace " + namespaceStack_.back());
        }
        
        namespaceStack_.pop_back();
    }
}

void Cpp11Generator::generateStruct(StructNode& node) {
    if (config_.addDoxygen) {
        writeHeaderLine("/**");
        writeHeaderLine(" * @brief IDL struct " + node.name);
        writeHeaderLine(" */");
    }
    
    writeHeaderLine("struct " + node.name + " {");
    indent();

    for (const auto& member : node.members) {
        std::string type = mapType(member->type.get());
        writeHeaderLine(type + " " + member->name + ";");
    }

    // Generate equality operator
    writeHeaderLine();
    writeHeaderLine("bool operator==(const " + node.name + "& other) const {");
    indent();
    if (node.members.empty()) {
        writeHeaderLine("(void)other;");
        writeHeaderLine("return true;");
    } else {
        std::string comparison;
        for (size_t i = 0; i < node.members.size(); ++i) {
            if (i > 0) comparison += " && ";
            comparison += node.members[i]->name + " == other." + node.members[i]->name;
        }
        writeHeaderLine("return " + comparison + ";");
    }
    outdent();
    writeHeaderLine("}");

    writeHeaderLine();
    writeHeaderLine("bool operator!=(const " + node.name + "& other) const {");
    indent();
    writeHeaderLine("return !(*this == other);");
    outdent();
    writeHeaderLine("}");

    outdent();
    writeHeaderLine("};");
    writeHeaderLine();
}

void Cpp11Generator::generateInterface(InterfaceNode& node) {
    if (config_.addDoxygen) {
        writeHeaderLine("/**");
        writeHeaderLine(" * @brief IDL interface " + node.name);
        if (node.isAbstract) {
            writeHeaderLine(" * @note This is an abstract interface");
        }
        writeHeaderLine(" */");
    }

    // Build class declaration
    std::string decl = "class " + node.name;
    if (!node.baseInterfaces.empty()) {
        decl += " : ";
        for (size_t i = 0; i < node.baseInterfaces.size(); ++i) {
            if (i > 0) decl += ", ";
            decl += "public virtual " + node.baseInterfaces[i];
        }
    }
    
    writeHeaderLine(decl + " {");
    writeHeaderLine("public:");
    indent();

    // Virtual destructor
    writeHeaderLine("virtual ~" + node.name + "() = default;");
    writeHeaderLine();

    inInterfaceDecl_ = true;

    // Generate operations and attributes
    for (const auto& content : node.contents) {
        if (auto* op = dynamic_cast<OperationNode*>(content.get())) {
            // Generate operation signature
            std::string returnType = mapTypeForReturn(op->returnType.get());
            std::string sig = "virtual " + returnType + " " + op->name + "(";

            for (size_t i = 0; i < op->parameters.size(); ++i) {
                if (i > 0) sig += ", ";
                auto& param = op->parameters[i];
                sig += mapTypeForParameter(param->type.get(), param->direction);
                sig += " " + param->name;
            }
            sig += ") = 0;";

            if (config_.addDoxygen && !op->parameters.empty()) {
                writeHeaderLine("/**");
                writeHeaderLine(" * @brief " + op->name + " operation");
                for (const auto& param : op->parameters) {
                    std::string dirStr;
                    switch (param->direction) {
                        case ParamDirection::In: dirStr = "[in]"; break;
                        case ParamDirection::Out: dirStr = "[out]"; break;
                        case ParamDirection::InOut: dirStr = "[in,out]"; break;
                    }
                    writeHeaderLine(" * @param " + param->name + " " + dirStr);
                }
                writeHeaderLine(" */");
            }
            writeHeaderLine(sig);
            writeHeaderLine();
        }
        else if (auto* attr = dynamic_cast<AttributeNode*>(content.get())) {
            // Generate getter
            std::string type = mapType(attr->type.get());
            
            if (config_.addDoxygen) {
                writeHeaderLine("/**");
                writeHeaderLine(" * @brief Get " + attr->name + " attribute");
                writeHeaderLine(" */");
            }
            writeHeaderLine("virtual " + type + " " + attr->name + "() const = 0;");

            // Generate setter if not readonly
            if (!attr->isReadonly) {
                if (config_.addDoxygen) {
                    writeHeaderLine("/**");
                    writeHeaderLine(" * @brief Set " + attr->name + " attribute");
                    writeHeaderLine(" */");
                }
                writeHeaderLine("virtual void " + attr->name + "(const " + type + "& value) = 0;");
            }
            writeHeaderLine();
        }
        else if (auto* nestedStruct = dynamic_cast<StructNode*>(content.get())) {
            // Nested type
            outdent();
            writeHeaderLine();
            generateStruct(*nestedStruct);
            indent();
        }
        else if (auto* nestedEnum = dynamic_cast<EnumNode*>(content.get())) {
            outdent();
            writeHeaderLine();
            generateEnum(*nestedEnum);
            indent();
        }
    }

    inInterfaceDecl_ = false;

    outdent();
    writeHeaderLine("};");
    writeHeaderLine();

    // Generate smart pointer typedef
    if (config_.useSmartPointers) {
        writeHeaderLine("using " + node.name + "Ptr = std::shared_ptr<" + node.name + ">;");
        writeHeaderLine();
    }
}

void Cpp11Generator::generateEnum(EnumNode& node) {
    if (config_.addDoxygen) {
        writeHeaderLine("/**");
        writeHeaderLine(" * @brief IDL enum " + node.name);
        writeHeaderLine(" */");
    }
    
    writeHeaderLine("enum class " + node.name + " {");
    indent();

    for (size_t i = 0; i < node.enumerators.size(); ++i) {
        std::string line = node.enumerators[i];
        if (i < node.enumerators.size() - 1) {
            line += ",";
        }
        writeHeaderLine(line);
    }

    outdent();
    writeHeaderLine("};");
    writeHeaderLine();
}

void Cpp11Generator::generateTypedef(TypedefNode& node) {
    std::string baseType = mapType(node.originalType.get());

    for (const auto& decl : node.declarators) {
        std::string finalType = baseType;
        
        // Handle array dimensions: typedef octet UUID[16] -> std::array<uint8_t, 16>
        if (!decl.arrayDimensions.empty()) {
            // Build nested std::array from innermost to outermost
            for (auto it = decl.arrayDimensions.rbegin(); it != decl.arrayDimensions.rend(); ++it) {
                finalType = "std::array<" + finalType + ", " + std::to_string(*it) + ">";
            }
        }
        
        writeHeaderLine("using " + decl.name + " = " + finalType + ";");
    }
    writeHeaderLine();
}

void Cpp11Generator::generateConst(ConstNode& node) {
    std::string type = mapType(node.type.get());
    std::string value = constValueToString(node.value);

    writeHeaderLine("constexpr " + type + " " + node.name + " = " + value + ";");
    writeHeaderLine();
}

void Cpp11Generator::generateException(ExceptionNode& node) {
    if (config_.addDoxygen) {
        writeHeaderLine("/**");
        writeHeaderLine(" * @brief IDL exception " + node.name);
        writeHeaderLine(" */");
    }
    
    writeHeaderLine("class " + node.name + " : public std::exception {");
    writeHeaderLine("public:");
    indent();

    // Members
    for (const auto& member : node.members) {
        std::string type = mapType(member->type.get());
        writeHeaderLine(type + " " + member->name + ";");
    }

    if (!node.members.empty()) {
        writeHeaderLine();
    }

    // Constructor
    if (!node.members.empty()) {
        std::string params;
        std::string inits;
        for (size_t i = 0; i < node.members.size(); ++i) {
            if (i > 0) {
                params += ", ";
                inits += ", ";
            }
            std::string type = mapType(node.members[i]->type.get());
            params += "const " + type + "& " + node.members[i]->name + "_";
            inits += node.members[i]->name + "(" + node.members[i]->name + "_)";
        }
        writeHeaderLine(node.name + "(" + params + ")");
        writeHeaderLine("    : " + inits + " {}");
        writeHeaderLine();
    }

    // Default constructor
    writeHeaderLine(node.name + "() = default;");
    writeHeaderLine();

    // what() method
    writeHeaderLine("const char* what() const noexcept override {");
    indent();
    writeHeaderLine("return \"" + node.name + "\";");
    outdent();
    writeHeaderLine("}");

    outdent();
    writeHeaderLine("};");
    writeHeaderLine();
}

void Cpp11Generator::generateUnion(UnionNode& node) {
    if (config_.addDoxygen) {
        writeHeaderLine("/**");
        writeHeaderLine(" * @brief IDL union " + node.name);
        writeHeaderLine(" */");
    }

    std::string discType = mapType(node.discriminatorType.get());

    writeHeaderLine("class " + node.name + " {");
    writeHeaderLine("public:");
    indent();

    // Discriminator getter/setter
    writeHeaderLine(discType + " _d() const { return discriminator_; }");
    writeHeaderLine("void _d(" + discType + " d) { discriminator_ = d; }");
    writeHeaderLine();

    // Generate accessors for each case
    for (const auto& caseNode : node.cases) {
        std::string type = mapType(caseNode->type.get());
        std::string memberName = caseNode->name;

        // Getter
        writeHeaderLine(type + " " + memberName + "() const { return " + memberName + "_; }");
        // Setter
        writeHeaderLine("void " + memberName + "(const " + type + "& value) { " + 
                       memberName + "_ = value; }");
        writeHeaderLine();
    }

    writeHeaderLine("private:");
    indent();
    writeHeaderLine(discType + " discriminator_;");

    // Member storage (using std::variant would be more type-safe)
    for (const auto& caseNode : node.cases) {
        std::string type = mapType(caseNode->type.get());
        writeHeaderLine(type + " " + caseNode->name + "_;");
    }

    outdent();
    outdent();
    writeHeaderLine("};");
    writeHeaderLine();
}

// ============================================================================
// Utility
// ============================================================================

std::string Cpp11Generator::sanitizeIdentifier(const std::string& name) const {
    // C++ reserved words that might conflict
    static const std::unordered_map<std::string, std::string> reserved = {
        {"class", "class_"},
        {"struct", "struct_"},
        {"union", "union_"},
        {"enum", "enum_"},
        {"template", "template_"},
        {"typename", "typename_"},
        {"virtual", "virtual_"},
        {"public", "public_"},
        {"private", "private_"},
        {"protected", "protected_"},
        {"friend", "friend_"},
        {"namespace", "namespace_"},
        {"using", "using_"},
        {"try", "try_"},
        {"catch", "catch_"},
        {"throw", "throw_"},
        {"new", "new_"},
        {"delete", "delete_"},
        {"this", "this_"},
        {"operator", "operator_"},
        {"sizeof", "sizeof_"},
        {"alignof", "alignof_"},
        {"decltype", "decltype_"},
        {"nullptr", "nullptr_"},
        {"constexpr", "constexpr_"},
        {"static_cast", "static_cast_"},
        {"dynamic_cast", "dynamic_cast_"},
        {"const_cast", "const_cast_"},
        {"reinterpret_cast", "reinterpret_cast_"}
    };

    auto it = reserved.find(name);
    if (it != reserved.end()) {
        return it->second;
    }
    return name;
}

std::string Cpp11Generator::constValueToString(const ConstValue& value) const {
    if (auto* i = std::get_if<int64_t>(&value)) {
        return std::to_string(*i);
    }
    if (auto* u = std::get_if<uint64_t>(&value)) {
        return std::to_string(*u) + "ULL";
    }
    if (auto* d = std::get_if<double>(&value)) {
        std::ostringstream oss;
        oss << std::setprecision(17) << *d;
        return oss.str();
    }
    if (auto* s = std::get_if<std::string>(&value)) {
        return "\"" + *s + "\"";
    }
    if (auto* b = std::get_if<bool>(&value)) {
        return *b ? "true" : "false";
    }
    return "0";
}

std::string Cpp11Generator::makeIncludeGuard(const std::string& filename) const {
    std::string guard = "IBORB_GENERATED_";
    
    if (!config_.namespacePrefix.empty()) {
        std::string prefix = config_.namespacePrefix;
        std::transform(prefix.begin(), prefix.end(), prefix.begin(),
                       [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        guard += prefix + "_";
    }

    for (char c : filename) {
        if (std::isalnum(c)) {
            guard += static_cast<char>(std::toupper(c));
        } else {
            guard += '_';
        }
    }
    
    guard += "_HPP";
    return guard;
}

void Cpp11Generator::addError(const std::string& message) {
    errors_.push_back(message);
}

} // namespace iborb::generator
