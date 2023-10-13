#include <iostream>

#include "KernelDirectStatement.h"

KernelDirectStatement::KernelDirectStatement(
    unsigned int id,
    std::string label,
    std::string name,
    std::vector<std::shared_ptr<ParamDirectStatement>> parameters,
    std::vector<std::shared_ptr<Statement>> bodyStatements
) : DirectStatement(id, label, ".entry"),
    Name(name),
    Parameters(parameters),
    BodyStatements(bodyStatements) {}

KernelDirectStatement::KernelDirectStatement(
    unsigned int id,
    std::string label,
    std::string name
) : DirectStatement(id, label, ".entry"),
    Name(name) {}

KernelDirectStatement::KernelDirectStatement(const KernelDirectStatement& stmt)
: DirectStatement(stmt.getId(), stmt.getLabel(), stmt.getDirective()) {
    Name = stmt.Name;
    Parameters = stmt.Parameters;
    BodyStatements = stmt.BodyStatements;
}

std::string KernelDirectStatement::getName() {
    return Name;
}

void KernelDirectStatement::AddParameter(std::shared_ptr<ParamDirectStatement> parameter) {
    Parameters.push_back(parameter);
}

void KernelDirectStatement::AddBodyStatement(std::shared_ptr<Statement> statement) {
    BodyStatements.push_back(statement);
}

std::vector<std::shared_ptr<Statement>>
KernelDirectStatement::getBodyStatements() {
    return BodyStatements;
}

bool KernelDirectStatement::operator==(const Statement stmt)
const {
    const KernelDirectStatement* kernelStmt =
        dynamic_cast<const KernelDirectStatement*>(&stmt);

    if (kernelStmt == nullptr) return false;

    return getId() == stmt.getId();
}

void KernelDirectStatement::ToLlvmIr() {

    // Create kernel definition

    std::vector<llvm::Type*> params;

    for (auto param : Parameters) {
        llvm::Type *paramType;
        if (param->getType() == "u64")
            paramType = llvm::Type::getInt32PtrTy(
                *PtxToLlvmIrConverter::Context
            );
        else {
            paramType = PtxToLlvmIrConverter::GetTypeMapping(
                param->getType()
            )(*PtxToLlvmIrConverter::Context);
        }
        params.push_back(paramType);
    }

    llvm::Type *funcType = llvm::Type::getVoidTy(
        *PtxToLlvmIrConverter::Context
    );

    llvm::FunctionType *ft = llvm::FunctionType::get(
        funcType,
        params,
        false
    );

    llvm::Function *func = llvm::Function::Create(
        ft,
        llvm::Function::ExternalLinkage,
        Name,
        PtxToLlvmIrConverter::Module.get()
    );

    // Set argument names
    unsigned int index = 0;
    for (auto &arg : func->args()) {
        arg.setName(Parameters[index]->getName());
        index++;
    }

    // Create kernel body
    llvm::BasicBlock *kernelBlock = llvm::BasicBlock::Create(
        *PtxToLlvmIrConverter::Context, "", func
    );

    PtxToLlvmIrConverter::Builder->SetInsertPoint(kernelBlock);

    // // Create allocations for parameters
    // std::vector<llvm::AllocaInst*> paramAllocs;
    // for (auto param : params) {
    //     paramAllocs.push_back(
    //         PtxToLlvmIrConverter::Builder->CreateAlloca(param)
    //     );
    // }

    // // Store parameter values in allocated space
    // for (int i = 0; i < params.size(); ++i) {
    //     llvm::Value *value = func->getArg(i);
    //     PtxToLlvmIrConverter::Builder->CreateStore(value, paramAllocs[i]);
    // }

    // Iterate through the body statements and create
    // instructions
    for (auto statement : BodyStatements) {
        statement->ToLlvmIr();
    }

    // return func;
}

void KernelDirectStatement::dump() const {
    std::cout << "Name: " << Name << std::endl;
    std::cout << "Parameters: " << std::endl;
    for (auto parameter : Parameters) {
        parameter->dump();
        std::cout << std::endl;
    }

    std::cout << "Body Statements: " << std::endl;
    for (auto statement : BodyStatements) {
        statement->dump();
        std::cout << std::endl;
    }
}