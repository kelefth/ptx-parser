#pragma once

#include "../LlvmInstruction.h"

class AllocaInst : public LlvmInstruction {

    Type AllocType;
    int NumOfElements;
    int Align;

public:
    AllocaInst(Type type);
    AllocaInst(Type type, int numOfElements);
    AllocaInst(Type type, int numOfElements, int align);
};