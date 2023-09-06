#include "PtxToLlvmIrConverter.h"
#include "../PTX/Statement.h"
#include "../PTX/KernelDirectStatement.h"

const std::map<std::string, typeFunc> PtxToLlvmIrConverter::TypeMap {
    {"s8", llvm::Type::getInt8Ty},
    {"s16", llvm::Type::getInt16Ty},
    {"s32", llvm::Type::getInt32Ty},
    {"s64", llvm::Type::getInt64Ty},
    {"u8", llvm::Type::getInt8Ty},
    {"u16", llvm::Type::getInt16Ty},
    {"u32", llvm::Type::getInt32Ty},
    {"u64", llvm::Type::getInt64Ty},
    {"f16", llvm::Type::getHalfTy},
    {"f32", llvm::Type::getFloatTy},
    {"f64", llvm::Type::getDoubleTy},
    {"pred", llvm::Type::getInt1Ty}
};

std::unique_ptr<llvm::LLVMContext> PtxToLlvmIrConverter::Context;
std::unique_ptr<llvm::IRBuilder<>> PtxToLlvmIrConverter::Builder;
std::unique_ptr<llvm::Module> PtxToLlvmIrConverter::Module;

void PtxToLlvmIrConverter::Initialize() {
    Context = std::make_unique<llvm::LLVMContext>();
    Module = std::make_unique<llvm::Module>("LLVM Module", *Context);
    Builder = std::make_unique<llvm::IRBuilder<>>(*Context);
}

typeFunc PtxToLlvmIrConverter::GetTypeMapping(std::string type) {
    return TypeMap.at(type);
}

// std::variant<LlvmKernel, LlvmStatement> PtxToLlvmIrConverter::ConvertToLlvmIr(Statement* stmt) {

//     // if (dynamic_cast<KernelDirectStatement*>(stmt) != nullptr) {
//     //     LlvmKernel kernel();
//     // }

//     return LlvmStatement("", LlvmInstruction(""));
// }