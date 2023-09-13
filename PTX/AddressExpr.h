#ifndef ADDRESSEXPR_H
#define ADDRESSEXPR_H

#include <string>
#include <memory>

class Operand;

class AddressExpr {

    std::string Operation;
    std::shared_ptr<Operand> Operand1;
    std::shared_ptr<Operand> Operand2;

public:

    AddressExpr(
        std::shared_ptr<Operand> operand1,
        std::shared_ptr<Operand> operand2,
        std::string operation
    );

    AddressExpr(std::shared_ptr<Operand> operand1);
    AddressExpr(const AddressExpr& expr);

    void setOperation(std::string op);
    std::string getOperation();

    void setFirstOperand(std::shared_ptr<Operand> operand);
    std::shared_ptr<Operand> getFirstOperand();

    void setSecondOperand(std::shared_ptr<Operand> operand);
    std::shared_ptr<Operand> getSecondOperand();

    bool operator==(const AddressExpr& expr) const;

    std::string ToString();
};

#endif