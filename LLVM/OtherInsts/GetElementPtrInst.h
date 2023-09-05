#pragma once

#include <vector>

#include "../LlvmInstruction.h"

class GetElementPtrInst : public LlvmInstruction {
    Type GetElemPtrType;
    bool Inbounds;
    Operand Ptr;
    std::vector<Operand> Indices;

public:
    GetElementPtrInst(
        Type type,
        Operand ptr,
        std::vector<Operand>
    );
    
    GetElementPtrInst(
        Type type,
        bool inbounds,
        Operand ptr,
        std::vector<Operand> indices
    );
};