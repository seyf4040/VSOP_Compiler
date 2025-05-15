#ifndef SEMANTIC_CHECKER_HPP
#define SEMANTIC_CHECKER_HPP

#include "AST.hpp"
#include "SemanticAnalyzer.hpp"
#include "TypeChecker.hpp"
#include <string>
#include <vector>
#include <memory>

namespace VSOP {

class SemanticChecker {
public:
    SemanticChecker(const std::string& source_file);
    
    // Check semantics of a program
    bool check(std::shared_ptr<Program> program);
    
    // Print the typed AST
    void printTypedAST(std::ostream& os) const;
    
    // Get error messages
    const std::vector<std::string>& getErrors() const;

private:
    std::string source_file;
    std::shared_ptr<Program> program;
    std::vector<std::string> errors;
    
    // Type context building
    void buildTypeContext();
    void annotateMethodBody(const Expression* expr, const std::string& return_type);
    void annotateExpressionType(const Expression* expr, const std::string& expected_type = "");
    
    // Helper to print a node and its children
    void printNode(std::ostream& os, const ASTNode* node, int indent = 0) const;
    
    // Print specific node types
    void printClass(std::ostream& os, const Class* cls, int indent) const;
    void printField(std::ostream& os, const Field* field, int indent) const;
    void printMethod(std::ostream& os, const Method* method, int indent) const;
    void printExpression(std::ostream& os, const Expression* expr, int indent) const;
    
    // Get the type annotation for an expression
    std::string getTypeAnnotation(const Expression* expr) const;
    
    // Helper methods for type annotation
    const Field* findFieldWithName(const std::string& name) const;
    const Method* findMethodWithName(const std::string& name, const std::string& class_name = "") const;
};

} // namespace VSOP

#endif // SEMANTIC_CHECKER_HPP