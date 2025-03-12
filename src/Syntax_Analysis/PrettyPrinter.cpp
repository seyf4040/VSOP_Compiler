#include "PrettyPrinter.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace VSOP {

PrettyPrinter::PrettyPrinter(std::ostream& os) : os(os) {}

void PrettyPrinter::print(const Program* program) {
    os << "[";
    bool first = true;
    for (const auto& cls : program->classes) {
        if (!first) {
            os << ",\n";
        }
        first = false;
        cls->accept(this);
    }
    os << "]" << std::endl;
}

void PrettyPrinter::indent() {
    for (int i = 0; i < indent_level; ++i) {
        os << "  ";
    }
}

void PrettyPrinter::increase_indent() {
    indent_level++;
}

void PrettyPrinter::decrease_indent() {
    indent_level--;
}

void PrettyPrinter::visit(const Class* node) {
    os << "Class(" << node->name << ", " << node->parent << ", [";
    
    bool first = true;
    for (const auto& field : node->fields) {
        if (!first) {
            os << ", ";
        }
        first = false;
        field->accept(this);
    }
    
    os << "], [";
    
    first = true;
    for (const auto& method : node->methods) {
        if (!first) {
            os << ", ";
        }
        first = false;
        method->accept(this);
    }
    
    os << "])";
}

void PrettyPrinter::visit(const Field* node) {
    os << "Field(" << node->name << ", " << node->type;
    if (node->init_expr) {
        os << ", ";
        node->init_expr->accept(this);
    }
    os << ")";
}

void PrettyPrinter::visit(const Method* node) {
    os << "Method(" << node->name << ", [";
    
    bool first = true;
    for (const auto& formal : node->formals) {
        if (!first) {
            os << ", ";
        }
        first = false;
        formal->accept(this);
    }
    
    os << "], " << node->return_type << ", ";
    node->body->accept(this);
    os << ")";
}

void PrettyPrinter::visit(const Formal* node) {
    os << node->name << " : " << node->type;
}

void PrettyPrinter::visit(const BinaryOp* node) {
    os << "BinOp(" << node->op << ", ";
    node->left->accept(this);
    os << ", ";
    node->right->accept(this);
    os << ")";
}

void PrettyPrinter::visit(const UnaryOp* node) {
    os << "UnOp(" << node->op << ", ";
    node->expr->accept(this);
    os << ")";
}

void PrettyPrinter::visit(const Call* node) {
    os << "Call(";
    node->object->accept(this);
    os << ", " << node->method_name << ", [";
    
    bool first = true;
    for (const auto& arg : node->arguments) {
        if (!first) {
            os << ", ";
        }
        first = false;
        arg->accept(this);
    }
    
    os << "])";
}

void PrettyPrinter::visit(const New* node) {
    os << "New(" << node->type_name << ")";
}

void PrettyPrinter::visit(const Let* node) {
    os << "Let(" << node->name << ", " << node->type;
    
    if (node->init_expr) {
        os << ", ";
        node->init_expr->accept(this);
        os << ", ";
    } else {
        os << ", ";
    }
    
    node->scope_expr->accept(this);
    os << ")";
}

void PrettyPrinter::visit(const If* node) {
    os << "If(";
    node->condition->accept(this);
    os << ", ";
    node->then_expr->accept(this);
    
    if (node->else_expr) {
        os << ", ";
        node->else_expr->accept(this);
    }
    
    os << ")";
}

void PrettyPrinter::visit(const While* node) {
    os << "While(";
    node->condition->accept(this);
    os << ", ";
    node->body->accept(this);
    os << ")";
}

void PrettyPrinter::visit(const Assign* node) {
    os << "Assign(" << node->name << ", ";
    node->expr->accept(this);
    os << ")";
}

void PrettyPrinter::visit(const StringLiteral* node) {
    os << "\"" << escape_string(node->value) << "\"";
}

void PrettyPrinter::visit(const IntegerLiteral* node) {
    os << node->value;
}

void PrettyPrinter::visit(const BooleanLiteral* node) {
    os << (node->value ? "true" : "false");
}

void PrettyPrinter::visit(const UnitLiteral* node) {
    os << "()";
}

void PrettyPrinter::visit(const Identifier* node) {
    os << node->name;
}

void PrettyPrinter::visit(const Self* node) {
    os << "self";
}

void PrettyPrinter::visit(const Block* node) {
    if (node->expressions.size() == 1) {
        node->expressions[0]->accept(this);
    } else {
        os << "[";
        bool first = true;
        for (const auto& expr : node->expressions) {
            if (!first) {
                os << ", ";
            }
            first = false;
            expr->accept(this);
        }
        os << "]";
    }
}

std::string PrettyPrinter::escape_string(const std::string& str) {
    std::ostringstream result;
    for (char c : str) {
        if (c == '\n') {
            result << "\\x0a";
        } else if (c == '\t') {
            result << "\\x09";
        } else if (c == '\r') {
            result << "\\x0d";
        } else if (c == '\b') {
            result << "\\x08";
        } else if (c == '\\') {
            result << "\\\\";
        } else if (c == '\"') {
            result << "\\\"";
        } else if (c < 32 || c > 126) {
            result << "\\x" << std::hex << std::setw(2) << std::setfill('0') << (static_cast<int>(c) & 0xFF);
        } else {
            result << c;
        }
    }
    return result.str();
}

void PrettyPrinter::print_expression_list(const std::vector<std::shared_ptr<Expression>>& expressions) {
    os << "[";
    bool first = true;
    for (const auto& expr : expressions) {
        if (!first) {
            os << ", ";
        }
        first = false;
        expr->accept(this);
    }
    os << "]";
}

} // namespace VSOP