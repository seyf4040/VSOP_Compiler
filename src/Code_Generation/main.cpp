#include <iostream>
#include <string>
#include <map>
#include <cstring>
#include <cerrno>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>
#include <cstdlib>
#include <filesystem>
#include <fstream>

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
    LLVM_IR,
    EXECUTABLE
};

static const map<string, Mode> flag_to_mode = {
    {"-l", Mode::LEX},
    {"-p", Mode::PARSE},
    {"-c", Mode::CHECK},
    {"-i", Mode::LLVM_IR}
};

void segfault_handler(int sig) {
    std::cerr << "SEGMENTATION FAULT DETECTED! " << "Index: " << sig << std::endl;
    
    // Get backtrace (limited functionality, but better than nothing)
    void* array[20];
    size_t size = backtrace(array, 20);
    std::cerr << "Backtrace:" << std::endl;
    backtrace_symbols_fd(array, size, STDERR_FILENO);
    
    exit(1);
}

// Execute a system command and return its exit code
int execute_command(const string& command) {
    return system(command.c_str());
}

int main(int argc, char const *argv[])
{
    signal(SIGSEGV, segfault_handler);
    Mode mode = Mode::EXECUTABLE;  // Default mode is native executable generation
    string source_file;
    bool extended_mode = false;
    
    // Parse arguments
    int arg_index = 1;
    while (arg_index < argc) {
        string arg = argv[arg_index];
        
        if (arg == "-e") {
            extended_mode = true;
            arg_index++;
            continue;
        }
        
        if (flag_to_mode.count(arg) > 0) {
            mode = flag_to_mode.at(arg);
            arg_index++;
            if (arg_index >= argc) {
                cerr << "Missing source file after " << arg << endl;
                return -1;
            }
            source_file = argv[arg_index];
            arg_index++;
        } else {
            // Assume this is the source file
            source_file = arg;
            arg_index++;
        }
    }
    
    if (source_file.empty()) {
        cerr << "Usage: " << argv[0] << " [-l|-p|-c|-i] [-e] <source_file>" << endl;
        return -1;
    }
    
    VSOP::Driver driver = VSOP::Driver(source_file);
    int res;
    
    try {
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
            
        case Mode::LLVM_IR:
            // First check the program for errors
            res = driver.check();
            if (res != 0) {
                return res;  // Return if there are semantic errors
            }
            
            {
                // Generate LLVM IR
                CodeGenerator generator(source_file);
                if (generator.generate(driver.program, true)) {
                    // Print the IR to stdout
                    generator.dumpIR(std::cout);
                    return 0;
                } else {
                    // Print errors
                    for (const auto& error : generator.getErrors()) {
                        cerr << error << endl;
                    }
                    return 1;
                }
            }
            
        case Mode::EXECUTABLE:
            // First check the program for errors
            res = driver.check();
            if (res != 0) {
                return res;  // Return if there are semantic errors
            }
            
            {
                // Get output filename (remove .vsop extension if present)
                std::filesystem::path input_path(source_file);
                std::string output_file = input_path.stem().string();
                
                // Generate temporary IR file
                std::string temp_ir_file = output_file + ".ll";
                std::ofstream ir_file(temp_ir_file);
                if (!ir_file) {
                    cerr << "Failed to create temporary file: " << temp_ir_file << endl;
                    return 1;
                }
                
                // Generate LLVM IR
                CodeGenerator generator(source_file);
                if (!generator.generate(driver.program, true)) {
                    // Print errors
                    for (const auto& error : generator.getErrors()) {
                        cerr << error << endl;
                    }
                    return 1;
                }
                
                // Write IR to temporary file
                generator.dumpIR(ir_file);
                ir_file.close();
                
                // Compile IR to object file using clang
                std::string temp_obj_file = output_file + ".o";
                std::string compile_cmd = "clang -c " + temp_ir_file + " -o " + temp_obj_file;
                if (execute_command(compile_cmd) != 0) {
                    cerr << "Failed to compile IR to object file" << endl;
                    return 1;
                }
                
                // Link with runtime library
                std::string runtime_lib = "runtime/runtime/object.o";
                if (!std::filesystem::exists(runtime_lib)) {
                    // Try to compile the runtime if object file doesn't exist
                    std::string compile_runtime_cmd = "clang -c runtime/runtime/object.c -o " + runtime_lib;
                    if (execute_command(compile_runtime_cmd) != 0) {
                        cerr << "Failed to compile runtime library" << endl;
                        return 1;
                    }
                }
                
                // Link object file with runtime
                std::string link_cmd = "clang " + temp_obj_file + " " + runtime_lib + " -o " + output_file;
                if (execute_command(link_cmd) != 0) {
                    cerr << "Failed to link object file with runtime" << endl;
                    return 1;
                }
                
                // Clean up temporary files
                std::filesystem::remove(temp_ir_file);
                std::filesystem::remove(temp_obj_file);
                
                cout << "Generated executable: " << output_file << endl;
                return 0;
            }
        }
    } catch (const std::exception& e) {
        cerr << "Exception: " << e.what() << endl;
        return 1;
    } catch (...) {
        cerr << "Unknown exception" << endl;
        return 1;
    }
    
    return 0;
}