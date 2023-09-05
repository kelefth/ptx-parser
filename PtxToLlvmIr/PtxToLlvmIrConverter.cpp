#include "PtxToLlvmIrConverter.h"
#include "../PTX/Statement.h"
#include "../PTX/KernelDirectStatement.h"

const std::map<std::string, std::string> PtxToLlvmIrConverter::TypeMap {
    {"s8", "i8"},
    {"s16", "i16"},
    {"s32", "i32"},
    {"s64", "i64"},
    {"u8", "i8"},
    {"u16", "i16"},
    {"u32", "i32"},
    {"u64", "i64"},
    {"f16", "half"},
    {"f32", "float"},
    {"f64", "double"},
    {"pred", "i1"}
};

std::string PtxToLlvmIrConverter::GetTypeMapping(std::string type) {
    return TypeMap.at(type);
}

std::variant<LlvmKernel, LlvmStatement> PtxToLlvmIrConverter::ConvertToLlvmIr(Statement* stmt) {

    // if (dynamic_cast<KernelDirectStatement*>(stmt) != nullptr) {
    //     LlvmKernel kernel();
    // }

    return LlvmStatement("", LlvmInstruction(""));
}