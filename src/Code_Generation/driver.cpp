#include <iostream>
#include <string>
#include <map>
#include <cstring> // For strerror
#include <cerrno>  // For errno
 
#include "driver.hpp"
#include "utils.hpp"
#include "PrettyPrinter.hpp"
#include "SemanticChecker.hpp"
#include "CodeGenerator.hpp"
 
using namespace std;
using namespace VSOP;
 
// External variable from lexer.lex
extern location loc;
 
// Constructor implementation (moved from header)
Driver::Driver(const std::string &_source_file) 
    : program(nullptr), current_class(nullptr), source_file(_source_file) {}
 
/**
 * @brief Map a token type to a string.
 */
static const map<Parser::token_type, string> type_to_string = {
    // {Parser::token::LOWERCASE_LETTER, "lowercase-letter"},
    // {Parser::token::UPPERCASE_LETTER, "uppercase-letter"},
    // {Parser::token::LETTER, "letter"},
    // {Parser::token::BIN_DIGIT, "bin-digit"},
    // {Parser::token::DIGIT, "digit"},
    // {Parser::token::HEX_DIGIT, "hex-digit"},

    // {Parser::token::WHITESPACE, "whitespace"},
    // {Parser::token::TAB, "tab"},
    // {Parser::token::LF, "lf"},
    // {Parser::token::FF, "ff"},
    // {Parser::token::CR, "cr"},

    {Parser::token::INTEGER_LITERAL, "integer-literal"},

    {Parser::token::AND, "and"},
    {Parser::token::BOOL, "bool"},
    {Parser::token::CLASS, "class"},
    {Parser::token::DO, "do"},
    {Parser::token::ELSE, "else"},
    {Parser::token::EXTENDS, "extends"},
    {Parser::token::FALSE, "false"},
    {Parser::token::IF, "if"},
    {Parser::token::IN, "in"},
    {Parser::token::INT32, "int32"},
    {Parser::token::ISNULL, "isnull"},
    {Parser::token::LET, "let"},
    {Parser::token::NEW, "new"},
    {Parser::token::NOT, "not"},
    {Parser::token::SELF, "self"},
    {Parser::token::STRING, "string"},
    {Parser::token::THEN, "then"},
    {Parser::token::TRUE, "true"},
    {Parser::token::UNIT, "unit"},
    {Parser::token::WHILE, "while"},

    {Parser::token::TYPE_IDENTIFIER, "type-identifier"},
    {Parser::token::OBJECT_IDENTIFIER, "object-identifier"},

    {Parser::token::STRING_LITERAL, "string-literal"},
    // {Parser::token::REGULAR_CHAR, "regular-char"},
    // {Parser::token::ESCAPED_CHAR, "escaped-char"},
    // {Parser::token::ESCAPE_SEQUENCE, "escape-sequence"},
    
    {Parser::token::LBRACE, "lbrace"},
    {Parser::token::RBRACE, "rbrace"},
    {Parser::token::LPAR, "lpar"},
    {Parser::token::RPAR, "rpar"},
    {Parser::token::COLON, "colon"},
    {Parser::token::SEMICOLON, "semicolon"},
    {Parser::token::COMMA, "comma"},
    {Parser::token::PLUS, "plus"},
    {Parser::token::MINUS, "minus"},
    {Parser::token::TIMES, "times"},
    {Parser::token::DIV, "div"},
    {Parser::token::POW, "pow"},
    {Parser::token::DOT, "dot"},
    {Parser::token::EQUAL, "equal"},
    {Parser::token::LOWER, "lower"},
    {Parser::token::LOWER_EQUAL, "lower-equal"},
    {Parser::token::ASSIGN, "assign"},
};

/**
 * @brief Print the information about a token
 *
 * @param token the token
 */
static void print_token(Parser::symbol_type token)
{
    position pos = token.location.begin;
    Parser::token_type type = (Parser::token_type)token.type_get();

    cout << pos.line << ","
         << pos.column << ","
         << type_to_string.at(type);

    switch (type)
    {
    case Parser::token::INTEGER_LITERAL:
    {
        int value = token.value.as<int>();
        cout << "," << value;
        break;
    }

    case Parser::token::TYPE_IDENTIFIER:
    case Parser::token::OBJECT_IDENTIFIER:
    {
        string id = token.value.as<string>();
        cout << "," << id;
        break;
    }

    case Parser::token::STRING_LITERAL:
    {
        string str = token.value.as<string>();
        cout << "," << "\"" << str << "\"";
        break;
    }

    default:
        break;
    }

    cout << endl;
}

int Driver::lex()
{
    try {
        scan_begin();
    
        int error = 0;
    
        while (true)
        {
            Parser::symbol_type token = yylex();
    
            if ((Parser::token_type)token.type_get() == Parser::token::YYEOF)
                break;
    
            if ((Parser::token_type)token.type_get() != Parser::token::YYerror){
                    tokens.push_back(reinterpret_cast<void*>(new Parser::symbol_type(std::move(token))));
                }
    
            else
                error = 1;
        }
    
        scan_end();
    
        return error;
    } catch (const std::exception& e) {
        cerr << "Exception during lexing: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "Unknown exception during lexing" << endl;
        return 1;
    }
}

