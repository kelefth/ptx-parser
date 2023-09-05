#include "AllocaInst.h"

AllocaInst::AllocaInst(Type type)
    : LlvmInstruction("alloca"), AllocType(type), NumOfElements(1) {}

AllocaInst::AllocaInst(Type type, int numOfElements)
    : LlvmInstruction("alloca"), 
    AllocType(type),
    NumOfElements(numOfElements) {}

AllocaInst::AllocaInst(Type type, int numOfElements, int align)
    : LlvmInstruction("alloca"),
    AllocType(type),
    NumOfElements(numOfElements),
    Align(align) {}
