#include "RetInst.h"

RetInst::RetInst(std::string value, Type type)
    : LlvmInstruction("ret"),
    Value(value),
    RetType(type) {}

RetInst::RetInst(std::string value)
    : LlvmInstruction("ret"), Value(value) {}