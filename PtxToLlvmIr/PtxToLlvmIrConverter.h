#pragma once

#include <map>
#include <variant>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Type.h"

#include "../PTX/Statement.h"
// #include "../LLVM/LlvmInstruction.h"
// #include "../LLVM/LlvmKernel.h"
// #include "../LLVM/LlvmStatement.h"

using typeFunc = std::function<llvm::Type*(llvm::LLVMContext&)>;

class PtxToLlvmIrConverter {

    static const std::map<std::string, typeFunc> TypeMap;

public:

    static std::unique_ptr<llvm::LLVMContext> Context;
    static std::unique_ptr<llvm::IRBuilder<>> Builder;
    static std::unique_ptr<llvm::Module> Module;

    static void Initialize();

    static typeFunc GetTypeMapping(std::string type);

    // static std::variant<LlvmKernel, LlvmStatement> ConvertToLlvmIr(Statement* stmt);

};