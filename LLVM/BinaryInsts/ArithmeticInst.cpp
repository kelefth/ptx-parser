#include "ArithmeticInst.h"

ArithmeticInst::ArithmeticInst(
    std::string name,
    Type type,
    Operand op1,
    Operand op2
) : LlvmInstruction(name), Operand1(op1), Operand2(op2) {

    type = op1.getType();
}