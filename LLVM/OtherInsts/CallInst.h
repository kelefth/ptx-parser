#pragma once

#include "../LlvmInstruction.h"

class CallInst : public LlvmInstruction {
    Type CallType;
    std::string Function;

public:
    CallInst(Type type, std::string function);
};