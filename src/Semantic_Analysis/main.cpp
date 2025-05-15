#include <iostream>
#include <string>
#include <map>
#include <cstring>
#include <cerrno>
#include <signal.h>
#include <execinfo.h>
#include <unistd.h>

#include "driver.hpp"
#include "utils.hpp"
#include "PrettyPrinter.hpp"
#include "SemanticChecker.hpp"

using namespace std;
using namespace VSOP;

// External variable from lexer.lex
extern location loc;

// Adapted from https://www.gnu.org/software/bison/manual/html_node/A-Complete-C_002b_002b-Example.html
enum class Mode
{
    LEX,
    PARSE,
    CHECK
};

static const map<string, Mode> flag_to_mode = {
    {"-l", Mode::LEX},
    {"-p", Mode::PARSE},
    {"-c", Mode::CHECK}
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
    
    if (argc == 2)
    {
        mode = Mode::CHECK;  // Default mode is semantic checking
        source_file = argv[1];
    }
    else if (argc == 3)
    {
        if (flag_to_mode.count(argv[1]) == 0)
        {
            cerr << "Invalid mode: " << argv[1] << endl;
            return -1;
        }
        mode = flag_to_mode.at(argv[1]);
        source_file = argv[2];
    }
    else
    {
        cerr << "Usage: " << argv[0] << " [-l|-p|-c] <source_file>" << endl;
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
    }
    
    return 0;
}