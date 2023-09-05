#pragma once

#include "../LlvmInstruction.h"

class StoreInst : public LlvmInstruction {
    Operand SourceOp;
    Operand DestOp;
    int Align;

public:
    StoreInst(Operand sourceOp, Operand destOp);
    StoreInst(Operand sourceOp, Operand destOp, int align);
};