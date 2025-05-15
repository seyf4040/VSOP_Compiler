#include "TypeChecker.hpp"
#include <iostream>
#include <algorithm>
#include <list>
#include <stack>
#include <sstream>
#include <optional> // Include optional

namespace VSOP {

// Store scopes for symbol lookup
static std::list<std::unordered_map<std::string, std::string>> scopes;

// Constructor
TypeChecker::TypeChecker(const std::string& source_file)
    : source_file(source_file) {
    // Initialize with global scope
    enterScope();
}

// Main entry point
bool TypeChecker::check(std::shared_ptr<Program> prog) {
    program = prog;

    // First pass: analyze semantics with the analyzer
    if (!analyzer.analyze(program)) {
        const auto& analyzer_errors = analyzer.getErrors();
        errors.insert(errors.end(), analyzer_errors.begin(), analyzer_errors.end());
        return false;
    }
    // Analyzer now holds the valid class definitions and hierarchy info.

    // Second pass: visit AST and perform type checking using analyzer info
    for (const auto& cls : program->classes) {
        if (cls) cls->accept(this); // Check for null just in case
    }

    return errors.empty();
}

// Get error messages
const std::vector<std::string>& TypeChecker::getErrors() const {
    return errors;
}

// Scope Management
void TypeChecker::enterScope() {
    scopes.push_front(std::unordered_map<std::string, std::string>());
}
void TypeChecker::exitScope() {
    if (!scopes.empty()) scopes.pop_front();
}
void TypeChecker::addSymbol(const std::string& name, const std::string& type) {
    if (!scopes.empty()) scopes.front()[name] = type;
}

std::string TypeChecker::lookupSymbol(const std::string& name) {
    // Look in lexical scopes first
    for (const auto& scope : scopes) {
        auto it = scope.find(name);
        if (it != scope.end()) return it->second;
    }

    // If not in lexical scopes, check if it's a field of the current class or ancestors
    if (!current_class.empty()) {
        std::optional<Type> field_type_opt = analyzer.findFieldType(current_class, name);
        if (field_type_opt.has_value()) {
            return field_type_opt.value().toString();
        }
    }

    return "__error__"; // Not found
}

// Expression Type Management
void TypeChecker::setExprType(const Expression* expr, const std::string& type) {
    if (expr) expr_types[expr] = type;
}
std::string TypeChecker::getExprType(const Expression* expr) {
    if (!expr) return "__error__";
    auto it = expr_types.find(expr);
    return (it != expr_types.end()) ? it->second : "__error__";
}

// Type Validation and Subtyping (using Analyzer)
bool TypeChecker::isValidType(const std::string& type) {
    return analyzer.isTypeValid(type); // Use analyzer's public method
}

bool TypeChecker::isSubtypeOf(const std::string& subtype_name, const std::string& supertype_name) {
    if (subtype_name == supertype_name) return true;
    if (subtype_name == "__error__" || supertype_name == "__error__") return true;

    Type subtype = analyzer.resolveType(subtype_name);
    Type supertype = analyzer.resolveType(supertype_name);

    if (subtype.isError() || supertype.isError()) return false; // Invalid types involved

    // Use the Type::conformsTo method, passing the analyzer's class_definitions
    return subtype.conformsTo(supertype, analyzer.getClassDefinitions());
}

std::string TypeChecker::getCommonAncestor(const std::string& type1_name, const std::string& type2_name) {
    Type type1 = analyzer.resolveType(type1_name);
    Type type2 = analyzer.resolveType(type2_name);

    if (type1.isError() || type2.isError()) return "__error__";

    // Use analyzer's public findCommonAncestor method
    return analyzer.findCommonAncestor(type1, type2).toString();
}

// Error Reporting
void TypeChecker::reportError(const std::string& message, int line, int col) {
    std::string error = source_file + ":" + std::to_string(line) + ":" +
                        std::to_string(col) + ": semantic error: " + message;
    if (std::find(errors.begin(), errors.end(), error) == errors.end()) {
         errors.push_back(error); // Avoid duplicate errors from same node
    }
}

// ---- Visitor Implementations (using analyzer methods) ----

void TypeChecker::visit(const Class* node) {
    current_class = node->name;
    enterScope();
    addSymbol("self", node->name);

    // Fields are conceptually members, not lexical variables in the same way.
    // Don't add them to scope here. lookupSymbol will check fields via analyzer.

    for (const auto& field : node->fields) if (field) field->accept(this);
    for (const auto& method : node->methods) if (method) method->accept(this);

    exitScope();
    current_class = "";
}

void TypeChecker::visit(const Field* node) {
    if (!isValidType(node->type)) {
        reportError("Unknown type '" + node->type + "' for field '" + node->name + "'");
    }

    if (node->init_expr) {
        node->init_expr->accept(this);
        std::string init_type = getExprType(node->init_expr.get());
        if (isValidType(node->type) && init_type != "__error__") {
            if (!isSubtypeOf(init_type, node->type)) {
                reportError("Field '" + node->name + "' initialized with incompatible type: expected " +
                            node->type + ", got " + init_type);
            }
        }
    }
}

void TypeChecker::visit(const Method* node) {
    current_method = node->name;
    enterScope();
    addSymbol("self", current_class);

    if (!isValidType(node->return_type)) {
        reportError("Unknown return type '" + node->return_type + "' for method '" + node->name + "'");
    }

    std::unordered_map<std::string, bool> formal_names;
    for (const auto& formal : node->formals) {
        if (formal) {
            if (formal_names[formal->name]) reportError("Duplicate parameter name '" + formal->name + "'");
            formal_names[formal->name] = true;
            formal->accept(this);
        }
    }

    if (node->body) {
        node->body->accept(this);
        std::string body_type = getExprType(node->body.get());
        if (body_type != "__error__" && isValidType(node->return_type)) {
             if (!isSubtypeOf(body_type, node->return_type)) {
                 reportError("Method '" + node->name + "' body final type " + body_type +
                             " is not a subtype of return type " + node->return_type);
             }
        }
    } else if (isValidType(node->return_type) && node->return_type != "unit") {
         reportError("Method '" + node->name + "' has non-unit return type '" + node->return_type + "' but no body");
    }

    exitScope();
    current_method = "";
}

void TypeChecker::visit(const Formal* node) {
    if (!isValidType(node->type)) {
        reportError("Unknown type '" + node->type + "' for parameter '" + node->name + "'");
        return;
    }
    addSymbol(node->name, node->type);
}

// BinaryOp, UnaryOp, If, While, Literals, Self, Block visitors remain largely the same,
// as they rely on getExprType, setExprType, isSubtypeOf, getCommonAncestor, which now use the analyzer correctly.
// We just need to ensure they handle __error__ propagation correctly.

void TypeChecker::visit(const BinaryOp* node) {
    node->left->accept(this);
    node->right->accept(this);
    std::string left_type = getExprType(node->left.get());
    std::string right_type = getExprType(node->right.get());
    std::string result_type = "__error__";

    if (left_type == "__error__" || right_type == "__error__") {
        setExprType(node, "__error__"); return;
    }

    if (node->op == "and") {
        if (left_type != "bool") reportError("Left operand of 'and' must be bool, got " + left_type);
        if (right_type != "bool") reportError("Right operand of 'and' must be bool, got " + right_type);
        if (left_type == "bool" && right_type == "bool") result_type = "bool";
    } else if (node->op == "=") {
         bool left_is_prim = (left_type == "int32" || left_type == "bool" || left_type == "string"); // unit handled below
         bool right_is_prim = (right_type == "int32" || right_type == "bool" || right_type == "string");
         bool comparable = false;
         if (left_type == "unit" || right_type == "unit") {
             if (left_type == "unit" && right_type == "unit") comparable = true;
             else { comparable = false; reportError("Type unit can only be compared for equality with unit"); }
         } else if (left_type == right_type) comparable = true;
         else if (left_is_prim != right_is_prim) { comparable = false; reportError("Cannot compare primitive type " + (left_is_prim ? left_type : right_type) + " with class type " + (left_is_prim ? right_type : left_type)); }
         else if (left_is_prim && right_is_prim) { comparable = false; reportError("Cannot compare distinct primitive types " + left_type + " and " + right_type); }
         else comparable = true; // Both are non-unit, non-primitive class types

         result_type = "bool"; // Comparison always yields bool, errors reported above
    } else if (node->op == "<" || node->op == "<=") {
        if (left_type != "int32") reportError("Left operand of '" + node->op + "' must be int32, got " + left_type);
        if (right_type != "int32") reportError("Right operand of '" + node->op + "' must be int32, got " + right_type);
        if (left_type == "int32" && right_type == "int32") result_type = "bool";
    } else if (node->op == "+" || node->op == "-" || node->op == "*" || node->op == "/" || node->op == "^") {
        if (left_type != "int32") reportError("Left operand of '" + node->op + "' must be int32, got " + left_type);
        if (right_type != "int32") reportError("Right operand of '" + node->op + "' must be int32, got " + right_type);
        if (left_type == "int32" && right_type == "int32") result_type = "int32";
    } else {
        reportError("Unknown binary operator: " + node->op);
    }
    setExprType(node, result_type);
}

void TypeChecker::visit(const UnaryOp* node) {
    node->expr->accept(this);
    std::string operand_type = getExprType(node->expr.get());
    std::string result_type = "__error__";

    if (operand_type == "__error__") { setExprType(node, "__error__"); return; }

    if (node->op == "not") {
        if (operand_type != "bool") reportError("Operand of 'not' must be bool, got " + operand_type);
        else result_type = "bool";
    } else if (node->op == "-") {
        if (operand_type != "int32") reportError("Operand of unary '-' must be int32, got " + operand_type);
        else result_type = "int32";
    } else if (node->op == "isnull") {
        bool is_prim = (operand_type == "int32" || operand_type == "bool" || operand_type == "string" || operand_type == "unit");
        if (is_prim) reportError("Operand of 'isnull' cannot be primitive type " + operand_type);
        else result_type = "bool";
    } else {
        reportError("Unknown unary operator: " + node->op);
    }
    setExprType(node, result_type);
}

void TypeChecker::visit(const Call* node) {
    std::string object_type;
    if (node->object) {
        node->object->accept(this);
        object_type = getExprType(node->object.get());
    } else {
        object_type = lookupSymbol("self"); // Use lookupSymbol for consistency
        if (object_type == "__error__") {
             reportError("Call to method '" + node->method_name + "' on 'self' outside class context");
             setExprType(node, "__error__"); return;
        }
    }

    if (object_type == "__error__") { setExprType(node, "__error__"); return; }

    bool is_prim = (object_type == "int32" || object_type == "bool" || object_type == "string" || object_type == "unit");
    if (is_prim) {
         reportError("Cannot call method '" + node->method_name + "' on primitive type " + object_type);
         setExprType(node, "__error__"); return;
    }

    std::vector<std::string> arg_types;
    bool arg_error = false;
    for (const auto& arg : node->arguments) {
        if (!arg) { arg_error = true; continue; } // Skip null args, mark error
        arg->accept(this);
        std::string arg_type = getExprType(arg.get());
        if (arg_type == "__error__") arg_error = true;
        arg_types.push_back(arg_type);
    }

    if (arg_error) { setExprType(node, "__error__"); return; }

    // Use the helper that now uses the analyzer's findMethodSignature
    std::string return_type = getMethodReturnType(object_type, node->method_name, arg_types);
    setExprType(node, return_type);
}

void TypeChecker::visit(const New* node) {
    if (!isValidType(node->type_name)) {
        reportError("Unknown type '" + node->type_name + "' in 'new' expression");
        setExprType(node, "__error__"); return;
    }
    if (node->type_name == "int32" || node->type_name == "bool" || node->type_name == "string" || node->type_name == "unit") {
        reportError("Cannot instantiate primitive or unit type with 'new': " + node->type_name);
        setExprType(node, "__error__"); return;
    }
    // Type is valid and not primitive/unit, so it's a class type
    setExprType(node, node->type_name);
}

void TypeChecker::visit(const Let* node) {
    std::string declared_type = node->type;
    if (!isValidType(declared_type)) {
        reportError("Unknown type '" + declared_type + "' in 'let' for variable '" + node->name + "'");
        declared_type = "__error__";
    }

    if (node->init_expr) {
        node->init_expr->accept(this);
        std::string init_type = getExprType(node->init_expr.get());
        if (declared_type != "__error__" && init_type != "__error__") {
            if (!isSubtypeOf(init_type, declared_type)) {
                reportError("Variable '" + node->name + "' initialized with incompatible type: expected " +
                            declared_type + ", got " + init_type);
            }
        }
    }

    enterScope();
    addSymbol(node->name, declared_type); // Add even if __error__ type
    if (node->scope_expr) node->scope_expr->accept(this);
    std::string scope_type = getExprType(node->scope_expr.get());
    exitScope();

    setExprType(node, (declared_type == "__error__") ? "__error__" : scope_type);
}

void TypeChecker::visit(const If* node) {
    node->condition->accept(this);
    std::string condition_type = getExprType(node->condition.get());
    if (condition_type != "bool" && condition_type != "__error__") {
        reportError("If condition must be bool, got " + condition_type);
    }

    node->then_expr->accept(this);
    std::string then_type = getExprType(node->then_expr.get());

    std::string else_type = "unit";
    if (node->else_expr) {
        node->else_expr->accept(this);
        else_type = getExprType(node->else_expr.get());
    }

    if (condition_type == "__error__" || then_type == "__error__" || (node->else_expr && else_type == "__error__")) {
         setExprType(node, "__error__"); return;
    }

    if (!node->else_expr) {
        setExprType(node, "unit");
    } else {
        setExprType(node, getCommonAncestor(then_type, else_type));
    }
}

void TypeChecker::visit(const While* node) {
    node->condition->accept(this);
    std::string condition_type = getExprType(node->condition.get());
    if (condition_type != "bool" && condition_type != "__error__") {
        reportError("While condition must be bool, got " + condition_type);
    }

    if (node->body) node->body->accept(this);
    std::string body_type = getExprType(node->body.get());

    setExprType(node, (condition_type == "__error__" || body_type == "__error__") ? "__error__" : "unit");
}

void TypeChecker::visit(const Assign* node) {
    std::string var_type = lookupSymbol(node->name);
    if (var_type == "__error__") {
        reportError("Assignment to undefined variable: " + node->name);
        setExprType(node, "__error__"); return;
    }
     if (node->name == "self") {
         reportError("Cannot assign to 'self'");
         setExprType(node, "__error__"); return;
     }

    node->expr->accept(this);
    std::string expr_type = getExprType(node->expr.get());
    if (expr_type == "__error__") {
         setExprType(node, "__error__"); return;
    }

    if (!isSubtypeOf(expr_type, var_type)) {
        reportError("Cannot assign value of type " + expr_type +
                    " to variable '" + node->name + "' of type " + var_type);
        // Assignment result type is still expr_type, error is reported
    }
    setExprType(node, expr_type);
}

void TypeChecker::visit(const StringLiteral* node) { setExprType(node, "string"); }
void TypeChecker::visit(const IntegerLiteral* node) { setExprType(node, "int32"); }
void TypeChecker::visit(const BooleanLiteral* node) { setExprType(node, "bool"); }
void TypeChecker::visit(const UnitLiteral* node) { setExprType(node, "unit"); }

void TypeChecker::visit(const Identifier* node) {
    std::string type = lookupSymbol(node->name);
    if (type == "__error__") {
        reportError("Undefined identifier: " + node->name);
    }
    setExprType(node, type);
}

void TypeChecker::visit(const Self* node) {
    std::string self_type = lookupSymbol("self");
     if (self_type == "__error__") {
         reportError("Use of 'self' outside of a class method context");
     }
    setExprType(node, self_type);
}

void TypeChecker::visit(const Block* node) {
    std::string last_expr_type = "unit";
    bool error_in_block = false;
    if (!node || node->expressions.empty()) {
         setExprType(node, "unit"); return;
    }

    for (size_t i = 0; i < node->expressions.size(); ++i) {
        const auto& expr = node->expressions[i];
        if (!expr) { error_in_block = true; continue; } // Skip null expressions

        expr->accept(this);
        std::string current_expr_type = getExprType(expr.get());
        if (current_expr_type == "__error__") error_in_block = true;
        if (i == node->expressions.size() - 1) last_expr_type = current_expr_type;
    }
    setExprType(node, error_in_block ? "__error__" : last_expr_type);
}

// Helper using the analyzer's public methods
std::string TypeChecker::getMethodReturnType(const std::string& className,
                                          const std::string& methodName,
                                          const std::vector<std::string>& argTypes) {

    std::optional<MethodSignature> sig_opt = analyzer.findMethodSignature(className, methodName);

    if (!sig_opt.has_value()) {
        // Check built-ins if class is Object (findMethodSignature should handle hierarchy already)
        if (className == "Object") {
             if (methodName == "print") { if (argTypes.size() == 1 && isSubtypeOf(argTypes[0], "string")) return "Object"; }
             else if (methodName == "printInt32") { if (argTypes.size() == 1 && isSubtypeOf(argTypes[0], "int32")) return "Object"; }
             else if (methodName == "inputInt32") { if (argTypes.empty()) return "int32"; }
             else if (methodName == "inputString") { if (argTypes.empty()) return "string"; }
        }
         // Report error if not found (findMethodSignature doesn't report)
         std::stringstream ss_args; for(size_t i=0; i<argTypes.size(); ++i) ss_args << (i > 0 ? ", " : "") << argTypes[i];
         reportError("Method '" + methodName + "' with argument types (" + ss_args.str() + ") not found in class '" + className + "' or its ancestors.");
         return "__error__";
    }

    // Method found, check arguments
    const MethodSignature& signature = sig_opt.value();
    if (signature.parameters.size() != argTypes.size()) {
         std::stringstream ss_args; for(size_t i=0; i<argTypes.size(); ++i) ss_args << (i > 0 ? ", " : "") << argTypes[i];
         reportError("Method '" + methodName + "' called with wrong number of arguments. Expected " +
                     std::to_string(signature.parameters.size()) + ", got " + std::to_string(argTypes.size()) +
                     " (" + ss_args.str() + ")");
         return "__error__";
    }

    for (size_t i = 0; i < argTypes.size(); ++i) {
        if (!isSubtypeOf(argTypes[i], signature.parameters[i].type.toString())) {
            std::stringstream ss_expected, ss_got;
             for(size_t j=0; j<signature.parameters.size(); ++j) ss_expected << (j > 0 ? ", " : "") << signature.parameters[j].type.toString();
             for(size_t j=0; j<argTypes.size(); ++j) ss_got << (j > 0 ? ", " : "") << argTypes[j];
            reportError("Argument type mismatch calling '" + methodName + "' on " + className + ". Parameter " + std::to_string(i+1) +
                        " expected " + signature.parameters[i].type.toString() + ", got " + argTypes[i] +
                         ". Full signature: (" + ss_expected.str() + "), call: (" + ss_got.str() + ")");
            return "__error__";
        }
    }

    // Signature matches
    return signature.returnType.toString();
}

} // namespace VSOP