#ifndef SEMANTIC_ANALYZER_HPP
#define SEMANTIC_ANALYZER_HPP

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <memory>
#include <set>
#include <optional> // For optional return values
#include "AST.hpp"

namespace VSOP {

// Forward declarations
class TypedExpression;
struct ClassDef; // <<< Forward declaration added here

// Class to represent a type in VSOP
class Type {
public:
    enum class Kind {
        PRIMITIVE,
        CLASS
    };

    Type() : name("__error__"), kind(Kind::PRIMITIVE) {}  // Default constructor
    Type(const std::string& name, Kind kind = Kind::CLASS);

    const std::string& getName() const { return name; }
    Kind getKind() const { return kind; }

    // Check if this type conforms to the given type (i.e., is a subtype of it)
    // Needs the full definition of ClassDef, but that's available in the .cpp file
    // where this method is implemented. The forward declaration is enough here.
    bool conformsTo(const Type& other, const std::unordered_map<std::string, ClassDef>& class_defs) const;

    // Print the type name
    std::string toString() const { return name; }

    // Create primitive types
    static Type Int32() { return Type("int32", Kind::PRIMITIVE); }
    static Type Boolean() { return Type("bool", Kind::PRIMITIVE); }
    static Type String() { return Type("string", Kind::PRIMITIVE); }
    static Type Unit() { return Type("unit", Kind::PRIMITIVE); }
    static Type Object() { return Type("Object", Kind::CLASS); }

    // For error recovery
    static Type Error() { return Type("__error__", Kind::PRIMITIVE); }
    bool isError() const { return name == "__error__"; }

// Make name and kind private if not needed publicly elsewhere
// private:
    std::string name;
    Kind kind;
};

// Represents a formal parameter with name and type
struct FormalParam {
    std::string name;
    Type type;

    FormalParam() : name(""), type(Type::Error()) {}  // Default constructor
    FormalParam(const std::string& name, const Type& type)
        : name(name), type(type) {}
};

// Represents a method signature
struct MethodSignature {
    std::string name;
    std::vector<FormalParam> parameters;
    Type returnType;

    MethodSignature() : name(""), returnType(Type::Error()) {}  // Default constructor
    MethodSignature(const std::string& name, const std::vector<FormalParam>& parameters, const Type& returnType)
        : name(name), parameters(parameters), returnType(returnType) {}

    // Check if this signature is compatible with another (for method overriding)
    bool isCompatible(const MethodSignature& other) const;
};

// Represents a class definition with its fields and methods
// Definition restored to its original position
struct ClassDef {
    std::string name;
    std::string parent;
    // Type and MethodSignature are now defined before this struct
    std::unordered_map<std::string, Type> fields;
    std::unordered_map<std::string, MethodSignature> methods;

    ClassDef() : name(""), parent("") {}  // Default constructor
    ClassDef(const std::string& name, const std::string& parent)
        : name(name), parent(parent) {}

    // Check for cyclic inheritance (needs access to the map)
    bool hasCyclicInheritance(const std::unordered_map<std::string, ClassDef>& class_definitions) const;
};

// Represents a scope for variable lookup
struct Scope {
    std::unordered_map<std::string, Type> variables;
    std::shared_ptr<Scope> parent;

    Scope() : parent(nullptr) {}
    Scope(std::shared_ptr<Scope> parent) : parent(parent) {}

    // Try to find a variable in the scope chain
    std::pair<bool, Type> lookupVariable(const std::string& name) const;

    // Add a variable to the current scope
    void addVariable(const std::string& name, const Type& type);
};

// The main semantic analyzer class
class SemanticAnalyzer {
public:
    SemanticAnalyzer();

    // Analyze and type-check a program
    bool analyze(std::shared_ptr<Program> program);

    
    const std::unordered_map<std::string, ClassDef>& getClassDefinitions() const;

    // Get semantic error messages
    const std::vector<std::string>& getErrors() const { return errors; }

    // --- Public methods for TypeChecker ---
    bool isTypeValid(const std::string& typeName); // Made public
    Type resolveType(const std::string& typeName);  // Made public
    std::pair<bool, std::string> getParentClassName(const std::string& className);
    std::pair<bool, Type> findFieldType(const std::string& className, const std::string& fieldName);
    std::pair<bool, MethodSignature> findMethodSignature(const std::string& className, const std::string& methodName);
    Type findCommonAncestor(const Type& type1, const Type& type2); // Made public


private:
    // Perform the different passes of the semantic analysis
    void buildClassDefinitions();
    void validateInheritanceHierarchy();
    void collectMethodsAndFields();
    void typeCheckProgram(); // This might become obsolete if TypeChecker does the visiting

    // Type-checking methods for different nodes (These might belong in TypeChecker now)
    /* ... (removed for brevity) ... */

    // Utility methods
    void reportError(const std::string& message);
    // Type findMethodReturnType(const std::string& className, const std::string& methodName,
    //                           const std::vector<Type>& argTypes); // Replaced by findMethodSignature


    // State
    std::shared_ptr<Program> program;
    std::vector<std::string> errors;
    std::unordered_map<std::string, std::shared_ptr<Class>> class_table; // From AST nodes
    // ClassDef is now fully defined before this usage
    std::unordered_map<std::string, ClassDef> class_definitions; // Built definitions
    std::shared_ptr<Scope> current_scope; // Analyzer might still manage global scope?
    std::string current_class_name; // Analyzer might still manage global scope?

    // Location information for errors
    std::string source_file;

    // Predefined Object methods
    void initObjectMethods();
};

} // namespace VSOP

#endif // SEMANTIC_ANALYZER_HPP