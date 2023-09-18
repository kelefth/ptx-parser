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

std::unordered_map<int, std::vector<llvm::Value*>>
PtxToLlvmIrConverter::PtxToLlvmMap;

std::unique_ptr<llvm::LLVMContext> PtxToLlvmIrConverter::Context;
std::unique_ptr<llvm::IRBuilder<>> PtxToLlvmIrConverter::Builder;
std::unique_ptr<llvm::Module> PtxToLlvmIrConverter::Module;

void PtxToLlvmIrConverter::Initialize() {
    Context = std::make_unique<llvm::LLVMContext>();
    Module = std::make_unique<llvm::Module>("LLVM Module", *Context);
    Builder = std::make_unique<llvm::IRBuilder<>>(*Context);
}

std::vector<llvm::Value*>
PtxToLlvmIrConverter::getPtxToLlvmMapValue(int stmtId) {
    return PtxToLlvmMap[stmtId];
    // auto pos = PtxToLlvmMap.find(stmtId);
    // if (pos == PtxToLlvmMap.end()) return std::vector<llvm::Value*>();
    // return pos->second;
}

void PtxToLlvmIrConverter::setPtxToLlvmMapValue(
    int stmtId,
    std::vector<llvm::Value*> val
) {
    PtxToLlvmMap[stmtId] = val;
}

typeFunc PtxToLlvmIrConverter::GetTypeMapping(std::string type) {
    return TypeMap.at(type);
}

llvm::ICmpInst::Predicate PtxToLlvmIrConverter::ConvertPtxToLlvmPred(
    std::string pred
) {
    if (pred == "eq")
        return llvm::CmpInst::ICMP_EQ;
    else if (pred == "ne")
        return llvm::CmpInst::ICMP_NE;
    else if (pred == "ge")
        return llvm::CmpInst::ICMP_SGE;
    else if (pred == "le")
        return llvm::CmpInst::ICMP_SLE;
    else if (pred == "gt")
        return llvm::CmpInst::ICMP_SGT;
    else if (pred == "lt")
        return llvm::CmpInst::ICMP_SLT;

    return llvm::CmpInst::BAD_ICMP_PREDICATE;
}

// std::variant<LlvmKernel, LlvmStatement> PtxToLlvmIrConverter::ConvertToLlvmIr(Statement* stmt) {

//     // if (dynamic_cast<KernelDirectStatement*>(stmt) != nullptr) {
//     //     LlvmKernel kernel();
//     // }

//     return LlvmStatement("", LlvmInstruction(""));
// }