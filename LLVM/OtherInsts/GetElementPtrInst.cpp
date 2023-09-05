#include "GetElementPtrInst.h"

GetElementPtrInst::GetElementPtrInst(
    Type type,
    Operand ptr,
    std::vector<Operand> indices
)
: LlvmInstruction("getelementptr"),
GetElemPtrType(type),
Inbounds(false),
Ptr(ptr),
Indices(indices) {}


GetElementPtrInst::GetElementPtrInst(
    Type type,
    bool inbounds,
    Operand ptr,
    std::vector<Operand> indices
)
: LlvmInstruction("getelementptr"),
Inbounds(inbounds),
GetElemPtrType(type),
Ptr(ptr),
Indices(indices) {}