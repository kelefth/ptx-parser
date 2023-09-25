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

    std::shared_ptr<Statement> GetStatementById(unsigned int id);

    // Get source operand's value by getting the mapping
    // of the last instruction that used it as destination.
    // isComplex is used when the PTX instructions don't
    // have a direct mapping to LLVM instructions.
    llvm::Value* GetLlvmOperandValue(
        std::string ptxOperandName,
        bool isComplex
    );

    llvm::Constant* GetImmediateValue(double value);

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

    unsigned int getKernelId() const;
    std::string getInst() const;
    std::vector<std::string> getModifiers() const;
    std::vector<std::unique_ptr<Operand>>& getSourceOps();
    std::vector<std::unique_ptr<Operand>>& getDestOps();

    bool operator==(const Statement stmt) const;

    void ToLlvmIr();
    void dump() const;

};


#endif