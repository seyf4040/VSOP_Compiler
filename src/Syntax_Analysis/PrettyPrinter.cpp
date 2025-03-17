#include "PrettyPrinter.hpp"
#include <iostream>
#include <sstream>
#include <iomanip>

namespace VSOP {

PrettyPrinter::PrettyPrinter(std::ostream& os) : os(os) {
    std::cerr << "DEBUG: PrettyPrinter constructor called" << std::endl;
}

void PrettyPrinter::print(const Program* program) {
    std::cerr << "DEBUG: PrettyPrinter::print called" << std::endl;
    if (!program) {
        std::cerr << "ERROR: Program is null in PrettyPrinter::print" << std::endl;
        os << "[]";
        return;
    }

    std::cerr << "DEBUG: Program has " << program->classes.size() << " classes" << std::endl;
    
    os << "[";
    bool first = true;
    for (size_t i = 0; i < program->classes.size(); i++) {
        std::cerr << "DEBUG: Processing class " << i << std::endl;
        const auto& cls = program->classes[i];
        
        if (!cls) {
            std::cerr << "ERROR: Class at index " << i << " is null!" << std::endl;
            continue;
        }
        
        if (!first) {
            os << ",\n";
        }
        first = false;
        
        std::cerr << "DEBUG: About to call accept() on class " << cls->name << std::endl;
        cls->accept(this);
        std::cerr << "DEBUG: Finished calling accept() on class " << cls->name << std::endl;
    }
    os << "]";
    std::cerr << "DEBUG: PrettyPrinter::print finished" << std::endl;
}

void PrettyPrinter::visit(const Class* node) {
    std::cerr << "DEBUG: PrettyPrinter::visit(Class) called" << std::endl;
    if (!node) {
        std::cerr << "ERROR: Class node is null in PrettyPrinter::visit" << std::endl;
        return;
    }
    
    std::cerr << "DEBUG: Class name=" << node->name << ", parent=" << node->parent << std::endl;
    os << "Class(" << node->name << ", " << node->parent << ", [";
    
    bool first = true;
    for (const auto& field : node->fields) {
        if (!field) {
            std::cerr << "ERROR: Field is null in Class!" << std::endl;
            continue;
        }
        
        if (!first) {
            os << ", ";
        }
        first = false;
        field->accept(this);
    }
    
    os << "], [";
    
    first = true;
    for (const auto& method : node->methods) {
        if (!method) {
            std::cerr << "ERROR: Method is null in Class!" << std::endl;
            continue;
        }
        
        if (!first) {
            os << ", ";
        }
        first = false;
        method->accept(this);
    }
    
    os << "])";
    std::cerr << "DEBUG: PrettyPrinter::visit(Class) finished" << std::endl;
}

void PrettyPrinter::visit(const Field* node) {
    std::cerr << "DEBUG: PrettyPrinter::visit(Field) called" << std::endl;
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
    std::cerr << "DEBUG: PrettyPrinter::visit(Field) finished" << std::endl;
}

void PrettyPrinter::visit(const Method* node) {
    std::cerr << "DEBUG: PrettyPrinter::visit(Method) called" << std::endl;
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
    
    os << "], " << node->return_type << ", ";
    if (node->body) {
        node->body->accept(this);
    } else {
        std::cerr << "ERROR: Method body is null!" << std::endl;
        os << "[]"; // Empty body as fallback
    }
    os << ")";
    std::cerr << "DEBUG: PrettyPrinter::visit(Method) finished" << std::endl;
}

void PrettyPrinter::visit(const Formal* node) {
    if (!node) {
        std::cerr << "ERROR: Formal node is null" << std::endl;
        return;
    }
    
    os << node->name << " : " << node->type;
}

void PrettyPrinter::visit(const BinaryOp* node) {
    std::cerr << "DEBUG: PrettyPrinter::visit(BinaryOp) called" << std::endl;
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
    std::cerr << "DEBUG: PrettyPrinter::visit(BinaryOp) finished" << std::endl;
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
    
    os << "\"" << escape_string(node->value) << "\"";
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
    std::cerr << "DEBUG: PrettyPrinter::visit(Block) called" << std::endl;
    if (!node) {
        std::cerr << "ERROR: Block node is null" << std::endl;
        os << "[]";
        return;
    }
    
    std::cerr << "DEBUG: Block has " << node->expressions.size() << " expressions" << std::endl;
    
    if (node->expressions.size() == 1 && node->expressions[0]) {
        node->expressions[0]->accept(this);
    } else {
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
    std::cerr << "DEBUG: PrettyPrinter::visit(Block) finished" << std::endl;
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

} // namespace VSOP