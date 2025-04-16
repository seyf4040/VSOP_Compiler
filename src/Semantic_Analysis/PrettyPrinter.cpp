#include "PrettyPrinter.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace VSOP {

PrettyPrinter::PrettyPrinter(std::ostream& os) : os(os) {}

void PrettyPrinter::print(const Program* program) {
    if (!program) {
        os << "[]";
        return;
    }

    os << "[";
    bool first = true;
    for (size_t i = 0; i < program->classes.size(); i++) {
        const auto& cls = program->classes[i];
        
        if (!cls) {
            std::cerr << "ERROR: Class at index " << i << " is null!" << std::endl;
            continue;
        }
        
        if (!first) {
            os << ",\n ";
        }
        first = false;
        
        cls->accept(this);
    }
    os << "]";
}

void PrettyPrinter::visit(const Class* node) {
    if (!node) {
        std::cerr << "ERROR: Class node is null in PrettyPrinter::visit" << std::endl;
        return;
    }
    
    os << "Class(" << node->name << ", " << node->parent << ",\n   [";
    
    bool first = true;
    for (const auto& field : node->fields) {
        if (!field) {
            std::cerr << "ERROR: Field is null in Class!" << std::endl;
            continue;
        }
        
        if (!first) {
            os << ",\n    ";
        } else {
            os << "\n    ";
        }
        first = false;
        field->accept(this);
    }
    
    if (!first) {
        os << "\n   ";
    }
    os << "],\n   [";
    
    first = true;
    for (const auto& method : node->methods) {
        if (!method) {
            std::cerr << "ERROR: Method is null in Class!" << std::endl;
            continue;
        }
        
        if (!first) {
            os << ",\n    ";
        } else {
            os << "\n    ";
        }
        first = false;
        method->accept(this);
    }
    
    if (!first) {
        os << "\n   ";
    }
    os << "])";
}

void PrettyPrinter::visit(const Field* node) {
    if (!node) {
        std::cerr << "ERROR: Field node is null" << std::endl;
        return;
    }
    
    os << "Field(" << node->name << ", " << node->type;
    if (node->init_expr) {
        os << ", ";
        node->init_expr->accept(this);
    }
    os << ")";
}

void PrettyPrinter::visit(const Method* node) {
    if (!node) {
        std::cerr << "ERROR: Method node is null" << std::endl;
        return;
    }
    
    os << "Method(" << node->name << ", [";
    
    bool first = true;
    for (const auto& formal : node->formals) {
        if (!formal) {
            std::cerr << "ERROR: Formal parameter is null in Method!" << std::endl;
            continue;
        }
        
        if (!first) {
            os << ", ";
        }
        first = false;
        formal->accept(this);
    }
    
    os << "], " << node->return_type << ",\n      ";
    if (node->body) {
        // Always wrap method body expressions in brackets
        os << "[";
        bool firstExpr = true;
        for (const auto& expr : node->body->expressions) {
            if (!expr) {
                std::cerr << "ERROR: Expression in Method body is null!" << std::endl;
                continue;
            }
            
            if (!firstExpr) {
                os << ",\n       ";
            }
            firstExpr = false;
            expr->accept(this);
        }
        os << "]";
    } else {
        std::cerr << "ERROR: Method body is null!" << std::endl;
        os << "[]"; // Empty body as fallback
    }
    os << ")";
}

void PrettyPrinter::visit(const Formal* node) {
    if (!node) {
        std::cerr << "ERROR: Formal node is null" << std::endl;
        return;
    }
    
    os << node->name << " : " << node->type;
}

void PrettyPrinter::visit(const BinaryOp* node) {
    if (!node) {
        std::cerr << "ERROR: BinaryOp node is null" << std::endl;
        return;
    }
    
    os << "BinOp(" << node->op << ", ";
    if (node->left) {
        node->left->accept(this);
    } else {
        std::cerr << "ERROR: BinaryOp left operand is null!" << std::endl;
        os << "null";
    }
    os << ", ";
    if (node->right) {
        node->right->accept(this);
    } else {
        std::cerr << "ERROR: BinaryOp right operand is null!" << std::endl;
        os << "null";
    }
    os << ")";
}

void PrettyPrinter::visit(const UnaryOp* node) {
    if (!node) {
        std::cerr << "ERROR: UnaryOp node is null" << std::endl;
        return;
    }
    
    os << "UnOp(" << node->op << ", ";
    if (node->expr) {
        node->expr->accept(this);
    } else {
        std::cerr << "ERROR: UnaryOp expression is null!" << std::endl;
        os << "null";
    }
    os << ")";
}

