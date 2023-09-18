#ifndef INSTR_STATEMENT_H
#define INSTR_STATEMENT_H

#include <string>
#include <vector>
#include <variant>
#include <memory>

#include "llvm/IR/Value.h"

#include "Statement.h"
#include "Operand.h"
#include "KernelDirectStatement.h"

class InstrStatement : public Statement {

    unsigned int KernelId;
    std::string Pred;
    std::string Inst;
    std::vector<std::string> Modifiers;
    std::vector<std::string> Types;
    // std::vector<std::variant<std::string, double>> DestOps;
    // std::vector<std::variant<std::string, double>> SourceOps;
    std::vector<std::unique_ptr<Operand>> DestOps;
    std::vector<std::unique_ptr<Operand>> SourceOps;

    llvm::Value* getLlvmOperandValue(std::string ptxOperandName);
    std::unique_ptr<KernelDirectStatement> GetCurrentKernel();

public:
    InstrStatement(
        unsigned int id,
        std::string label,
        unsigned int kernelId,
        std::string pred,
        std::string inst,
        std::vector<std::string> modifiers,
        std::vector<std::string> types,
        std::vector<std::unique_ptr<Operand>> destOps,
        std::vector<std::unique_ptr<Operand>> sourceOps
    );

    unsigned int getKernelId();

    // std::string ToString();
    std::vector<std::unique_ptr<Operand>>& getSourceOps();
    std::vector<std::unique_ptr<Operand>>& getDestOps();

    bool operator==(const Statement stmt) const;

    void ToLlvmIr();
    void dump() const;

};


#endif