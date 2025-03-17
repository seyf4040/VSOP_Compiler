#include "AST.hpp"
#include <iostream>

namespace VSOP {

void Program::accept(Visitor* visitor) const {
    std::cerr << "DEBUG: Program::accept called with " << classes.size() << " classes" << std::endl;
    for (size_t i = 0; i < classes.size(); i++) {
        std::cerr << "DEBUG: Accepting class " << i << std::endl;
        if (!classes[i]) {
            std::cerr << "ERROR: Class " << i << " is null in Program::accept!" << std::endl;
            continue;
        }
        classes[i]->accept(visitor);
    }
    std::cerr << "DEBUG: Program::accept finished" << std::endl;
}

Class::Class(const std::string& name, const std::string& parent)
    : name(name), parent(parent) {
    std::cerr << "DEBUG: Class constructor: name=" << name << ", parent=" << parent << std::endl;
}

void Class::accept(Visitor* visitor) const {
    std::cerr << "DEBUG: Class::accept called for " << name << std::endl;
    if (!visitor) {
        std::cerr << "ERROR: Visitor is null in Class::accept!" << std::endl;
        return;
    }
    visitor->visit(this);
    std::cerr << "DEBUG: Class::accept finished" << std::endl;
}

Field::Field(const std::string& name, const std::string& type, std::shared_ptr<Expression> init_expr)
    : name(name), type(type), init_expr(init_expr) {}

void Field::accept(Visitor* visitor) const {
    visitor->visit(this);
}

Method::Method(const std::string& name, std::vector<std::shared_ptr<Formal>> formals, 
               const std::string& return_type, std::shared_ptr<Block> body)
    : name(name), formals(formals), return_type(return_type), body(body) {}

void Method::accept(Visitor* visitor) const {
    visitor->visit(this);
}

Formal::Formal(const std::string& name, const std::string& type)
    : name(name), type(type) {}

void Formal::accept(Visitor* visitor) const {
    visitor->visit(this);
}

Block::Block(std::vector<std::shared_ptr<Expression>> expressions)
    : expressions(expressions) {}

void Block::accept(Visitor* visitor) const {
    visitor->visit(this);
}

BinaryOp::BinaryOp(const std::string& op, std::shared_ptr<Expression> left, std::shared_ptr<Expression> right)
    : op(op), left(left), right(right) {
    std::cerr << "DEBUG: BinaryOp constructor: op=" << op << std::endl;
    if (!left) std::cerr << "WARNING: left is null in BinaryOp constructor" << std::endl;
    if (!right) std::cerr << "WARNING: right is null in BinaryOp constructor" << std::endl;
}

void BinaryOp::accept(Visitor* visitor) const {
    visitor->visit(this);
}

UnaryOp::UnaryOp(const std::string& op, std::shared_ptr<Expression> expr)
    : op(op), expr(expr) {}

void UnaryOp::accept(Visitor* visitor) const {
    visitor->visit(this);
}

Call::Call(std::shared_ptr<Expression> object, const std::string& method_name, 
           std::vector<std::shared_ptr<Expression>> arguments)
    : object(object), method_name(method_name), arguments(arguments) {}

void Call::accept(Visitor* visitor) const {
    visitor->visit(this);
}

New::New(const std::string& type_name)
    : type_name(type_name) {}

void New::accept(Visitor* visitor) const {
    visitor->visit(this);
}

Let::Let(const std::string& name, const std::string& type, 
          std::shared_ptr<Expression> init_expr, std::shared_ptr<Expression> scope_expr)
    : name(name), type(type), init_expr(init_expr), scope_expr(scope_expr) {}

Let::Let(const std::string& name, const std::string& type, std::shared_ptr<Expression> scope_expr)
    : name(name), type(type), init_expr(nullptr), scope_expr(scope_expr) {}

void Let::accept(Visitor* visitor) const {
    visitor->visit(this);
}

If::If(std::shared_ptr<Expression> condition, std::shared_ptr<Expression> then_expr, 
       std::shared_ptr<Expression> else_expr)
    : condition(condition), then_expr(then_expr), else_expr(else_expr) {}

void If::accept(Visitor* visitor) const {
    visitor->visit(this);
}

While::While(std::shared_ptr<Expression> condition, std::shared_ptr<Expression> body)
    : condition(condition), body(body) {}

void While::accept(Visitor* visitor) const {
    visitor->visit(this);
}

Assign::Assign(const std::string& name, std::shared_ptr<Expression> expr)
    : name(name), expr(expr) {}

void Assign::accept(Visitor* visitor) const {
    visitor->visit(this);
}

StringLiteral::StringLiteral(const std::string& value)
    : value(value) {}

void StringLiteral::accept(Visitor* visitor) const {
    visitor->visit(this);
}

IntegerLiteral::IntegerLiteral(int value)
    : value(value) {}

void IntegerLiteral::accept(Visitor* visitor) const {
    visitor->visit(this);
}

BooleanLiteral::BooleanLiteral(bool value)
    : value(value) {}

void BooleanLiteral::accept(Visitor* visitor) const {
    visitor->visit(this);
}

void UnitLiteral::accept(Visitor* visitor) const {
    visitor->visit(this);
}

Identifier::Identifier(const std::string& name)
    : name(name) {}

void Identifier::accept(Visitor* visitor) const {
    visitor->visit(this);
}

void Self::accept(Visitor* visitor) const {
    visitor->visit(this);
}

} // namespace VSOP