#pragma once

#include "../LlvmInstruction.h"

enum ComparisonOp {
    eq,
    ne,
    ugt,
    uge,
    ult,
    ule,
    sgt,
    sge,
    slt,
    sle,
};

class IcmpInst : public LlvmInstruction {
    ComparisonOp Cond;
    Operand Operand1;
    Operand Operand2;

public:
    IcmpInst(ComparisonOp cond, Operand op1, Operand op2);

};