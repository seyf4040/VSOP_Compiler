#include "SemanticChecker.hpp"
#include <iostream>
#include <sstream>
#include <unordered_map>

namespace VSOP {

// Store expression type annotations
static std::unordered_map<const Expression*, std::string> expr_types;

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
    
    // Annotate AST with type information
    annotateAST();
    
    return errors.empty();
}

// Annotate the AST with type information
void SemanticChecker::annotateAST() {
    // For each class, method, and field, collect type information
    for (const auto& cls : program->classes) {
        // For fields
        for (const auto& field : cls->fields) {
            if (field->init_expr) {
                // Annotate field initializer
                expr_types[field->init_expr.get()] = field->type;
            }
        }
        
        // For methods
        for (const auto& method : cls->methods) {
            if (method->body) {
                // Annotate method body
                expr_types[method->body.get()] = method->return_type;
                
                // Annotate expressions in the method body
                // This would be done by traversing the AST with a visitor
                // that collects type information, but we're relying on the
                // TypeChecker to have already done this
            }
        }
    }
}

// Print the typed AST
void SemanticChecker::printTypedAST(std::ostream& os) const {
    if (!program) {
        os << "[]" << std::endl;
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
    
    os << "]" << std::endl;
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
    auto it = expr_types.find(expr);
    if (it != expr_types.end()) {
        return it->second;
    }
    
    // If not found in our map, try to infer the type
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
        // This is a best guess without context
        // In a real implementation, we'd have the current class available
        return "Object";
    }
    
    // Default when type is unknown
    return "Object";
}

// Get error messages
const std::vector<std::string>& SemanticChecker::getErrors() const {
    return errors;
}

} // namespace VSOP