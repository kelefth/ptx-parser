#pragma once

#include <map>
#include <variant>
#include <cstdlib>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/NoFolder.h"
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

    // Map that holds conversion of PTX to LLVM instructions and the block in
    // which they are inserted (needed for constants)
    static std::unordered_map<
        int, std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>>>
        PtxToLlvmMap;

    static std::map<
        llvm::BasicBlock*, std::map<std::string, llvm::Value*>
    > RegToValueInMap;

    static std::map<
        llvm::BasicBlock*, std::map<std::string, llvm::Value*>
    > RegToValueOutMap;


  public:

    static std::unique_ptr<llvm::LLVMContext> Context;
    static std::unique_ptr<llvm::IRBuilder<llvm::NoFolder>> Builder;
    static std::unique_ptr<llvm::Module> Module;

    static void Initialize();

    static typeFunc GetTypeMapping(std::string type);

    static std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>>
    getPtxToLlvmMapValue(int stmtId);

    static void setPtxToLlvmMapValue(
        int stmtId,
        std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> val
    );
    static void removePtxToLlvmMapValue(int stmtId);

    static std::map<
        llvm::BasicBlock*, std::map<std::string, llvm::Value*>
    > getRegToValueInMaps();

    static std::map<
        llvm::BasicBlock*, std::map<std::string, llvm::Value*>
    > getRegToValueOutMaps();

    static llvm::Value*
    getRegToValueInMapValue(llvm::BasicBlock* bb, std::string regName);

    static void setRegToValueInMapValue(
        llvm::BasicBlock* bb,
        std::string regName,
        llvm::Value* value
    );

    static void setRegToValueInMap(
        llvm::BasicBlock* bb,
        std::map<std::string, llvm::Value*> map
    );

    static std::map<std::string, llvm::Value*>
    getRegToValueInMap(llvm::BasicBlock* bb);

    static void clearRegToValueInMap(llvm::BasicBlock* bb);

    static llvm::Value*
    getRegToValueOutMapValue(llvm::BasicBlock* bb, std::string regName);

    static void setRegToValueOutMapValue(
        llvm::BasicBlock* bb,
        std::string regName,
        llvm::Value* value
    );

    static void setRegToValueOutMap(
        llvm::BasicBlock* bb,
        std::map<std::string, llvm::Value*> map
    );

    static std::map<std::string, llvm::Value*>
    getRegToValueOutMap(llvm::BasicBlock* bb);

    static void clearRegToValueOutMap(llvm::BasicBlock* bb);

    static void UpdateMapsAndGeneratePhis(llvm::BasicBlock* bb);

    static llvm::ICmpInst::Predicate ConvertPtxToLlvmPred(std::string pred);
    static llvm::BasicBlock* GetBasicBlock(
        llvm::Function* func,
        std::string name
    );

    static uint ConvertPtxToLlvmAddrSpace(std::string addressSpace);

};