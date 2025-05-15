#ifndef AST_HPP
#define AST_HPP

#include <string>
#include <vector>
#include <memory>
#include <iostream>

namespace VSOP {

// Forward declarations
class Class;
class Field;
class Method;
class Formal;
class Expression;
class BinaryOp;
class UnaryOp;
class Call;
class New;
class Let;
class If;
class While;
class Assign;
class Literal;
class StringLiteral;
class IntegerLiteral;
class BooleanLiteral;
class UnitLiteral;
class Identifier;
class Block;
class Self;

// Visitor interface
class Visitor {
public:
    virtual ~Visitor() = default;
    
    // Program and class elements
    virtual void visit(const Class* node) = 0;
    virtual void visit(const Field* node) = 0;
    virtual void visit(const Method* node) = 0;
    virtual void visit(const Formal* node) = 0;
    
    // Expressions
    virtual void visit(const BinaryOp* node) = 0;
    virtual void visit(const UnaryOp* node) = 0;
    virtual void visit(const Call* node) = 0;
    virtual void visit(const New* node) = 0;
    virtual void visit(const Let* node) = 0;
    virtual void visit(const If* node) = 0;
    virtual void visit(const While* node) = 0;
    virtual void visit(const Assign* node) = 0;
    
    // Literals and identifiers
    virtual void visit(const StringLiteral* node) = 0;
    virtual void visit(const IntegerLiteral* node) = 0;
    virtual void visit(const BooleanLiteral* node) = 0;
    virtual void visit(const UnitLiteral* node) = 0;
    virtual void visit(const Identifier* node) = 0;
    virtual void visit(const Self* node) = 0;
    
    // Other
    virtual void visit(const Block* node) = 0;
};

// Base node class
class ASTNode {
public:
    virtual ~ASTNode() = default;
    virtual void accept(Visitor* visitor) const = 0;
};

// Program class - Contains a list of classes
class Program : public ASTNode {
public:
    std::vector<std::shared_ptr<Class>> classes;
    
    Program() = default;
    void accept(Visitor* visitor) const override;
};

// Class node
class Class : public ASTNode {
public:
    std::string name;
    std::string parent;
    std::vector<std::shared_ptr<Field>> fields;
    std::vector<std::shared_ptr<Method>> methods;
    
    Class(const std::string& name, const std::string& parent = "Object");
    void accept(Visitor* visitor) const override;
};

// Field node
class Field : public ASTNode {
public:
    std::string name;
    std::string type;
    std::shared_ptr<Expression> init_expr;
    
    Field(const std::string& name, const std::string& type, std::shared_ptr<Expression> init_expr = nullptr);
    void accept(Visitor* visitor) const override;
};

// Method node
class Method : public ASTNode {
public:
    std::string name;
    std::vector<std::shared_ptr<Formal>> formals;
    std::string return_type;
    std::shared_ptr<Block> body;
    
    Method(const std::string& name, std::vector<std::shared_ptr<Formal>> formals, 
           const std::string& return_type, std::shared_ptr<Block> body);
    void accept(Visitor* visitor) const override;
};

// Formal parameter node
class Formal : public ASTNode {
public:
    std::string name;
    std::string type;
    
    Formal(const std::string& name, const std::string& type);
    void accept(Visitor* visitor) const override;
};

// Base expression class
class Expression : public ASTNode {
public:
    virtual ~Expression() = default;
};

// Block expression
class Block : public Expression {
public:
    std::vector<std::shared_ptr<Expression>> expressions;
    
    Block() = default;
    Block(std::vector<std::shared_ptr<Expression>> expressions);
    void accept(Visitor* visitor) const override;
};

// Binary operation
class BinaryOp : public Expression {
public:
    std::string op;
    std::shared_ptr<Expression> left;
    std::shared_ptr<Expression> right;
    
    BinaryOp(const std::string& op, std::shared_ptr<Expression> left, std::shared_ptr<Expression> right);
    void accept(Visitor* visitor) const override;
};

// Unary operation
class UnaryOp : public Expression {
public:
    std::string op;
    std::shared_ptr<Expression> expr;
    
    UnaryOp(const std::string& op, std::shared_ptr<Expression> expr);
    void accept(Visitor* visitor) const override;
};

// Method call
class Call : public Expression {
public:
    std::shared_ptr<Expression> object;
    std::string method_name;
    std::vector<std::shared_ptr<Expression>> arguments;
    
    Call(std::shared_ptr<Expression> object, const std::string& method_name, 
         std::vector<std::shared_ptr<Expression>> arguments);
    void accept(Visitor* visitor) const override;
};

// New object creation
class New : public Expression {
public:
    std::string type_name;
    
    New(const std::string& type_name);
    void accept(Visitor* visitor) const override;
};

// Let expression
class Let : public Expression {
public:
    std::string name;
    std::string type;
    std::shared_ptr<Expression> init_expr;
    std::shared_ptr<Expression> scope_expr;
    
    Let(const std::string& name, const std::string& type, 
        std::shared_ptr<Expression> init_expr, std::shared_ptr<Expression> scope_expr);
    Let(const std::string& name, const std::string& type, std::shared_ptr<Expression> scope_expr);
    void accept(Visitor* visitor) const override;
};

// If expression
class If : public Expression {
public:
    std::shared_ptr<Expression> condition;
    std::shared_ptr<Expression> then_expr;
    std::shared_ptr<Expression> else_expr;
    
    If(std::shared_ptr<Expression> condition, std::shared_ptr<Expression> then_expr, 
       std::shared_ptr<Expression> else_expr = nullptr);
    void accept(Visitor* visitor) const override;
};

// While expression
class While : public Expression {
public:
    std::shared_ptr<Expression> condition;
    std::shared_ptr<Expression> body;
    
    While(std::shared_ptr<Expression> condition, std::shared_ptr<Expression> body);
    void accept(Visitor* visitor) const override;
};

// Assignment
class Assign : public Expression {
public:
    std::string name;
    std::shared_ptr<Expression> expr;
    
    Assign(const std::string& name, std::shared_ptr<Expression> expr);
    void accept(Visitor* visitor) const override;
};

// Base literal class
class Literal : public Expression {
public:
    virtual ~Literal() = default;
};

// String literal
class StringLiteral : public Literal {
public:
    std::string value;
    
    StringLiteral(const std::string& value);
    void accept(Visitor* visitor) const override;
};

// Integer literal
class IntegerLiteral : public Literal {
public:
    int value;
    
    IntegerLiteral(int value);
    void accept(Visitor* visitor) const override;
};

// Boolean literal
class BooleanLiteral : public Literal {
public:
    bool value;
    
    BooleanLiteral(bool value);
    void accept(Visitor* visitor) const override;
};

// Unit literal
class UnitLiteral : public Literal {
public:
    UnitLiteral() = default;
    void accept(Visitor* visitor) const override;
};

// Identifier
class Identifier : public Expression {
public:
    std::string name;
    
    Identifier(const std::string& name);
    void accept(Visitor* visitor) const override;
};

// Self keyword
class Self : public Expression {
public:
    Self() = default;
    void accept(Visitor* visitor) const override;
};

} // namespace VSOP

#endif // AST_HPP