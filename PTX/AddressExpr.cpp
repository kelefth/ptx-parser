#include "AddressExpr.h"
#include "Operand.h"

AddressExpr::AddressExpr(
    std::shared_ptr<Operand> operand1,
    std::shared_ptr<Operand> operand2,
    std::string operation
) : Operand1(operand1), Operand2(operand2), Operation(operation) {}

AddressExpr::AddressExpr(std::shared_ptr<Operand> operand1) : Operand1(operand1) {
    Operand2 = nullptr;
    Operation = "";
}

AddressExpr::AddressExpr(const AddressExpr& expr) {
    Operand1 = expr.Operand1;
    Operand2 = expr.Operand2;
    Operation = expr.Operation;
}

void AddressExpr::setOperation(std::string op) {
    Operation = op;
}

std::string AddressExpr::getOperation() {
    return Operation;
}

void AddressExpr::setFirstOperand(std::shared_ptr<Operand> operand) {
    Operand1 = operand;
}

std::shared_ptr<Operand> AddressExpr::getFirstOperand() {
    return Operand1;
}

void AddressExpr::setSecondOperand(std::shared_ptr<Operand> operand) {
    Operand2 = operand;
}

std::shared_ptr<Operand> AddressExpr::getSecondOperand() {
    return Operand2;
}

bool AddressExpr::operator==(const AddressExpr& expr) const {
    return Operation == expr.Operation  &&
           Operand1 == expr.Operand1    &&
           Operand2 == expr.Operand2;
}

std::string AddressExpr::ToString() {
    std::string str = Operand1->ToString();
    if (Operation != "") str += " " + Operation;
    if (Operand2) str += " " + Operand2->ToString();

    return str;
}