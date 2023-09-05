#include "CallInst.h"

CallInst::CallInst(Type type, std::string function)
    : LlvmInstruction("call"),
    CallType(type),
    Function(function) {}