int Driver::parse()
{
    try {
        // Initialize the program
        program = std::make_shared<Program>();
        
        scan_begin();
        
        parser = new Parser(*this);
        
        int res = parser->parse();
        scan_end();
        
        delete parser;
        
        return res;
    } catch (const std::exception& e) {
        cerr << "Exception during parsing: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "Unknown exception during parsing" << endl;
        return 1;
    }
}

int Driver::check()
{
    try {
        // First, parse the program
        int parse_result = parse();
        if (parse_result != 0) {
            return parse_result;  // Parsing failed
        }
        
        // Then run semantic analysis
        SemanticChecker checker(source_file);
        if (!checker.check(program)) {
            // Print semantic errors
            const auto& errors = checker.getErrors();
            for (const auto& error : errors) {
                cerr << error << endl;
            }
            return 1;  // Semantic errors detected
        }
        
        return 0;  // Success
        
    } catch (const std::exception& e) {
        cerr << "Exception during semantic checking: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "Unknown exception during semantic checking" << endl;
        return 1;
    }
}

int Driver::generate_ir(std::ostream& output)
{
    try {
        // First check the program
        int check_result = check();
        if (check_result != 0) {
            return check_result;  // Semantic errors detected
        }
        
        // Generate LLVM IR
        CodeGenerator generator(source_file);
        if (!generator.generate(program, true)) {
            // Print code generation errors
            const auto& errors = generator.getErrors();
            for (const auto& error : errors) {
                cerr << error << endl;
            }
            return 1;  // Code generation errors detected
        }
        
        // Output the IR
        generator.dumpIR(output);
        
        return 0;  // Success
        
    } catch (const std::exception& e) {
        cerr << "Exception during code generation: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "Unknown exception during code generation" << endl;
        return 1;
    }
}

int Driver::generate_executable(const std::string& output_file)
{
    try {
        // First check the program
        int check_result = check();
        if (check_result != 0) {
            return check_result;  // Semantic errors detected
        }
        
        // Generate native executable
        CodeGenerator generator(source_file);
        if (!generator.generate(program, true)) {
            // Print code generation errors
            const auto& errors = generator.getErrors();
            for (const auto& error : errors) {
                cerr << error << endl;
            }
            return 1;  // Code generation errors detected
        }
        
        // Write executable
        if (!generator.writeNativeExecutable(output_file)) {
            cerr << "Failed to write executable: " << output_file << endl;
            return 1;
        }
        
        return 0;  // Success
        
    } catch (const std::exception& e) {
        cerr << "Exception during executable generation: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "Unknown exception during executable generation" << endl;
        return 1;
    }
}

void Driver::print_tokens()
{
    try {
        for (auto token_ptr : tokens) {
            Parser::symbol_type* token = reinterpret_cast<Parser::symbol_type*>(token_ptr);
            print_token(*token);
        }
    } catch (const std::exception& e) {
        cerr << "Exception during token printing: " << e.what() << endl;
    } catch (...) {
        cerr << "Unknown exception during token printing" << endl;
    }
}

void Driver::print_ast()
{
    try {
        if (program) {
            PrettyPrinter printer(std::cout);
            printer.print(program.get());
        } else {
            cerr << "ERROR: Program is null" << endl;
            std::cout << "[]" << std::endl;
        }
    } catch (const std::exception& e) {
        cerr << "Exception during AST printing: " << e.what() << endl;
    } catch (...) {
        cerr << "Unknown exception during AST printing" << endl;
    }
}

void Driver::print_typed_ast()
{
    try {
        if (program) {
            SemanticChecker checker(source_file);
            checker.check(program);
            checker.printTypedAST(std::cout);
        } else {
            cerr << "ERROR: Program is null" << endl;
            std::cout << "[]" << std::endl;
        }
    } catch (const std::exception& e) {
        cerr << "Exception during typed AST printing: " << e.what() << endl;
    } catch (...) {
        cerr << "Unknown exception during typed AST printing" << endl;
    }
}

void Driver::add_class(std::shared_ptr<Class> cls) {
    try {
        if (!cls) {
            cerr << "ERROR: Attempted to add a null class" << endl;
            return;
        }
        
        if (!program) {
            cerr << "ERROR: Program is null when adding class" << endl;
            program = std::make_shared<Program>();
        }
        
        program->classes.push_back(cls);
        current_class = cls;
        
    } catch (const std::exception& e) {
        cerr << "Exception when adding class: " << e.what() << endl;
    } catch (...) {
        cerr << "Unknown exception when adding class" << endl;
    }
}

//  void Driver::scan_begin()
//  {
//      loc.initialize(&source_file);

//      if (source_file.empty() || source_file == "-")
//          yyin = stdin;
//      else if (!(yyin = fopen(source_file.c_str(), "r")))
//      {
//          cerr << "cannot open " << source_file << ": " << strerror(errno) << '\n';
//          exit(EXIT_FAILURE);
//      }
//  }

//  void Driver::scan_end()
//  {
//      fclose(yyin);
//  }