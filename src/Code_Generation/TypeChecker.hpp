#ifndef TYPE_CHECKER_HPP
#define TYPE_CHECKER_HPP

#include "AST.hpp"
#include "SemanticAnalyzer.hpp"
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <sstream>

namespace VSOP {

class TypeChecker : public Visitor {
public:
    TypeChecker(const std::string& source_file);
    ~TypeChecker() = default;
    
    // Main entry point
    bool check(std::shared_ptr<Program> program);
    
    // Get the error messages
    const std::vector<std::string>& getErrors() const;
    
    // Visitor pattern implementation
    void visit(const Class* node) override;
    void visit(const Field* node) override;
    void visit(const Method* node) override;
    void visit(const Formal* node) override;
    void visit(const BinaryOp* node) override;
    void visit(const UnaryOp* node) override;
    void visit(const Call* node) override;
    void visit(const New* node) override;
    void visit(const Let* node) override;
    void visit(const If* node) override;
    void visit(const While* node) override;
    void visit(const Assign* node) override;
    void visit(const StringLiteral* node) override;
    void visit(const IntegerLiteral* node) override;
    void visit(const BooleanLiteral* node) override;
    void visit(const UnitLiteral* node) override;
    void visit(const Identifier* node) override;
    void visit(const Self* node) override;
    void visit(const Block* node) override;

private:
    // State
    std::string source_file;
    std::shared_ptr<Program> program;
    SemanticAnalyzer analyzer;
    
    // Current context
    std::string current_class;
    std::string current_method;
    std::unordered_map<std::string, std::string> symbol_types;
    
    // Track expression types
    std::unordered_map<const Expression*, std::string> expr_types;
    
    // Error tracking
    std::vector<std::string> errors;
    
    // Helper methods
    void reportError(const std::string& message, int line = 1, int col = 1);
    bool isSubtypeOf(const std::string& type, const std::string& parent_type);
    std::string getCommonAncestor(const std::string& type1, const std::string& type2);
    bool isValidType(const std::string& type);
    
    // Symbol table management
    void enterScope();
    void exitScope();
    void addSymbol(const std::string& name, const std::string& type);
    std::string lookupSymbol(const std::string& name);
    
    // Type management
    void setExprType(const Expression* expr, const std::string& type);
    std::string getExprType(const Expression* expr);
    
    // Node-specific helpers
    std::string getMethodReturnType(const std::string& class_name, const std::string& method_name, 
                                   const std::vector<std::string>& arg_types);
};

} // namespace VSOP

#endif // TYPE_CHECKER_HPP