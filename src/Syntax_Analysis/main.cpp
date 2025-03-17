#include <iostream>
#include <string>
#include "driver.hpp"

#include <signal.h>
#include <execinfo.h>
#include <unistd.h> 

using namespace std;

// Adapted from https://www.gnu.org/software/bison/manual/html_node/A-Complete-C_002b_002b-Example.html
enum class Mode
{
    LEX,
    PARSE
};

static const map<string, Mode> flag_to_mode = {
    {"-l", Mode::LEX},
    {"-p", Mode::PARSE},
};

void segfault_handler(int sig) {
    std::cerr << "SEGMENTATION FAULT DETECTED!" << std::endl;
    
    // Get backtrace (limited functionality, but better than nothing)
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
        mode = Mode::PARSE;
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
        cerr << "Usage: " << argv[0] << " [-l|-p] <source_file>" << endl;
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
    }
    
    return 0;
}