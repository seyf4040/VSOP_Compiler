#include "utils.hpp"
#include <stdexcept>

int stringToInt(const std::string& str) {
    try {
        size_t idx = 0;
        int base = (str.find("0x") == 0 || str.find("0X") == 0) ? 16 : 10;
        return std::stoi(str, &idx, base);
    } catch (const std::exception& e) {
        throw std::invalid_argument("Invalid input string: " + str);
    }
}
