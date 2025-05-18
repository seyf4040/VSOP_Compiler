#include "SemanticChecker.hpp"
#include <iostream>
#include <sstream>
#include <unordered_map>

namespace VSOP {

// Store expression type annotations and current context
static std::unordered_map<const Expression*, std::string> expr_types;
static std::string current_class_name;
static std::string current_method_name;
static std::unordered_map<std::string, std::string> current_params;
static std::unordered_map<std::string, std::string> current_locals;

// Constructor
SemanticChecker::SemanticChecker(const std::string& source_file)
    : source_file(source_file) {
}

// Main entry point for semantic checking
bool SemanticChecker::check(std::shared_ptr<Program> prog) {
    program = prog;
    
    // Clear state
    errors.clear();
    expr_types.clear();
    current_class_name = "";
    current_method_name = "";
    current_params.clear();
    current_locals.clear();
    
    // Create analyzer and type checker
    SemanticAnalyzer analyzer;
    
    // Analyze program semantics
    if (!analyzer.analyze(program)) {
        // Collect errors from analyzer
        const auto& analyzer_errors = analyzer.getErrors();
        errors.insert(errors.end(), analyzer_errors.begin(), analyzer_errors.end());
        return false;
    }
    
    // Type check program
    TypeChecker checker(source_file);
    if (!checker.check(program)) {
        // Collect errors from type checker
        const auto& checker_errors = checker.getErrors();
        errors.insert(errors.end(), checker_errors.begin(), checker_errors.end());
        return false;
    }
    
    // Build a more complete type context by visiting the AST
    buildTypeContext();
    
    return errors.empty();
}

void SemanticChecker::buildTypeContext() {
    // Build param and local variable type info for better expression typing
    for (const auto& cls : program->classes) {
        current_class_name = cls->name;
        
        // Process fields
        for (const auto& field : cls->fields) {
            if (field->init_expr) {
                annotateExpressionType(field->init_expr.get(), field->type);
            }
        }
        
        // Process methods
        for (const auto& method : cls->methods) {
            current_method_name = method->name;
            current_params.clear();
            
            // Add method parameters to context
            for (const auto& formal : method->formals) {
                current_params[formal->name] = formal->type;
            }
            
            // Process method body
            if (method->body) {
                annotateMethodBody(method->body.get(), method->return_type);
            }
            
            current_method_name = "";
            current_params.clear();
        }
        
        current_class_name = "";
    }
}

void SemanticChecker::annotateMethodBody(const Expression* expr, const std::string& return_type) {
    if (!expr) return;
    
    // Process different expressions
    if (const Block* block = dynamic_cast<const Block*>(expr)) {
        // Initialize with unit, will be updated to last expression's type if not empty
        annotateExpressionType(block, block->expressions.empty() ? "unit" : return_type);
        
        // Process all expressions in the block
        for (size_t i = 0; i < block->expressions.size(); i++) {
            const auto& blockExpr = block->expressions[i];
            if (!blockExpr) continue;
            
            // Last expression in method body should have method's return type
            if (i == block->expressions.size() - 1) {
                annotateExpressionType(blockExpr.get(), return_type);
            } else {
                annotateExpressionType(blockExpr.get());
            }
        }
    } else {
        // Single expression body
        annotateExpressionType(expr, return_type);
    }
}

void SemanticChecker::annotateExpressionType(const Expression* expr, const std::string& expected_type) {
    if (!expr) return;
    
    // Set expected type if provided
    if (!expected_type.empty()) {
        expr_types[expr] = expected_type;
    }
    
    // Process specific expression types
    if (const StringLiteral* stringLit = dynamic_cast<const StringLiteral*>(expr)) {
        expr_types[expr] = "string";
    }
    else if (const IntegerLiteral* intLit = dynamic_cast<const IntegerLiteral*>(expr)) {
        expr_types[expr] = "int32";
    }
    else if (const BooleanLiteral* boolLit = dynamic_cast<const BooleanLiteral*>(expr)) {
        expr_types[expr] = "bool";
    }
    else if (const UnitLiteral* unitLit = dynamic_cast<const UnitLiteral*>(expr)) {
        expr_types[expr] = "unit";
    }
    else if (const Self* self = dynamic_cast<const Self*>(expr)) {
        expr_types[expr] = current_class_name;
    }
    else if (const Identifier* id = dynamic_cast<const Identifier*>(expr)) {
        // Check local variables first (they shadow parameters and fields)
        if (current_locals.count(id->name)) {
            expr_types[expr] = current_locals[id->name];
        } 
        // Then check parameters
        else if (current_params.count(id->name)) {
            expr_types[expr] = current_params[id->name];
        }
        // Then field in current class
        else {
            const Field* field = findFieldWithName(id->name);
            if (field) {
                expr_types[expr] = field->type;
            }
        }
    }
    else if (const New* newExpr = dynamic_cast<const New*>(expr)) {
        expr_types[expr] = newExpr->type_name;
    }
    else if (const BinaryOp* binop = dynamic_cast<const BinaryOp*>(expr)) {
        // Visit operands first
        if (binop->left) annotateExpressionType(binop->left.get());
        if (binop->right) annotateExpressionType(binop->right.get());
        
        // Set result type based on operation
        if (binop->op == "+" || binop->op == "-" || binop->op == "*" || 
            binop->op == "/" || binop->op == "^") {
            expr_types[expr] = "int32";
        }
        else if (binop->op == "<" || binop->op == "<=" || binop->op == "=" || 
                 binop->op == "and") {
            expr_types[expr] = "bool";
        }
    }
    else if (const UnaryOp* unop = dynamic_cast<const UnaryOp*>(expr)) {
        // Visit operand first
        if (unop->expr) annotateExpressionType(unop->expr.get());
        
        // Set result type based on operation
        if (unop->op == "-") {
            expr_types[expr] = "int32";
        }
        else if (unop->op == "not" || unop->op == "isnull") {
            expr_types[expr] = "bool";
        }
    }
    else if (const Assign* assign = dynamic_cast<const Assign*>(expr)) {
        // Visit right-hand side first
        if (assign->expr) {
            annotateExpressionType(assign->expr.get());
            // Assignment has the type of the right-hand side
            if (expr_types.count(assign->expr.get())) {
                expr_types[expr] = expr_types[assign->expr.get()];
            }
        }
    }
    else if (const If* ifExpr = dynamic_cast<const If*>(expr)) {
        // Visit condition, then branches
        if (ifExpr->condition) annotateExpressionType(ifExpr->condition.get(), "bool");
        if (ifExpr->then_expr) annotateExpressionType(ifExpr->then_expr.get());
        if (ifExpr->else_expr) {
            annotateExpressionType(ifExpr->else_expr.get());
            // If-else has the expected type if provided, otherwise the branches determine it
            if (expected_type.empty() && expr_types.count(ifExpr->then_expr.get())) {
                expr_types[expr] = expr_types[ifExpr->then_expr.get()];
            }
        } else {
            // If without else is always unit
            expr_types[expr] = "unit";
        }
    }
    else if (const While* whileExpr = dynamic_cast<const While*>(expr)) {
        // Visit condition and body
        if (whileExpr->condition) annotateExpressionType(whileExpr->condition.get(), "bool");
        if (whileExpr->body) annotateExpressionType(whileExpr->body.get());
        // While expression always has unit type
        expr_types[expr] = "unit";
    }
    else if (const Let* letExpr = dynamic_cast<const Let*>(expr)) {
        // Visit initializer if present
        if (letExpr->init_expr) {
            annotateExpressionType(letExpr->init_expr.get(), letExpr->type);
        }
        
        // Remember the variable for scope
        current_locals[letExpr->name] = letExpr->type;
        
        // Visit scope expression
        if (letExpr->scope_expr) {
            annotateExpressionType(letExpr->scope_expr.get());
            // Let has the type of its scope expression
            if (expr_types.count(letExpr->scope_expr.get())) {
                expr_types[expr] = expr_types[letExpr->scope_expr.get()];
            }
        }
        
        // Remove variable after scope
        current_locals.erase(letExpr->name);
    }
    else if (const Block* blockExpr = dynamic_cast<const Block*>(expr)) {
        // Visit all expressions in the block
        for (const auto& blockExpr : blockExpr->expressions) {
            if (blockExpr) annotateExpressionType(blockExpr.get());
        }
        
        // Block has type of last expression or unit if empty
        if (!blockExpr->expressions.empty() && blockExpr->expressions.back()) {
            const Expression* lastExpr = blockExpr->expressions.back().get();
            if (expr_types.count(lastExpr)) {
                expr_types[expr] = expr_types[lastExpr];
            }
        } else {
            expr_types[expr] = "unit";
        }
    }
    else if (const Call* callExpr = dynamic_cast<const Call*>(expr)) {
        // Visit object and arguments
        if (callExpr->object) {
            annotateExpressionType(callExpr->object.get());
        }
        
        for (const auto& arg : callExpr->arguments) {
            if (arg) annotateExpressionType(arg.get());
        }
        
        // For method calls, look up the method's return type
        std::string object_class = current_class_name;
        if (callExpr->object) {
            if (expr_types.count(callExpr->object.get())) {
                object_class = expr_types[callExpr->object.get()];
            }
        }
        
        // Check for built-in Object methods
        if (callExpr->method_name == "print" || callExpr->method_name == "printInt32") {
            expr_types[expr] = "Object";
        }
        else if (callExpr->method_name == "inputInt32") {
            expr_types[expr] = "int32";
        }
        else if (callExpr->method_name == "inputString") {
            expr_types[expr] = "string";
        }
        else {
            // Look for the method in the class
            const Method* method = findMethodWithName(callExpr->method_name, object_class);
            if (method) {
                expr_types[expr] = method->return_type;
            }
        }
    }
}

// Print the typed AST
void SemanticChecker::printTypedAST(std::ostream& os) const {
    if (!program) {
        os << "[]";
        return;
    }
    
    os << "[";
    bool first_class = true;
    
    for (const auto& cls : program->classes) {
        if (!first_class) {
            os << ",\n ";
        }
        first_class = false;
        
        printClass(os, cls.get(), 1);
    }
    
    os << "]";
}

// Print a class node
void SemanticChecker::printClass(std::ostream& os, const Class* cls, int indent) const {
    os << "Class(" << cls->name << ", " << cls->parent << ",\n";
    
    // Indent for fields
    os << std::string(indent * 3, ' ') << "[";
    bool first_field = true;
    
    for (const auto& field : cls->fields) {
        if (!first_field) {
            os << ",\n" << std::string(indent * 3 + 1, ' ');
        } else if (!cls->fields.empty()) {
            os << "\n" << std::string(indent * 3 + 1, ' ');
        }
        first_field = false;
        
        printField(os, field.get(), indent + 1);
    }
    
    if (!cls->fields.empty()) {
        os << "\n" << std::string(indent * 3, ' ');
    }
    os << "],\n";
    
    // Indent for methods
    os << std::string(indent * 3, ' ') << "[";
    bool first_method = true;
    
    for (const auto& method : cls->methods) {
        if (!first_method) {
            os << ",\n" << std::string(indent * 3 + 1, ' ');
        } else if (!cls->methods.empty()) {
            os << "\n" << std::string(indent * 3 + 1, ' ');
        }
        first_method = false;
        
        printMethod(os, method.get(), indent + 1);
    }
    
    if (!cls->methods.empty()) {
        os << "\n" << std::string(indent * 3, ' ');
    }
    os << "])";
}

// Print a field node
void SemanticChecker::printField(std::ostream& os, const Field* field, int indent) const {
    os << "Field(" << field->name << ", " << field->type;
    
    if (field->init_expr) {
        os << ", ";
        printExpression(os, field->init_expr.get(), indent);
    }
    
    os << ")";
}

// Print a method node
void SemanticChecker::printMethod(std::ostream& os, const Method* method, int indent) const {
    os << "Method(" << method->name << ", [";
    
    // Print formal parameters
    bool first_formal = true;
    for (const auto& formal : method->formals) {
        if (!first_formal) {
            os << ", ";
        }
        first_formal = false;
        
        os << formal->name << " : " << formal->type;
    }
    
    os << "], " << method->return_type << ",\n";
    os << std::string(indent * 3 + 6, ' ');
    
    if (method->body) {
        printExpression(os, method->body.get(), indent + 2);
    } else {
        os << "[]";
    }
    
    os << ")";
}

// Print an expression node
void SemanticChecker::printExpression(std::ostream& os, const Expression* expr, int indent) const {
    if (const BinaryOp* binop = dynamic_cast<const BinaryOp*>(expr)) {
        os << "BinOp(" << binop->op << ", ";
        printExpression(os, binop->left.get(), indent);
        os << ", ";
        printExpression(os, binop->right.get(), indent);
        os << ")";
    }
    else if (const UnaryOp* unop = dynamic_cast<const UnaryOp*>(expr)) {
        os << "UnOp(" << unop->op << ", ";
        printExpression(os, unop->expr.get(), indent);
        os << ")";
    }
    else if (const Call* call = dynamic_cast<const Call*>(expr)) {
        os << "Call(";
        if (call->object) {
            printExpression(os, call->object.get(), indent);
        } else {
            os << "self";
        }
        
        os << ", " << call->method_name << ", [";
        
        bool first_arg = true;
        for (const auto& arg : call->arguments) {
            if (!first_arg) {
                os << ", ";
            }
            first_arg = false;
            
            printExpression(os, arg.get(), indent);
        }
        
        os << "])";
    }
    else if (const New* newExpr = dynamic_cast<const New*>(expr)) {
        os << "New(" << newExpr->type_name << ")";
    }
    else if (const Let* let = dynamic_cast<const Let*>(expr)) {
        os << "Let(" << let->name << ", " << let->type;
        
        if (let->init_expr) {
            os << ", ";
            printExpression(os, let->init_expr.get(), indent);
        }
        
        os << ", ";
        printExpression(os, let->scope_expr.get(), indent);
        os << ")";
    }
    else if (const If* ifExpr = dynamic_cast<const If*>(expr)) {
        os << "If(";
        printExpression(os, ifExpr->condition.get(), indent);
        os << ", ";
        printExpression(os, ifExpr->then_expr.get(), indent);
        
        if (ifExpr->else_expr) {
            os << ", ";
            printExpression(os, ifExpr->else_expr.get(), indent);
        }
        
        os << ")";
    }
    else if (const While* whileExpr = dynamic_cast<const While*>(expr)) {
        os << "While(";
        printExpression(os, whileExpr->condition.get(), indent);
        os << ", ";
        printExpression(os, whileExpr->body.get(), indent);
        os << ")";
    }
    else if (const Assign* assign = dynamic_cast<const Assign*>(expr)) {
        os << "Assign(" << assign->name << ", ";
        printExpression(os, assign->expr.get(), indent);
        os << ")";
    }
    else if (const StringLiteral* stringLit = dynamic_cast<const StringLiteral*>(expr)) {
        os << "\"" << stringLit->value << "\"";
    }
    else if (const IntegerLiteral* intLit = dynamic_cast<const IntegerLiteral*>(expr)) {
        os << intLit->value;
    }
    else if (const BooleanLiteral* boolLit = dynamic_cast<const BooleanLiteral*>(expr)) {
        os << (boolLit->value ? "true" : "false");
    }
    else if (const UnitLiteral* unitLit = dynamic_cast<const UnitLiteral*>(expr)) {
        os << "()";
    }
    else if (const Identifier* id = dynamic_cast<const Identifier*>(expr)) {
        os << id->name;
    }
    else if (const Self* self = dynamic_cast<const Self*>(expr)) {
        os << "self";
    }
    else if (const Block* block = dynamic_cast<const Block*>(expr)) {
        os << "[";
        
        bool first_expr = true;
        for (const auto& blockExpr : block->expressions) {
            if (!first_expr) {
                os << ", ";
            }
            first_expr = false;
            
            printExpression(os, blockExpr.get(), indent);
        }
        
        os << "]";
    }
    else {
        os << "UnknownExpression";
    }
    
    // Append type annotation
    os << " : " << getTypeAnnotation(expr);
}

// Get the type annotation for an expression
std::string SemanticChecker::getTypeAnnotation(const Expression* expr) const {
    // First check our expression type map
    auto it = expr_types.find(expr);
    if (it != expr_types.end()) {
        return it->second;
    }
    
    // If not in our map, infer based on expression kind
    if (const StringLiteral* stringLit = dynamic_cast<const StringLiteral*>(expr)) {
        return "string";
    }
    else if (const IntegerLiteral* intLit = dynamic_cast<const IntegerLiteral*>(expr)) {
        return "int32";
    }
    else if (const BooleanLiteral* boolLit = dynamic_cast<const BooleanLiteral*>(expr)) {
        return "bool";
    }
    else if (const UnitLiteral* unitLit = dynamic_cast<const UnitLiteral*>(expr)) {
        return "unit";
    }
    else if (const Self* self = dynamic_cast<const Self*>(expr)) {
        // Use the surrounding class
        if (!current_class_name.empty()) {
            return current_class_name;
        }
        return "Object";
    }
    else if (const New* newExpr = dynamic_cast<const New*>(expr)) {
        return newExpr->type_name;
    }
    else if (const Identifier* id = dynamic_cast<const Identifier*>(expr)) {
        // Check parameters first
        if (current_params.count(id->name)) {
            return current_params[id->name];
        } 
        // Then local variables
        else if (current_locals.count(id->name)) {
            return current_locals[id->name];
        }
        // Then field in current class
        else {
            const Field* field = findFieldWithName(id->name);
            if (field) {
                return field->type;
            }
            return id->name; // As fallback
        }
    }
    else if (const BinaryOp* binop = dynamic_cast<const BinaryOp*>(expr)) {
        // Binary operators with known result types
        if (binop->op == "+" || binop->op == "-" || binop->op == "*" || 
            binop->op == "/" || binop->op == "^") {
            return "int32";
        }
        else if (binop->op == "<" || binop->op == "<=" || binop->op == "=" || 
                 binop->op == "and") {
            return "bool";
        }
    }
    else if (const UnaryOp* unop = dynamic_cast<const UnaryOp*>(expr)) {
        // Unary operators with known result types
        if (unop->op == "-") {
            return "int32";
        }
        else if (unop->op == "not" || unop->op == "isnull") {
            return "bool";
        }
    }
    else if (const If* ifExpr = dynamic_cast<const If*>(expr)) {
        // If with no else is unit
        if (!ifExpr->else_expr) {
            return "unit";
        }
        
        // If-else takes type of branches
        if (ifExpr->then_expr) {
            return getTypeAnnotation(ifExpr->then_expr.get());
        }
    }
    else if (const While* whileExpr = dynamic_cast<const While*>(expr)) {
        return "unit";
    }
    else if (const Let* letExpr = dynamic_cast<const Let*>(expr)) {
        // Let has type of scope
        if (letExpr->scope_expr) {
            return getTypeAnnotation(letExpr->scope_expr.get());
        }
        return letExpr->type; // Fallback
    }
    else if (const Block* blockExpr = dynamic_cast<const Block*>(expr)) {
        // Block has type of last expression or unit
        if (!blockExpr->expressions.empty()) {
            return getTypeAnnotation(blockExpr->expressions.back().get());
        }
        return "unit";
    }
    else if (const Call* callExpr = dynamic_cast<const Call*>(expr)) {
        // Built-in methods
        if (callExpr->method_name == "print" || callExpr->method_name == "printInt32") {
            return "Object";
        }
        else if (callExpr->method_name == "inputInt32") {
            return "int32";
        }
        else if (callExpr->method_name == "inputString") {
            return "string";
        }
        
        // Regular methods - find in current class or other
        std::string object_class = "";
        if (callExpr->object) {
            // If we know the object type, use that
            auto obj_it = expr_types.find(callExpr->object.get());
            if (obj_it != expr_types.end()) {
                object_class = obj_it->second;
            } else {
                // Otherwise, try to infer
                object_class = getTypeAnnotation(callExpr->object.get());
            }
        } else {
            // Self call - use current class
            object_class = current_class_name;
        }
        
        const Method* method = findMethodWithName(callExpr->method_name, object_class);
        if (method) {
            return method->return_type;
        }
    }
    
    // Default when can't be inferred
    return "Object";
}

// Helper methods for type annotation
const Field* SemanticChecker::findFieldWithName(const std::string& name) const {
    // Look in the current class first
    for (const auto& cls : program->classes) {
        if (cls->name == current_class_name) {
            for (const auto& field : cls->fields) {
                if (field && field->name == name) {
                    return field.get();
                }
            }
        }
    }
    
    // If not found in current class, look in all classes
    for (const auto& cls : program->classes) {
        for (const auto& field : cls->fields) {
            if (field && field->name == name) {
                return field.get();
            }
        }
    }
    
    return nullptr;
}

const Method* SemanticChecker::findMethodWithName(const std::string& name, const std::string& class_name) const {
    // If class specified, look there first
    if (!class_name.empty()) {
        for (const auto& cls : program->classes) {
            if (cls->name == class_name) {
                for (const auto& method : cls->methods) {
                    if (method && method->name == name) {
                        return method.get();
                    }
                }
                break; // Stop searching in this class
            }
        }
    }
    
    // Look in current class (if different from specified class)
    if (class_name != current_class_name && !current_class_name.empty()) {
        for (const auto& cls : program->classes) {
            if (cls->name == current_class_name) {
                for (const auto& method : cls->methods) {
                    if (method && method->name == name) {
                        return method.get();
                    }
                }
                break; // Stop searching in this class
            }
        }
    }
    
    // If not found in specified classes, look in all classes
    for (const auto& cls : program->classes) {
        for (const auto& method : cls->methods) {
            if (method && method->name == name) {
                return method.get();
            }
        }
    }
    
    return nullptr;
}

// Get error messages
const std::vector<std::string>& SemanticChecker::getErrors() const {
    return errors;
}

} // namespace VSOP