#include <iostream>
#include <string>
#include <map>
#include <cstring>
#include <cerrno>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <filesystem>

#include "driver.hpp"
#include "utils.hpp"
#include "PrettyPrinter.hpp"
#include "SemanticChecker.hpp"
#include "CodeGenerator.hpp"

using namespace std;
using namespace VSOP;

// External variable from lexer.lex
extern location loc;

// Adapted from https://www.gnu.org/software/bison/manual/html_node/A-Complete-C_002b_002b-Example.html
enum class Mode
{
    LEX,
    PARSE,
    CHECK,
    IR,
    EXEC
};

static const map<string, Mode> flag_to_mode = {
    {"-l", Mode::LEX},
    {"-p", Mode::PARSE},
    {"-c", Mode::CHECK},
    {"-i", Mode::IR}
};

void segfault_handler(int sig) {
    std::cerr << "SEGMENTATION FAULT DETECTED! " << "Index: " << sig << std::endl;
    
    void* array[20];
    size_t size = backtrace(array, 20);
    std::cerr << "Backtrace:" << std::endl;
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    
    exit(1);
}

int main(int argc, char const *argv[])
{
    signal(SIGSEGV, segfault_handler);
    Mode mode;
    string source_file;
    bool extended_mode = false;
    
    // Parse command line arguments
    int arg_index = 1;
    
    // Check for -e flag
    if (arg_index < argc && string(argv[arg_index]) == "-e") {
        extended_mode = true;
        arg_index++;
    }
    
    // Check for mode flag
    if (arg_index < argc && flag_to_mode.count(argv[arg_index]) > 0) {
        mode = flag_to_mode.at(argv[arg_index]);
        arg_index++;
    } else {
        mode = Mode::EXEC;  // Default mode is generate executable
    }
    
    // Ensure source file is provided
    if (arg_index < argc) {
        source_file = argv[arg_index];
    } else {
        cerr << "Usage: " << argv[0] << " [-e] [-l|-p|-c|-i] <source_file>" << endl;
        return -1;
    }

    VSOP::Driver driver = VSOP::Driver(source_file);
    int res;
    
    switch (mode)
    {
    case Mode::LEX:
        res = driver.lex();
        driver.print_tokens();
        return res;
        
    case Mode::PARSE:
        res = driver.parse();
        if (res == 0)
            driver.print_ast();
        return res;
        
    case Mode::CHECK:
        res = driver.check();
        if (res == 0)
            driver.print_typed_ast();
        return res;

    case Mode::IR:
        {  // Add a scope with braces around this case
            // First check semantics
            res = driver.check();
            if (res != 0) return res;
            
            // Then generate IR
            VSOP::CodeGenerator codegen(source_file, true);
            if (!codegen.generate(driver.program)) {
                // Print errors
                const auto& errors = codegen.getErrors();
                for (const auto& error : errors) {
                    cerr << error << endl;
                }
                return 1;
            }
            
            // Dump IR to stdout
            codegen.dumpIR();
            return 0;
        }  // Close the scope
        
    case Mode::EXEC:
        {  // Add a scope with braces around this case
            // First check semantics
            res = driver.check();
            if (res != 0) return res;
            
            // Then generate executable
            VSOP::CodeGenerator codegen_exec(source_file, false);
            if (!codegen_exec.generate(driver.program)) {
                // Print errors
                const auto& errors = codegen_exec.getErrors();
                for (const auto& error : errors) {
                    cerr << error << endl;
                }
                return 1;
            }
            
            // Generate executable
            std::string output_file = source_file;
            // Remove any directory part
            size_t last_slash = output_file.find_last_of("/\\");
            if (last_slash != std::string::npos) {
                output_file = output_file.substr(last_slash + 1);
            }
            // Remove extension
            size_t last_dot = output_file.find_last_of(".");
            if (last_dot != std::string::npos) {
                output_file = output_file.substr(0, last_dot);
            }
            
            if (!codegen_exec.generateExecutable(output_file)) {
                // Print errors
                const auto& errors = codegen_exec.getErrors();
                for (const auto& error : errors) {
                    cerr << error << endl;
                }
                return 1;
            }
            
            return 0;
        }  // Close the scope
    }

    return 0;
}