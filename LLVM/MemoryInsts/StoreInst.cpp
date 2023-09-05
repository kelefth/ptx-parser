#include "StoreInst.h"

StoreInst::StoreInst(Operand sourceOp, Operand destOp)
    : LlvmInstruction("store"),
    SourceOp(sourceOp),
    DestOp(destOp) {}

StoreInst::StoreInst(Operand sourceOp, Operand destOp, int align)
    : LlvmInstruction("store"),
    SourceOp(sourceOp),
    DestOp(destOp),
    Align(align) {}