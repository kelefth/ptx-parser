#pragma once

#include "../LlvmInstruction.h"

class SextInst : public LlvmInstruction {
    Type InitialType;
    std::string Value;
    Type NewType;

public:
    SextInst(Type initialType, std::string value, Type newType);

    std::string ToString();
};