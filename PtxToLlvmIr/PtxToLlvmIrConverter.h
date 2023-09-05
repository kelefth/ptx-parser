#pragma once

#include <map>
#include <variant>

#include "../PTX/Statement.h"
#include "../LLVM/LlvmInstruction.h"
#include "../LLVM/LlvmKernel.h"
#include "../LLVM/LlvmStatement.h"

class PtxToLlvmIrConverter {

    static const std::map<std::string, std::string> TypeMap;

public:

    static std::string GetTypeMapping(std::string type);

    static std::variant<LlvmKernel, LlvmStatement> ConvertToLlvmIr(Statement* stmt);

};