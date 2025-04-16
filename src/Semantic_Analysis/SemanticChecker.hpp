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
    
    // Annotate the AST with type information
    void annotateAST();
    
    // Helper to print a node and its children
    void printNode(std::ostream& os, const ASTNode* node, int indent = 0) const;
    
    // Print specific node types
    void printClass(std::ostream& os, const Class* cls, int indent) const;
    void printField(std::ostream& os, const Field* field, int indent) const;
    void printMethod(std::ostream& os, const Method* method, int indent) const;
    void printExpression(std::ostream& os, const Expression* expr, int indent) const;
    
    // Get the type annotation for an expression
    std::string getTypeAnnotation(const Expression* expr) const;
};

} // namespace VSOP

#endif // SEMANTIC_CHECKER_HPP