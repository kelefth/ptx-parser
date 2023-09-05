#include "LoadInst.h"

LoadInst::LoadInst(Type type, Operand sourceOp)
    : LlvmInstruction("load"),
    LoadType(type),
    SourceOp(sourceOp) {}

LoadInst::LoadInst(Type type, Operand sourceOp, int align)
    : LlvmInstruction("load"),
    LoadType(type),
    SourceOp(sourceOp),
    Align(align) {}