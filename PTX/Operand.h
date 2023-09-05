#ifndef OPERAND_H
#define OPERAND_H

#include <string>
#include <variant>

#include "AddressExpr.h"

class AddressExpr;

enum OperandType {
    Register,
    Immediate,
    Address,
    Label
};

class Operand {
    std::variant<std::string, double, AddressExpr> Value;
    OperandType Type;
    std::string Dimension;

public:
    Operand(std::string value, OperandType type);
    Operand(double value, OperandType type);
    Operand(AddressExpr value, OperandType type);
    Operand(std::string value, OperandType type, std::string dim);

    Operand(const Operand& operand);

    std::variant<std::string, double, AddressExpr> getValue();
    void setValue(std::variant<std::string, double, AddressExpr> value);

    OperandType getType();

    std::string getDimension();
    void setDimension(std::string dim);

    std::string ToString();

};

#endif