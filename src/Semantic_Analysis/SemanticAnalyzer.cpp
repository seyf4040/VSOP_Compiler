#include "SemanticAnalyzer.hpp"
#include <iostream>
#include <algorithm>
#include <sstream>
#include <set> // Include set for hierarchy checks

namespace VSOP {

// Type implementation
Type::Type(const std::string& name, Kind kind)
    : name(name), kind(kind) {}

// Modified conformsTo to use ClassDef map
bool Type::conformsTo(const Type& other, const std::unordered_map<std::string, ClassDef>& class_defs) const {
    if (name == other.name) return true;
    if (isError() || other.isError()) return true;
    if (kind == Kind::PRIMITIVE || other.kind == Kind::PRIMITIVE) {
        if (other.name == "Object") return true; // Primitives conform to Object
        return false; // Otherwise, primitives only conform to themselves
    }
    if (other.name == "Object") return true; // All classes conform to Object

    std::string current = name;
    while (current != "Object" && !current.empty()) {
        auto it = class_defs.find(current);
        if (it == class_defs.end()) return false; // Should not happen for valid types
        current = it->second.parent;
        if (current == other.name) return true;
    }
    return false;
}

const std::unordered_map<std::string, ClassDef>& SemanticAnalyzer::getClassDefinitions() const {
    return class_definitions;
}

// MethodSignature implementation
bool MethodSignature::isCompatible(const MethodSignature& other) const {
    // Check return type and parameter count
    // For VSOP, require exact match for return type
    if (returnType.toString() != other.returnType.toString() ||
        parameters.size() != other.parameters.size()) {
        return false;
    }

    // Check parameter types (must match exactly - no contravariance in VSOP)
    for (size_t i = 0; i < parameters.size(); ++i) {
        if (parameters[i].type.toString() != other.parameters[i].type.toString()) {
            return false;
        }
    }

    return true;
}

// ClassDef implementation - modified to use ClassDef map
bool ClassDef::hasCyclicInheritance(const std::unordered_map<std::string, ClassDef>& class_definitions) const {
    std::set<std::string> visited;
    std::string current = name;

    while (current != "Object" && !current.empty()) {
        if (visited.count(current)) {
            return true; // Cycle detected
        }
        visited.insert(current);

        auto it = class_definitions.find(current);
        if (it == class_definitions.end()) {
            return false; // Broken chain, not necessarily a cycle (error reported elsewhere)
        }
        current = it->second.parent;

        // Check if parent is the original class name
        if (current == name) return true;
    }
    return false; // Reached Object or end without cycle
}

// Scope implementation remains the same
std::pair<bool, Type> Scope::lookupVariable(const std::string& name) const {
    auto it = variables.find(name);
    if (it != variables.end()) {
        return {true, it->second};
    }
    if (parent) {
        return parent->lookupVariable(name);
    }
    return {false, Type::Error()};
}

void Scope::addVariable(const std::string& name, const Type& type) {
    variables[name] = type;
}

// SemanticAnalyzer implementation
SemanticAnalyzer::SemanticAnalyzer() {
    current_scope = std::make_shared<Scope>(); // Global scope might be needed later
    initObjectMethods();
}

void SemanticAnalyzer::initObjectMethods() {
    ClassDef object_def("Object", ""); // Object has no parent
    std::vector<FormalParam> print_params = {FormalParam("s", Type::String())};
    object_def.methods["print"] = MethodSignature("print", print_params, Type::Object());
    std::vector<FormalParam> print_int_params = {FormalParam("i", Type::Int32())};
    object_def.methods["printInt32"] = MethodSignature("printInt32", print_int_params, Type::Object());
    std::vector<FormalParam> input_int_params;
    object_def.methods["inputInt32"] = MethodSignature("inputInt32", input_int_params, Type::Int32());
    std::vector<FormalParam> input_string_params;
    object_def.methods["inputString"] = MethodSignature("inputString", input_string_params, Type::String());
    
    // Add additional built-in methods for I/O operations
    std::vector<FormalParam> printBool_params = {FormalParam("b", Type::Boolean())};
    object_def.methods["printBool"] = MethodSignature("printBool", printBool_params, Type::Object());
    std::vector<FormalParam> inputBool_params;
    object_def.methods["inputBool"] = MethodSignature("inputBool", inputBool_params, Type::Boolean());
    std::vector<FormalParam> inputLine_params;
    object_def.methods["inputLine"] = MethodSignature("inputLine", inputLine_params, Type::String());
    
    class_definitions["Object"] = object_def;
}

bool SemanticAnalyzer::analyze(std::shared_ptr<Program> prog) {
    program = prog;
    errors.clear();
    class_table.clear();
    // Clear class_definitions but keep Object
    class_definitions.erase(class_definitions.begin(), class_definitions.end());
    initObjectMethods(); // Re-initialize Object definition

    current_scope = std::make_shared<Scope>(); // Reset global scope

    if (!program) {
        reportError("No program to analyze");
        return false;
    }

    buildClassDefinitions();
    if (!errors.empty()) return false; // Stop if class structure is wrong

    validateInheritanceHierarchy();
    if (!errors.empty()) return false; // Stop if hierarchy is wrong

    collectMethodsAndFields();
    if (!errors.empty()) return false; // Stop if members have issues

    // Check for Main class and main method
    auto main_class_it = class_definitions.find("Main");
    if (main_class_it == class_definitions.end()) {
        reportError("Program must have a Main class");
    } else {
        auto main_method_it = main_class_it->second.methods.find("main");
        if (main_method_it == main_class_it->second.methods.end()) {
            reportError("Main class must have a main method");
        } else {
            const auto& main_sig = main_method_it->second;
            if (!main_sig.parameters.empty()) {
                reportError("Main.main method must not have parameters");
            }
            if (main_sig.returnType.toString() != "int32") {
                reportError("Main.main method must have return type int32");
            }
        }
    }

    return errors.empty();
}

void SemanticAnalyzer::buildClassDefinitions() {
    std::unordered_set<std::string> defined_classes;
    defined_classes.insert("Object"); // Object is implicitly defined

    for (const auto& cls : program->classes) {
        if (!cls) continue; // Should not happen if parser works

        // Check for primitive type redefinition
         if (cls->name == "int32" || cls->name == "bool" || cls->name == "string" || cls->name == "unit") {
            reportError("Cannot redefine primitive type: " + cls->name);
            continue;
        }
        // Check for Object redefinition
        if (cls->name == "Object") {
            reportError("Class Object cannot be redefined");
             continue;
        }

        // Check for class redefinition
        if (defined_classes.count(cls->name)) {
            reportError("Redefinition of class " + cls->name);
            continue;
        }

        // Add to class table (AST nodes) and create definition
        class_table[cls->name] = cls;
        class_definitions[cls->name] = ClassDef(cls->name, cls->parent.empty() ? "Object" : cls->parent);
        defined_classes.insert(cls->name);
    }
}

void SemanticAnalyzer::validateInheritanceHierarchy() {
    for (const auto& [name, class_def] : class_definitions) {
        if (name == "Object") continue;

        const std::string& parent_name = class_def.parent;

        // Check parent exists and is not a primitive type
        if (parent_name == "int32" || parent_name == "bool" || parent_name == "string" || parent_name == "unit") {
             reportError("Class " + name + " cannot extend primitive type " + parent_name);
             continue;
        }
        if (class_definitions.find(parent_name) == class_definitions.end()) {
             reportError("Class " + name + " extends undefined class " + parent_name);
             continue;
        }

        // Check for inheritance cycles using the map
        if (class_def.hasCyclicInheritance(class_definitions)) {
            reportError("Class " + name + " has cyclic inheritance");
        }
    }
}

void SemanticAnalyzer::collectMethodsAndFields() {
     // Use sets to track defined names in the hierarchy to check overriding/shadowing
    std::unordered_map<std::string, std::unordered_set<std::string>> class_fields;
    std::unordered_map<std::string, std::unordered_map<std::string, MethodSignature>> class_methods;

    // Iterate through the AST classes
    for (const auto& [name, cls_node] : class_table) {
        if (!cls_node) continue;
        auto& class_def = class_definitions[name]; // Get the definition being built

        // Check fields
        std::unordered_set<std::string> local_field_names;
        for (const auto& field_node : cls_node->fields) {
            if (!field_node) continue;

            // Check field redefinition within this class
            if (local_field_names.count(field_node->name)) {
                reportError("Field " + field_node->name + " is already defined in class " + name);
                continue;
            }
             local_field_names.insert(field_node->name);

            // Check if field shadows parent field
             if (findFieldType(class_def.parent, field_node->name).has_value()) {
                  reportError("Field " + field_node->name + " in class " + name +
                              " cannot shadow a field from an ancestor class.");
                 continue;
             }

            // Check field type is valid
            Type field_type = resolveType(field_node->type);
            if (field_type.isError()) {
                reportError("Unknown type " + field_node->type + " for field " + field_node->name + " in class " + name);
                continue;
            }

            // Add field to class definition
            class_def.fields[field_node->name] = field_type;
        }

         // Check methods
         std::unordered_set<std::string> local_method_names;
         for (const auto& method_node : cls_node->methods) {
            if (!method_node) continue;

             // Check method redefinition within this class
             if (local_method_names.count(method_node->name)) {
                 reportError("Method " + method_node->name + " is already defined in class " + name);
                 continue;
             }
              local_method_names.insert(method_node->name);

             // Check parameters and return type
             std::vector<FormalParam> formal_params;
             std::unordered_set<std::string> param_names;
             bool param_type_error = false;

             for (const auto& formal_node : method_node->formals) {
                 if (!formal_node) continue;
                 if (param_names.count(formal_node->name)) {
                     reportError("Duplicate parameter name " + formal_node->name + " in method " + method_node->name);
                     param_type_error = true; // Mark error for signature creation
                 }
                 param_names.insert(formal_node->name);

                  if (formal_node->name == "self") {
                     reportError("Parameter name cannot be 'self' in method " + method_node->name);
                     param_type_error = true;
                 }

                 Type param_type = resolveType(formal_node->type);
                 if (param_type.isError()) {
                     reportError("Unknown type " + formal_node->type + " for parameter " + formal_node->name + " in method " + method_node->name);
                     param_type_error = true;
                 }
                 
                 // Remove restriction on unit parameters - VSOP allows unit parameters
                 // if (param_type.toString() == "unit") {
                 //     reportError("Parameter " + formal_node->name + " in method " + method_node->name + " cannot have type unit");
                 //     param_type_error = true;
                 // }
                 
                 formal_params.push_back(FormalParam(formal_node->name, param_type));
             }

             Type return_type = resolveType(method_node->return_type);
             if (return_type.isError()) {
                 reportError("Unknown return type " + method_node->return_type + " for method " + method_node->name);
                 param_type_error = true; // Treat as signature error
             }

             // Only proceed if parameter and return types were valid
             if (param_type_error) continue;

             // Create method signature
             MethodSignature current_sig(method_node->name, formal_params, return_type);

             // Check method overriding: find method in parent hierarchy
             std::optional<MethodSignature> parent_sig_opt = findMethodSignature(class_def.parent, method_node->name);

             if (parent_sig_opt.has_value()) {
                 // Method overrides parent method. Check signature compatibility.
                 if (!current_sig.isCompatible(parent_sig_opt.value())) {
                     reportError("Method " + method_node->name + " in class " + name +
                                 " overrides parent method with incompatible signature.");
                     continue; // Don't add incompatible override
                 }
             }

             // Add method to class definition
             class_def.methods[method_node->name] = current_sig;
         }
    }
}

// --- Public method implementations ---

bool SemanticAnalyzer::isTypeValid(const std::string& typeName) {
    if (typeName == "int32" || typeName == "bool" || typeName == "string" || typeName == "unit") {
        return true;
    }
    // Check if it's a defined class (including Object)
    return class_definitions.count(typeName);
}

Type SemanticAnalyzer::resolveType(const std::string& typeName) {
    if (typeName == "int32") return Type::Int32();
    if (typeName == "bool") return Type::Boolean();
    if (typeName == "string") return Type::String();
    if (typeName == "unit") return Type::Unit();
    if (class_definitions.count(typeName)) return Type(typeName, Type::Kind::CLASS);
    // Special case for potentially unresolved 'Object' during early stages?
    if (typeName == "Object") return Type::Object();

    return Type::Error(); // Unknown type
}

std::optional<std::string> SemanticAnalyzer::getParentClassName(const std::string& className) {
    if (className == "Object") return std::nullopt; // Object has no parent
    auto it = class_definitions.find(className);
    if (it != class_definitions.end()) {
        // Return parent, which could be empty string if it should be Object but wasn't set?
        // Assume parent is correctly set during buildClassDefinitions.
        if (it->second.parent.empty()) return std::nullopt; // Should only happen for Object
        return it->second.parent;
    }
    return std::nullopt; // Class not found
}

std::optional<Type> SemanticAnalyzer::findFieldType(const std::string& className, const std::string& fieldName) {
    std::string current_class = className;
    while (!current_class.empty()) {
        auto class_it = class_definitions.find(current_class);
        if (class_it == class_definitions.end()) {
            return std::nullopt; // Class not found in hierarchy
        }
        auto field_it = class_it->second.fields.find(fieldName);
        if (field_it != class_it->second.fields.end()) {
            return field_it->second; // Found field
        }
        if (current_class == "Object") break; // Stop at Object
        current_class = class_it->second.parent;
    }
    return std::nullopt; // Field not found in hierarchy
}

std::optional<MethodSignature> SemanticAnalyzer::findMethodSignature(const std::string& className, const std::string& methodName) {
    std::string current_class = className;
    while (!current_class.empty()) {
        auto class_it = class_definitions.find(current_class);
        if (class_it == class_definitions.end()) {
            return std::nullopt; // Class not found in hierarchy
        }
        auto method_it = class_it->second.methods.find(methodName);
        if (method_it != class_it->second.methods.end()) {
            return method_it->second; // Found method
        }
         if (current_class == "Object") break; // Stop at Object
        current_class = class_it->second.parent;
    }
    return std::nullopt; // Method not found in hierarchy
}


Type SemanticAnalyzer::findCommonAncestor(const Type& type1, const Type& type2) {
    if (type1.isError() || type2.isError()) return Type::Error();
    if (!isTypeValid(type1.getName()) || !isTypeValid(type2.getName())) return Type::Error();
    if (type1.getName() == type2.getName()) return type1;

    // Use conformsTo using the class_definitions map
    if (type1.conformsTo(type2, class_definitions)) return type2;
    if (type2.conformsTo(type1, class_definitions)) return type1;

    // Handle primitives (only common ancestor is Object)
    if (type1.getKind() == Type::Kind::PRIMITIVE || type2.getKind() == Type::Kind::PRIMITIVE) {
        return Type::Object();
    }

    // Both are class types, find common ancestor
    std::set<std::string> ancestors1;
    std::string current = type1.getName();
    while (!current.empty()) {
        ancestors1.insert(current);
        if (current == "Object") break;
        auto it = class_definitions.find(current);
        if (it == class_definitions.end()) return Type::Error(); // Should not happen
        current = it->second.parent;
    }

    current = type2.getName();
    while (!current.empty()) {
        if (ancestors1.count(current)) {
            return Type(current, Type::Kind::CLASS); // Found common ancestor
        }
        if (current == "Object") break;
        auto it = class_definitions.find(current);
         if (it == class_definitions.end()) return Type::Error(); // Should not happen
        current = it->second.parent;
    }

    return Type::Object(); // Should technically always find Object
}


void SemanticAnalyzer::reportError(const std::string& message) {
    // Add location info if available from AST nodes eventually
    errors.push_back(message);
}

} // namespace VSOP