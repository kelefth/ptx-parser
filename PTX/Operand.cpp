#include <iomanip>

#include "Operand.h"

Operand::Operand(std::string value, OperandType type) : Value(value), Type(type) {}
Operand::Operand(double value, OperandType type) : Value(value), Type(type) {}
Operand::Operand(AddressExpr value, OperandType type) : Value(std::in_place_type<AddressExpr>, value), Type(type) {}
Operand::Operand(std::string value, OperandType type, std::string dim) : Value(value), Type(type), Dimension(dim) {}

Operand::Operand(const Operand& operand) {
    Value = operand.Value;
    Type = operand.Type;
    Dimension = operand.Dimension;
}

std::variant<std::string, double, AddressExpr> Operand::getValue() {
    return Value;
}

void Operand::setValue(std::variant<std::string, double, AddressExpr> value) {
    Value = value;
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

bool Operand::operator==(const Operand& op) const {
    return Value == op.Value  &&
           Type == op.Type    &&
           Dimension == op.Dimension;
}

std::string Operand::ToString() {
    if (const auto strPtr (std::get_if<std::string>(&Value)); strPtr) 
        return *strPtr;
    else if (const auto doublePtr (std::get_if<double>(&Value)); doublePtr) 
        return std::to_string(*doublePtr);
    else if (const auto AddressExprPtr (std::get_if<AddressExpr>(&Value)); AddressExprPtr) 
        return AddressExprPtr->ToString();

    return "";
}