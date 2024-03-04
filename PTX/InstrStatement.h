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
#include "LinkingDirectStatement.h"

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

    // Check if block needle is a predecessor of block haystack
    bool isBlockInPredecessors(
        llvm::BasicBlock* needle,
        llvm::BasicBlock* haystack
    );

    // llvm::PHINode* CheckAndGeneratePhiNode(
    //     std::string opName,
    //     std::pair<llvm::Value*, llvm::BasicBlock*> llvmStmt,
    //     std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> llvmStmts,
    //     llvm::Instruction* lastLlvmInst,
    //     llvm::BasicBlock* currBlock,
    //     std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>>* incomingValBlocksToAdd,
    //     std::vector<llvm::BasicBlock*> visitedBlocks,
    //     llvm::PHINode* phi
    // );
    llvm::PHINode* CheckAndGeneratePhiNode(
        std::string opName,
        std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>>* incomingValBlocksToAdd
    );

    void GeneratePhiNodes(std::string ptxOperandName, int stStmtId);

    // Get source operand's value by getting the mapping
    // of the last instruction that used it as destination.
    // isComplex is used when the PTX instructions don't
    // have a direct mapping to LLVM instructions.
    llvm::Value* GetLlvmRegisterValue(
        std::string ptxOperandName,
        int stStmtId
    );

    llvm::Value* GetLlvmOperandValue(
        const std::unique_ptr<Operand>& operand,
        llvm::Type* type,
        int stStmtId
    );

    llvm::Constant* GetLlvmImmediateValue(double value, llvm::Type* type, bool isSigned);

    std::unique_ptr<KernelDirectStatement> GetCurrentKernel();

    // Check if label "name" is a variable and return it
    DirectStatement* GetVar(std::string name);

    // Find and return the last instructions in each block before inst,
    // where the source operand at sourceOpNum was modified
    std::vector<uint> GetOperandWriteInstructionIds(
        InstrStatement* inst,
        uint sourceOpNum
    );
    std::vector<uint> GetOperandWriteInstructionIds(
        std::string operandName,
        int stStmtId
    );

    std::vector<InstrStatement*> GetOperandWriteInstructions(
        InstrStatement* inst,
        uint sourceOpNum
    );

    // Create a phi node in the beginning of the current block and add
    // an incoming value from the previous block
    llvm::PHINode* CreatePhiInBlockStart(
        llvm::Value* value,
        llvm::BasicBlock* currBasicBlock
    );

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
    std::vector<std::string> getTypes() const;
    std::vector<std::unique_ptr<Operand>>& getSourceOps();
    std::vector<std::unique_ptr<Operand>>& getDestOps();

    bool operator==(const Statement stmt) const;

    void ToLlvmIr();
    void dump() const;

};


#endif