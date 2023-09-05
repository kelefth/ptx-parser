#include "IcmpInst.h"

IcmpInst::IcmpInst(ComparisonOp cond, Operand op1, Operand op2)
    : LlvmInstruction("icmp"),
    Operand1(op1),
    Operand2(op2) {}