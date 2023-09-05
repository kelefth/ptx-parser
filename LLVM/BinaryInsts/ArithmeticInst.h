#pragma once

#include "../LlvmInstruction.h"

class ArithmeticInst : public LlvmInstruction {

    Type InstType;
    Operand Operand1;
    Operand Operand2;
    bool Nuw;
    bool Nsw;

public:

    ArithmeticInst(
        std::string name,
        Type type,
        Operand op1,
        Operand op2
    );
};