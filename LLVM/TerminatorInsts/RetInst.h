#pragma once

#include "../LlvmInstruction.h"

class RetInst : public LlvmInstruction {
    Type RetType;
    std::string Value;

public:
    RetInst(std::string value, Type type);
    RetInst(std::string value);
};