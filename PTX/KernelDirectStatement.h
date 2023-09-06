#ifndef KERNEL_DIRECTSTATEMT_H
#define KERNEL_DIRECTSTATEMT_H

#include <memory>

#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Function.h"

#include "Statement.h"
#include "DirectStatement.h"
#include "ParamDirectStatement.h"

class KernelDirectStatement : public DirectStatement {

    std::string Name;
    std::vector<std::shared_ptr<ParamDirectStatement>> Parameters;
    std::vector<std::shared_ptr<Statement>> BodyStatements;

public:

    KernelDirectStatement(
        std::string label,
        std::string name,
        std::vector<std::shared_ptr<ParamDirectStatement>> parameters,
        std::vector<std::shared_ptr<Statement>> bodyStatements
    );

    KernelDirectStatement(
        std::string label,
        std::string name
    );

    KernelDirectStatement(const KernelDirectStatement& stmt);

    std::string getName();
    void AddParameter(std::shared_ptr<ParamDirectStatement> parameter);
    void AddBodyStatement(std::shared_ptr<Statement> statement);

    llvm::Value* ToLlvmIr();

    void dump() const;
};

#endif