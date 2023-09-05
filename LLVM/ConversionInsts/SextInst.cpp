#include "SextInst.h"

SextInst::SextInst(Type initialType, std::string value, Type newType)
    : LlvmInstruction("sext"),
    InitialType(initialType),
    Value(value),
    NewType(newType) {}

std::string SextInst::ToString() {
    return getName() + " " 
        + enumToString[InitialType] + " "
        + Value + " to"
        + enumToString[NewType];
}