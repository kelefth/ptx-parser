#ifndef OPERAND_H
#define OPERAND_H

#include <string>
#include <variant>

enum OperandType {
    Register,
    Immediate,
    Address,
    Label
};

class Operand {

    std::variant<std::string, double> Value;
    OperandType Type;
    std::string Dimension;

public:
    Operand(std::string value, OperandType type) : Value(value), Type(type) {}
    Operand(double value, OperandType type) : Value(value), Type(type) {}
    Operand(std::string value, OperandType type, std::string dim) : Value(value), Type(type), Dimension(dim) {}

    std::variant<std::string, double> getValue();
    OperandType getType();

    std::string getDimension();
    void setDimension(std::string dim);

    std::string ToString();

};

#endif