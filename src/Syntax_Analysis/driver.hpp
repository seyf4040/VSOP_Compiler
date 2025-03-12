#ifndef DRIVER_HPP
#define DRIVER_HPP

#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cstdio> // For FILE*
#include "AST.hpp"

// External declaration for yyin
extern FILE* yyin;

// Forward declaration - we'll include the full header later
namespace VSOP {
    // Forward declaration of Parser to be used
    class Parser;
    
    // Forward declaration for location, to be used in Driver
    class location;
}

namespace VSOP
{
    class Driver
    {
    public:
        /**
         * @brief Construct a new Driver.
         *
         * @param source_file The file containing the source code.
         */
        Driver(const std::string &_source_file);
        
        /**
         * @brief Get the source file.
         *
         * @return const std::string& The source file.
         */
        const std::string &get_source_file() { return source_file; }
        
        /**
         * @brief Add a new integer variable.
         *
         * @param name The name of the variable.
         * @param value The value of the variable.
         */
        void add_variable(std::string name, int value) { variables[name] = value; }
        
        /**
         * @brief Check if a variable exists.
         *
         * @param name The name of the variable.
         *
         * @return true If the variable exists.
         * @return false If the variable does not exist.
         */
        bool has_variable(std::string name) { return variables.count(name); }
        
        /**
         * @brief Get the interger value of a variable.
         *
         * @param name The name of the variable.
         *
         * @return int The value of the variable.
         */
        int get_variable(std::string name) { return variables.at(name); }
        
        /**
         * @brief Run the lexer on the source file.
         *
         * @return int 0 if no lexical error.
         */
        int lex();
        
        /**
         * @brief Run the parser on the source file and compute the result.
         *
         * @return int 0 if no syntax or lexical error.
         */
        int parse();
        
        /**
         * @brief Print all the tokens.
         */
        void print_tokens();
        
        /**
         * @brief Print the AST.
         */
        void print_ast();
        
        /**
         * @brief The result of the computation.
         */
        int result;
        
        /**
         * @brief The root of the AST.
         */
        std::shared_ptr<Program> program;
        
        /**
         * @brief The current class being parsed.
         */
        std::shared_ptr<Class> current_class;
        
        /**
         * @brief Add a class to the program.
         */
        void add_class(std::shared_ptr<Class> cls);
        
    private:
        /**
         * @brief The source file.
         */
        std::string source_file;
        
        /**
         * @brief The parser.
         */
        VSOP::Parser *parser;
        
        /**
         * @brief Store the variables (names + values).
         */
        std::map<std::string, int> variables;
        
        /**
         * @brief Store the tokens.
         */
        std::vector<void*> tokens; // Use void* for now, will be cast to the correct type
        
        /**
         * @brief Start the lexer.
         */
        void scan_begin();
        
        /**
         * @brief Stop the lexer.
         */
        void scan_end();
    };
}

// Include parser.hpp after Driver definition
#include "parser.hpp"

// Define YY_DECL after including parser.hpp
#define YY_DECL VSOP::Parser::symbol_type yylex()
YY_DECL;

#endif // DRIVER_HPP