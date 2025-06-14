%skeleton "lalr1.cc" // -*- C++ -*-
%language "c++"
%require "3.7.5"
%locations
%defines

// Put parser inside a namespace
%define api.namespace {VSOP}

// Give the name of the parser class
%define api.parser.class {Parser}

// Force the token kind enum (used by the lexer) and the symbol kind enum
// (used by the parser) to use the same value for the tokens.
// (e.g. '+' will be represented by the same integer value in both enums.)
%define api.token.raw

// Tokens contain their type, value and location
// Also allow to use the make_TOKEN functions
%define api.token.constructor

// Allow to use C++ objects as semantic values
%define api.value.type variant

// Add some assertions.
%define parse.assert

// C++ code put inside header file
%code requires {
    #include <string>
    #include <vector>
    #include <memory>
    #include "AST.hpp"
    
    namespace VSOP
    {
        class Driver;
    }
}

// Add an argument to the parser constructor
%parse-param {VSOP::Driver &driver}

%code {
    #include "driver.hpp"
    using namespace std;
    using namespace VSOP;
}

// Token and symbols definitions
%token 
    LOWERCASE_LETTER "lowercase-letter"
    UPPERCASE_LETTER "uppercase-letter"
    LETTER "letter"
    BIN_DIGIT "bin-digit"
    DIGIT "digit"
    HEX_DIGIT "hex-digit"
    WHITESPACE "whitespace"
    TAB "tab"
    LF "lf"
    FF "ff"
    CR "cr"
    AND "and"
    BOOL "bool"
    CLASS "class"
    DO "do"
    ELSE "else"
    EXTENDS "extends"
    FALSE "false"
    IF "if"
    IN "in"
    INT32 "int32"
    ISNULL "isnull"
    LET "let"
    NEW "new"
    NOT "not"
    SELF "self"
    STRING "string"
    THEN "then"
    TRUE "true"
    UNIT "unit"
    WHILE "while"
    REGULAR_CHAR "regular-char"
    ESCAPED_CHAR "escaped-char"
    ESCAPE_SEQUENCE "escape-sequence"
    LBRACE "{"
    RBRACE "}"
    LPAR "("
    RPAR ")"
    COLON ":"
    SEMICOLON ";"
    COMMA ","
    PLUS "+"
    MINUS "-"
    TIMES "*"
    DIV "/"
    POW "^"
    DOT "."
    EQUAL "="
    LOWER "<"
    LOWER_EQUAL "<="
    ASSIGN "<-"
;

// For some symbols, need to store a value
%token <int> INTEGER_LITERAL "integer-literal"
%token <std::string> STRING_LITERAL "string-literal"
%token <std::string> TYPE_IDENTIFIER "type-identifier"
%token <std::string> OBJECT_IDENTIFIER "object-identifier"

// Non-terminals with semantic values
%type <std::shared_ptr<Class>> class class_extends
%type <std::shared_ptr<Field>> field
%type <std::shared_ptr<Method>> method
%type <std::shared_ptr<Formal>> formal
%type <std::vector<std::shared_ptr<Formal>>> formal_list formals
%type <std::shared_ptr<Expression>> expr
%type <std::vector<std::shared_ptr<Expression>>> expr_list args
%type <std::shared_ptr<Block>> block
%type <std::string> type

// Precedence and associativity according to VSOP language spec
// From lowest to highest precedence
%right "<-"          // Precedence 9, right-associative
%left "and"          // Precedence 8, left-associative
%right "not"         // Precedence 7, right-associative
%nonassoc "=" "<" "<="  // Precedence 6, non-associative
%left "+" "-"        // Precedence 5, left-associative
%left "*" "/"        // Precedence 4, left-associative
%right UMINUS        // Precedence 3, right-associative (unary minus)
%right "isnull"      // Precedence 3, right-associative
%right "^"           // Precedence 2, right-associative
%left "."            // Precedence 1, left-associative (highest)

%%
// Grammar rules
%start program;

program:
    class_list { }
;

class_list:
    /* empty */
  | class_list class { 
        driver.add_class($2);
    }
;

class:
    "class" TYPE_IDENTIFIER class_extends 
    {
        auto cls = $3;
        
        if (!cls) {
            std::cerr << "ERROR: Class object from class_extends is null!" << std::endl;
            cls = std::make_shared<Class>($2, "Object"); // Create a new one as fallback
        } else {
            cls->name = $2;
        }
        
        // Set current_class BEFORE parsing class_body
        driver.current_class = cls;
    } 
    class_body 
    {
        // Now set the final semantic value but DON'T add it to program
        $$ = driver.current_class;
    }
;

class_extends:
    /* empty */ {
        $$ = std::make_shared<Class>("placeholder", "Object");
    }
  | "extends" TYPE_IDENTIFIER {
        $$ = std::make_shared<Class>("placeholder", $2);
    }
;

class_body:
    "{" class_content "}" { }
  | "{" "}" { }
;

class_content:
    field_or_method
  | class_content field_or_method
;

field_or_method:
    field {
        if (!driver.current_class) {
            std::cerr << "ERROR: current_class is null when adding field!" << std::endl;
        } else {
            driver.current_class->fields.push_back($1);
        }
    }
  | method {
        if (!driver.current_class) {
            std::cerr << "ERROR: current_class is null when adding method!" << std::endl;
        } else {
            driver.current_class->methods.push_back($1);
        }
    }
;

field:
    OBJECT_IDENTIFIER ":" type ";" {
        $$ = std::make_shared<Field>($1, $3);
    }
  | OBJECT_IDENTIFIER ":" type "<-" expr ";" {
        $$ = std::make_shared<Field>($1, $3, $5);
    }