void PrettyPrinter::visit(const Call* node) {
    if (!node) {
        std::cerr << "ERROR: Call node is null" << std::endl;
        return;
    }
    
    os << "Call(";
    if (node->object) {
        node->object->accept(this);
    } else {
        std::cerr << "ERROR: Call object is null!" << std::endl;
        os << "null";
    }
    os << ", " << node->method_name << ", [";
    
    bool first = true;
    for (const auto& arg : node->arguments) {
        if (!arg) {
            std::cerr << "ERROR: Call argument is null!" << std::endl;
            continue;
        }
        
        if (!first) {
            os << ", ";
        }
        first = false;
        arg->accept(this);
    }
    
    os << "])";
}

void PrettyPrinter::visit(const New* node) {
    if (!node) {
        std::cerr << "ERROR: New node is null" << std::endl;
        return;
    }
    
    os << "New(" << node->type_name << ")";
}

void PrettyPrinter::visit(const Let* node) {
    if (!node) {
        std::cerr << "ERROR: Let node is null" << std::endl;
        return;
    }
    
    os << "Let(" << node->name << ", " << node->type;
    
    if (node->init_expr) {
        os << ", ";
        node->init_expr->accept(this);
        os << ", ";
    } else {
        os << ", ";
    }
    
    if (node->scope_expr) {
        node->scope_expr->accept(this);
    } else {
        std::cerr << "ERROR: Let scope expression is null!" << std::endl;
        os << "null";
    }
    os << ")";
}

void PrettyPrinter::visit(const If* node) {
    if (!node) {
        std::cerr << "ERROR: If node is null" << std::endl;
        return;
    }
    
    os << "If(";
    if (node->condition) {
        node->condition->accept(this);
    } else {
        std::cerr << "ERROR: If condition is null!" << std::endl;
        os << "null";
    }
    os << ", ";
    if (node->then_expr) {
        node->then_expr->accept(this);
    } else {
        std::cerr << "ERROR: If then expression is null!" << std::endl;
        os << "null";
    }
    
    if (node->else_expr) {
        os << ", ";
        node->else_expr->accept(this);
    }
    
    os << ")";
}

void PrettyPrinter::visit(const While* node) {
    if (!node) {
        std::cerr << "ERROR: While node is null" << std::endl;
        return;
    }
    
    os << "While(";
    if (node->condition) {
        node->condition->accept(this);
    } else {
        std::cerr << "ERROR: While condition is null!" << std::endl;
        os << "null";
    }
    os << ", ";
    if (node->body) {
        node->body->accept(this);
    } else {
        std::cerr << "ERROR: While body is null!" << std::endl;
        os << "null";
    }
    os << ")";
}

void PrettyPrinter::visit(const Assign* node) {
    if (!node) {
        std::cerr << "ERROR: Assign node is null" << std::endl;
        return;
    }
    
    os << "Assign(" << node->name << ", ";
    if (node->expr) {
        node->expr->accept(this);
    } else {
        std::cerr << "ERROR: Assign expression is null!" << std::endl;
        os << "null";
    }
    os << ")";
}

void PrettyPrinter::visit(const StringLiteral* node) {
    if (!node) {
        std::cerr << "ERROR: StringLiteral node is null" << std::endl;
        return;
    }
    
    os << "\"" << format_string_literal(node->value) << "\"";
}

void PrettyPrinter::visit(const IntegerLiteral* node) {
    if (!node) {
        std::cerr << "ERROR: IntegerLiteral node is null" << std::endl;
        return;
    }
    
    os << node->value;
}

void PrettyPrinter::visit(const BooleanLiteral* node) {
    if (!node) {
        std::cerr << "ERROR: BooleanLiteral node is null" << std::endl;
        return;
    }
    
    os << (node->value ? "true" : "false");
}

void PrettyPrinter::visit(const UnitLiteral* node) {
    if (!node) {
        std::cerr << "ERROR: UnitLiteral node is null" << std::endl;
        return;
    }
    
    os << "()";
}

void PrettyPrinter::visit(const Identifier* node) {
    if (!node) {
        std::cerr << "ERROR: Identifier node is null" << std::endl;
        return;
    }
    
    os << node->name;
}

void PrettyPrinter::visit(const Self* node) {
    if (!node) {
        std::cerr << "ERROR: Self node is null" << std::endl;
        return;
    }
    
    os << "self";
}

void PrettyPrinter::visit(const Block* node) {
    if (!node) {
        std::cerr << "ERROR: Block node is null" << std::endl;
        os << "[]";
        return;
    }
    
    os << "[";
    bool first = true;
    for (const auto& expr : node->expressions) {
        if (!expr) {
            std::cerr << "ERROR: Expression in Block is null!" << std::endl;
            continue;
        }
        
        if (!first) {
            os << ", ";
        }
        first = false;
        expr->accept(this);
    }
    os << "]";
}

// Format the string literal for printing in the AST
// This is directly using escaped sequences in the format the reference compiler expects
std::string PrettyPrinter::format_string_literal(const std::string& str) {
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
            result << "\\";
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

std::string PrettyPrinter::escape_string(const std::string& str) {
    // Same implementation as format_string_literal for now
    return format_string_literal(str);
}

} // namespace VSOP