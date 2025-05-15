#ifndef PRETTY_PRINTER_HPP
#define PRETTY_PRINTER_HPP

#include "AST.hpp"
#include <iostream>
#include <sstream>

namespace VSOP {

class PrettyPrinter : public Visitor {
public:
    PrettyPrinter(std::ostream& os = std::cout);
    
    void print(const Program* program);
    
private:
    std::ostream& os;
    int indent_level = 0;
    
    void indent();
    void increase_indent();
    void decrease_indent();
    
    // Visitor implementation
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
    
    std::string format_string_literal(const std::string& str);
    std::string escape_string(const std::string& str);
    void print_expression_list(const std::vector<std::shared_ptr<Expression>>& expressions);
};

} // namespace VSOP

#endif // PRETTY_PRINTER_HPP