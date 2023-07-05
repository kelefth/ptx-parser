#include <iomanip>

#include "Operand.h"

std::variant<std::string, double> Operand::getValue() {
    return Value;
}

OperandType Operand::getType() {
    return Type;
}

std::string Operand::getDimension() {
    return Dimension;
}

void Operand::setDimension(std::string dim) {
    Dimension = dim;
}

std::string Operand::ToString() {
    if (const auto strPtr (std::get_if<std::string>(&Value)); strPtr) 
        return *strPtr;
    else if (const auto doublePtr (std::get_if<double>(&Value)); doublePtr) 
        return std::to_string(*doublePtr);

    return "";
}