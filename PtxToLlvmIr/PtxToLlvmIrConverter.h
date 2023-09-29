#pragma once

#include <map>
#include <variant>
#include <cstdlib>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"

#include "../PTX/Statement.h"
// #include "../LLVM/LlvmInstruction.h"
// #include "../LLVM/LlvmKernel.h"
// #include "../LLVM/LlvmStatement.h"

using typeFunc = std::function<llvm::Type*(llvm::LLVMContext&)>;

struct StatementHash
{
    std::size_t operator()(const Statement stmt) const noexcept
    {
        return stmt.getId();
    }
};

struct StatementEquality {
  bool operator()(Statement stmt1, Statement stmt2) const {
    return stmt1 == stmt2;
  }
};

class PtxToLlvmIrConverter {

    static const std::map<std::string, typeFunc> TypeMap;

    // Map that holds conversion of PTX to LLVM instructions
    static std::unordered_map<int, std::vector<llvm::Value*>>
    PtxToLlvmMap;

public:

    static std::unique_ptr<llvm::LLVMContext> Context;
    static std::unique_ptr<llvm::IRBuilder<>> Builder;
    static std::unique_ptr<llvm::Module> Module;

    static void Initialize();

    static typeFunc GetTypeMapping(std::string type);
    static std::vector<llvm::Value*> getPtxToLlvmMapValue(
        int stmtId
    );
    static void setPtxToLlvmMapValue(
        int stmtId,
        std::vector<llvm::Value*> val
    );
    static void removePtxToLlvmMapValue(int stmtId);

    static llvm::ICmpInst::Predicate ConvertPtxToLlvmPred(std::string pred);
    static llvm::BasicBlock* GetBasicBlock(
        llvm::Function* func,
        std::string name
    );

};