;

method:
    OBJECT_IDENTIFIER "(" formals ")" ":" type block {
        $$ = std::make_shared<Method>($1, $3, $6, $7);
        if (!$$) std::cerr << "WARNING: Method creation failed" << std::endl;
        if (!$7) std::cerr << "WARNING: Method body (block) is null" << std::endl;
    }
;

formals:
    /* empty */ {
        $$ = std::vector<std::shared_ptr<Formal>>();
    }
  | formal_list {
        $$ = $1;
    }
;

formal_list:
    formal {
        $$ = std::vector<std::shared_ptr<Formal>>();
        $$.push_back($1);
    }
  | formal_list "," formal {
        $$ = $1;
        $$.push_back($3);
    }
;

formal:
    OBJECT_IDENTIFIER ":" type {
        $$ = std::make_shared<Formal>($1, $3);
    }
;

type:
    TYPE_IDENTIFIER {
        $$ = $1;
    }
  | "int32" {
        $$ = "int32";
    }
  | "bool" {
        $$ = "bool";
    }
  | "string" {
        $$ = "string";
    }
  | "unit" {
        $$ = "unit";
    }
;

block:
    "{" expr_list "}" {
        $$ = std::make_shared<Block>($2);
        if (!$$) std::cerr << "WARNING: Block creation failed" << std::endl;
    }
;

expr_list:
    expr {
        $$ = std::vector<std::shared_ptr<Expression>>();
        $$.push_back($1);
    }
  | expr_list ";" expr {
        $$ = $1;
        $$.push_back($3);
    }
;

expr:
    "if" expr "then" expr "else" expr {
        $$ = std::make_shared<If>($2, $4, $6);
    }
  | "if" expr "then" expr {
        $$ = std::make_shared<If>($2, $4);
    }
  | "while" expr "do" expr {
        $$ = std::make_shared<While>($2, $4);
    }
  | "let" OBJECT_IDENTIFIER ":" type "<-" expr "in" expr {
        $$ = std::make_shared<Let>($2, $4, $6, $8);
    }
  | "let" OBJECT_IDENTIFIER ":" type "in" expr {
        $$ = std::make_shared<Let>($2, $4, $6);
    }
  | OBJECT_IDENTIFIER "<-" expr {
        $$ = std::make_shared<Assign>($1, $3);
    }
  | "not" expr %prec "not" {
        $$ = std::make_shared<UnaryOp>("not", $2);
        if (!$$) std::cerr << "WARNING: Failed to create UnaryOp node" << std::endl;
    }
  | "-" expr %prec UMINUS {
        $$ = std::make_shared<UnaryOp>("-", $2);
    }
  | "isnull" expr %prec "isnull" {
        $$ = std::make_shared<UnaryOp>("isnull", $2);
    }
  | expr "=" expr {
        $$ = std::make_shared<BinaryOp>("=", $1, $3);
    }
  | expr "<" expr {
        $$ = std::make_shared<BinaryOp>("<", $1, $3);
    }
  | expr "<=" expr {
        $$ = std::make_shared<BinaryOp>("<=", $1, $3);
    }
  | expr "+" expr {
        $$ = std::make_shared<BinaryOp>("+", $1, $3);
    }
  | expr "-" expr {
        $$ = std::make_shared<BinaryOp>("-", $1, $3);
    }
  | expr "*" expr {
        $$ = std::make_shared<BinaryOp>("*", $1, $3);
    }
  | expr "/" expr {
        $$ = std::make_shared<BinaryOp>("/", $1, $3);
    }
  | expr "^" expr {
        $$ = std::make_shared<BinaryOp>("^", $1, $3);
    }
  | expr "and" expr {
        $$ = std::make_shared<BinaryOp>("and", $1, $3);
    }
  | OBJECT_IDENTIFIER "(" args ")" {
        auto self = std::make_shared<Self>();
        $$ = std::make_shared<Call>(self, $1, $3);
    }
  | expr "." OBJECT_IDENTIFIER "(" args ")" {
        $$ = std::make_shared<Call>($1, $3, $5);
    }
  | "(" expr ")" "." OBJECT_IDENTIFIER "(" args ")" {
        // Handle expressions like (new Cons).init(...)
        $$ = std::make_shared<Call>($2, $5, $7);
    }
  | "new" TYPE_IDENTIFIER {
        $$ = std::make_shared<New>($2);
    }
  | OBJECT_IDENTIFIER {
        $$ = std::make_shared<Identifier>($1);
    }
  | "self" {
        $$ = std::make_shared<Self>();
    }
  | INTEGER_LITERAL {
        $$ = std::make_shared<IntegerLiteral>($1);
    }
  | STRING_LITERAL {
        $$ = std::make_shared<StringLiteral>($1);
    }
  | "true" {
        $$ = std::make_shared<BooleanLiteral>(true);
    }
  | "false" {
        $$ = std::make_shared<BooleanLiteral>(false);
    }
  | "(" ")" {
        $$ = std::make_shared<UnitLiteral>();
    }
  | "(" expr ")" {
        $$ = $2;
    }
  | block {
        $$ = $1;
    }
;

args:
    /* empty */ {
        $$ = std::vector<std::shared_ptr<Expression>>();
    }
  | expr_list {
        $$ = $1;
    }
;

%%

// User code
void VSOP::Parser::error(const location_type& l, const std::string& m)
{
    const position &pos = l.begin;
    cerr << *(pos.filename) << ":"
         << pos.line << ":" 
         << pos.column << ": "
         << m
         << endl;
}