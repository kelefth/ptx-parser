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
    {"b8", llvm::Type::getInt8Ty},
    {"b16", llvm::Type::getInt16Ty},
    {"b32", llvm::Type::getInt32Ty},
    {"b64", llvm::Type::getInt64Ty},
    {"f16", llvm::Type::getHalfTy},
    {"f32", llvm::Type::getFloatTy},
    {"f64", llvm::Type::getDoubleTy},
    {"pred", llvm::Type::getInt1Ty}
};

std::unordered_map<int, std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>>>
PtxToLlvmIrConverter::PtxToLlvmMap;

std::unique_ptr<llvm::LLVMContext> PtxToLlvmIrConverter::Context;
std::unique_ptr<llvm::IRBuilder<llvm::NoFolder>> PtxToLlvmIrConverter::Builder;
std::unique_ptr<llvm::Module> PtxToLlvmIrConverter::Module;

void PtxToLlvmIrConverter::Initialize() {
    Context = std::make_unique<llvm::LLVMContext>();
    Module = std::make_unique<llvm::Module>("LLVM Module", *Context);
    Builder = std::make_unique<llvm::IRBuilder<llvm::NoFolder>>(*Context);
}

std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>>
PtxToLlvmIrConverter::getPtxToLlvmMapValue(int stmtId) {
    return PtxToLlvmMap[stmtId];
}

void PtxToLlvmIrConverter::setPtxToLlvmMapValue(
    int stmtId,
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> val
) {
    PtxToLlvmMap[stmtId] = val;
}

void PtxToLlvmIrConverter::removePtxToLlvmMapValue(int stmtId) {
    PtxToLlvmMap.erase(stmtId);
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

llvm::BasicBlock* PtxToLlvmIrConverter::GetBasicBlock(
    llvm::Function* func,
    std::string name
 ) {
    for (llvm::BasicBlock &block : *func) {
        if (block.getName() == name)
            return &block;
    }

    return nullptr;
}

uint PtxToLlvmIrConverter::ConvertPtxToLlvmAddrSpace(std::string addressSpace) {
    if (addressSpace == "global") return 1;
    else if (addressSpace == "shared") return 3;
    else if (addressSpace == "local") return 5;

    return 0;
}