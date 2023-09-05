#pragma once

#include "../LlvmInstruction.h"

class LoadInst : public LlvmInstruction {
    Type LoadType;
    Operand SourceOp;
    int Align;

public:
    LoadInst(Type type, Operand sourceOp);
    LoadInst(Type type, Operand sourceOp, int align);
};