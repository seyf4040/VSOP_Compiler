#include "utils.hpp"

#include <iostream>
#include <sstream>
#include <iomanip>
#include <string>
#include <stdexcept>

std::string charToHex(char ch);


int stringToInt(const std::string& str) {
    // std::cout << str <<  std::endl;
    try {
        size_t idx = 0;
        int base = (str.find("0x") == 0) ? 16 : 10;
        int val = std::stoi(str, &idx, base);

        if (idx != str.length()) {
            return -1;
        }

        return val;

    } catch (const std::exception& e) {
        throw std::invalid_argument("Invalid input string: " + str);
        return -1;
    }
}

std::string escapedToChar(char* escapedSequence) {
    switch(escapedSequence[1]) {
		case 'x': return std::to_string(std::strtol(escapedSequence, NULL, 16));
		case 'b': return charToHex('\b');
		case 't': return charToHex('\t');
		case 'n': return charToHex('\n');
		case 'r': return charToHex('\r');
        case '\\': return charToHex('\\');
        case '\"': return charToHex('\"');
		default: return "";
	}  
}

std::string charToHex(char ch) {
    std::ostringstream oss;
    oss << "\\x" << std::hex << std::setw(2) << std::setfill('0') << (static_cast<int>(ch) & 0xFF);
    return oss.str();
}