#pragma once

#include "../LlvmInstruction.h"

class BrInst : public LlvmInstruction {

    std::string Cond;
    std::string LabelTrue;
    std::string LabelFalse;

public:
    BrInst(std::string labelTrue);
    BrInst(
        std::string cond,
        std::string labelTrue,
        std::string labelFalse
    );
};