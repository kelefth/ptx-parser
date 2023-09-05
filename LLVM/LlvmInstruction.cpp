#include "LlvmInstruction.h"

LlvmInstruction::LlvmInstruction(std::string name) : Name(name) {}

std::string LlvmInstruction::getName() {
    return Name;
}

Operand::Operand(
    std::string name,
    Type type
) : Name(name), OpType(type), PtrDepth(0) {}

Operand::Operand(
    std::string name,
    Type type,
    int ptrDepth
) : Name(name), OpType(type), PtrDepth(ptrDepth) {}

Type Operand::getType() {
    return OpType;
}

int main() {
    
}