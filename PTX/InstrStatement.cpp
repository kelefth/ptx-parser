#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <algorithm>
#include <regex>

// #include "KernelDirectStatement.h"

#include "parser/parser.h"
#include "InstrStatement.h"
#include "../PtxToLlvmIr/PtxToLlvmIrConverter.h"
#include "Operand.h"
#include "LinkingDirectStatement.h"
#include "VarDecDirectStatement.h"

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/CFG.h"

// std::string InstrStatement::ToString() {
//     std::string str = "Label: " + getLabel() + " Instruction: " + Inst + " Modifiers: ";
//     int index = 0;
//     for (std::string modifier : Modifiers) {
//         str += modifier;
//         index++;
//         if (index < Modifiers.size()) str += ", ";
//     }

//     str += " Type: " + Type;

//     str += " Source Operands: ";
//     index = 0;
//     for (std::string dest : DestOps) {
//         str += dest;
//         index++;
//         if (index < DestOps.size()) str += ", ";
//     }

//     str += " Dest. Operands: ";
//     index = 0;
//     for (std::string source : SourceOps) {
//         str += source;
//         index++;
//         if (index < SourceOps.size()) str += ", ";
//     }

//     return str;
// }

InstrStatement::InstrStatement(
        unsigned int id,
        std::string label,
        unsigned int kernelId,
        std::string pred,
        std::string inst,
        std::vector<std::string> modifiers,
        std::vector<std::string> types,
        std::vector<std::unique_ptr<Operand>> destOps,
        std::vector<std::unique_ptr<Operand>> sourceOps
    )
    : Statement(id, label),
      KernelId(kernelId),
      Pred(pred),
      Inst(inst),
      Modifiers(modifiers),
      Types(types),
      DestOps(std::move(destOps)),
      SourceOps(std::move(sourceOps)) {}

std::string operandTypeToString(OperandType type) {
    switch (type) {
        case Register:
            return "R";
            break;
        case Immediate:
            return "I";
            break;
        case Address:
            return "A";
            break;
        case Label:
            return "L";
            break;
        default:
            return "";
            break;
    }
}

unsigned int InstrStatement::getKernelId() const {
    return KernelId;
}

std::string InstrStatement::getInst() const {
    return Inst;
}

std::vector<std::string> InstrStatement::getModifiers() const {
    return Modifiers;
}

std::vector<std::unique_ptr<Operand>>& InstrStatement::getSourceOps() {
    return SourceOps;
}

std::vector<std::unique_ptr<Operand>>& InstrStatement::getDestOps() {
    return DestOps;
}

bool InstrStatement::operator==(const Statement stmt) const {
    const InstrStatement* instrStmt =
        dynamic_cast<const InstrStatement*>(&stmt);

    if (instrStmt == nullptr) return false;
    
    return getId() == stmt.getId();
}

// find the kernel that the instruction is in
std::unique_ptr<KernelDirectStatement> InstrStatement::GetCurrentKernel() {
    std::unique_ptr<KernelDirectStatement> currKernel = nullptr;
    for (const auto stmt : statements) {
        KernelDirectStatement* kernelStatement =
                dynamic_cast<KernelDirectStatement*>(stmt.get());
        if(kernelStatement == nullptr) continue;
        if (kernelStatement->getId() == KernelId) {
            currKernel = std::make_unique<KernelDirectStatement>(
                *kernelStatement
            );
        }
    }

    return currKernel;
}

std::shared_ptr<Statement> InstrStatement::GetStatementById(unsigned int id) {
    for (auto stmt : statements) {
        KernelDirectStatement* kernelPtr =
            dynamic_cast<KernelDirectStatement*>(stmt.get());

        if (kernelPtr != nullptr) {
            for (auto bodyStmt : kernelPtr->getBodyStatements()) {
                if (bodyStmt->getId() == id)
                    return bodyStmt;
            }
        }

        if (stmt->getId() == id)
            return stmt;
    }

    return nullptr;
}

bool InstrStatement::isBlockInPredecessors(
    llvm::BasicBlock* blockToFind,
    llvm::BasicBlock* block
) {
    // iterate through predecessors and return true if blockToFind exists
    if (block && block->hasNPredecessorsOrMore(1)) {
        llvm::pred_iterator pred = llvm::pred_begin(block);
        llvm::pred_iterator end = llvm::pred_end(block);
        for (pred; pred != end; ++pred)
            if (*pred == blockToFind) return true;
    }

    return false;
}

bool isInSuccessors(llvm::BasicBlock* block, llvm::BasicBlock *blockToFind) {
    
    if (block == blockToFind) return true;

    for (auto succIt = succ_begin(block), succEnd = succ_end(block); succIt != succEnd; ++succIt) {
        if (*succIt == block) continue;

        llvm::BasicBlock* successorBlock = *succIt;
        return isInSuccessors(successorBlock, blockToFind);
    }

    return false;
}

llvm::PHINode* InstrStatement::CheckAndGeneratePhiNode(
    std::pair<llvm::Value*, llvm::BasicBlock*> llvmStmt,
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> llvmStmts,
    llvm::Instruction* lastLlvmInst,
    llvm::BasicBlock* currBlock,
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>>* incomingValBlocksToAdd,
    llvm::PHINode* phi = nullptr
) {
    llvm::Value* llvmStmtValue = llvmStmt.first;
    llvm::BasicBlock* llvmStmtBlock = llvmStmt.second;

    llvm::pred_iterator pred = llvm::pred_begin(currBlock);
    llvm::pred_iterator end = llvm::pred_end(currBlock);
    for (pred; pred != end; ++pred) {
        if (isInSuccessors(llvmStmtBlock, *pred)) {
            // Check if an incoming value from the same block already exists,
            // and if yes the phi node must be inserted into a predecessor
            bool isPhiInPred = false;
            for (auto valBlock : *incomingValBlocksToAdd) {
                if (valBlock.second == *pred) {
                    phi = CheckAndGeneratePhiNode(
                        llvmStmt,
                        llvmStmts,
                        lastLlvmInst,
                        *pred,
                        incomingValBlocksToAdd
                    );
                    isPhiInPred = true;
                }
            }

            if ((incomingValBlocksToAdd->size() > 0) && !phi) {
                phi = llvm::PHINode::Create(
                    lastLlvmInst->getType(),
                    0,
                    "",
                    &*currBlock->getFirstInsertionPt()
                );
                // phi->print(llvm::outs());
                // llvm::outs() << "\n";
            }
            if (!isPhiInPred) {
                incomingValBlocksToAdd->push_back(
                    std::pair<llvm::Value*, llvm::BasicBlock*>(
                        llvmStmtValue, *pred
                    )
                );
            }
            else {
                incomingValBlocksToAdd->clear();
                for (std::pair<llvm::Value*, llvm::BasicBlock*> newLlvmStmt : llvmStmts) {
                    CheckAndGeneratePhiNode(
                        newLlvmStmt,
                        llvmStmts,
                        lastLlvmInst,
                        *pred,
                        incomingValBlocksToAdd,
                        phi
                    );
                }
            }
        }
    }
    return phi;
}

llvm::Value* InstrStatement::GetLlvmRegisterValue(std::string ptxOperandName) {
    std::vector<uint> defStmtIds = GetOperandWriteInstructionIds(ptxOperandName);

    // if none found, return
    if (defStmtIds.empty()) return nullptr;

    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> llvmStmts;
    for (uint stmtId : defStmtIds) {
        std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> mappedStmts =
            PtxToLlvmIrConverter::getPtxToLlvmMapValue(stmtId);
        llvmStmts.insert(
            llvmStmts.end(),
            mappedStmts.begin(),
            mappedStmts.end()
        );
    }

    if (llvmStmts.empty()) {
        std::shared_ptr<Statement> userStmt = GetStatementById(defStmtIds.back());
        InstrStatement* userStmtInst = dynamic_cast<InstrStatement*>(
            userStmt.get()
        );

        if (!userStmt) return nullptr;

        for (auto &op : userStmtInst->getSourceOps()) {
            if (op->getType() == OperandType::Register) {
                std::string opName = std::get<std::string>(op->getValue());
                llvm::Value* value = GetLlvmRegisterValue(opName);
                return value;
            }
        }
    }
    
    if (llvmStmts.empty()) return nullptr;

    llvm::Value* lastLlvmStmt = llvmStmts.back().first;
    llvm::Instruction* lastLlvmInst = llvm::dyn_cast<llvm::Instruction>(
        lastLlvmStmt
    );

    // get last if in the same block, or create a phi node with the last
    // of each predecessor block, or find the value from an already created
    // phi node
    llvm::BasicBlock* currBlock = PtxToLlvmIrConverter::Builder->GetInsertBlock();
    if (lastLlvmInst) {
        llvm::PHINode* phi = nullptr;
        bool blockChanged = false;
        llvm::BasicBlock* prevBlock = currBlock;
        // reverse the vector to get the last generated instruction from each
        // block
        reverse(llvmStmts.begin(), llvmStmts.end());
        std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> incomingValBlocksToAdd;
        for (std::pair<llvm::Value*, llvm::BasicBlock*> llvmStmt : llvmStmts) {
            llvm::Value* llvmStmtValue = llvmStmt.first;
            llvm::BasicBlock* llvmStmtBlock = llvmStmt.second;
            if (prevBlock != llvmStmtBlock) {
                if (isBlockInPredecessors(llvmStmtBlock, currBlock)) {
                    // If in direct predecessor

                    // if already shown in another predecessor create a phi node,
                    // or add a value to an existing one
                    if ((incomingValBlocksToAdd.size() > 0) && !phi) {
                        phi = llvm::PHINode::Create(
                            lastLlvmInst->getType(),
                            0,
                            "",
                            &*currBlock->getFirstInsertionPt()
                        );
                        phi->print(llvm::outs());
                        llvm::outs() << "\n";
                    }
                    incomingValBlocksToAdd.push_back(
                        std::pair<llvm::Value*, llvm::BasicBlock*>(
                            llvmStmtValue, llvmStmtBlock
                        )
                    );
                }
                else {
                    // If not in direct predecessor
                    phi = CheckAndGeneratePhiNode(
                        llvmStmt,
                        llvmStmts,
                        lastLlvmInst,
                        currBlock,
                        &incomingValBlocksToAdd
                    );
                }
            }

            prevBlock = llvmStmtBlock;
        }

        if (phi) {
            // Add all the incoming values to the generated phi node
            for (auto valBlock : incomingValBlocksToAdd) {
                phi->addIncoming(valBlock.first, valBlock.second);
            }
            return phi;
        }

        reverse(llvmStmts.begin(), llvmStmts.end());
    }

    // generate a load and get its value if the last generate instruction is a
    // store, else get the value of the last generated llvm instruction
    // that returns a value
    // std::string lastLlvmStmtName = lastLlvmInst->getOpcodeName();
    llvm::Value* instValue = nullptr;
    // if (lastLlvmInst && (lastLlvmInst->getOpcode() == llvm::Instruction::Store)) {
    //     llvm::Value* firstOperand = lastLlvmInst->getOperand(0);
    //     llvm::Value* secondOperand = lastLlvmInst->getOperand(1);
    //     llvm::Type* loadType = firstOperand->getType();
    //     instValue = PtxToLlvmIrConverter::Builder->CreateLoad(
    //         loadType,
    //         secondOperand
    //     );
    // }
    // else {
        for (auto it = llvmStmts.rbegin(); it != llvmStmts.rend(); ++it) {
            if ((*it).first->getType()->getTypeID() != llvm::Type::VoidTyID) {
                instValue = (*it).first;
                break;
            }
        }
    // }

    return instValue;
}

llvm::Value* InstrStatement::GetLlvmOperandValue(
    const std::unique_ptr<Operand>& operand
) {
    auto operandValue = operand->getValue();
    OperandType operandType = operand->getType();

    llvm::Value* operandLlvmValue = nullptr;
    if (operandType == OperandType::Register) {
        std::string regName = std::get<std::string>(operandValue);
        operandLlvmValue = GetLlvmRegisterValue(regName);
    }
    else if (operandType == OperandType::Immediate) {
        double value = std::get<double>(operandValue);
        operandLlvmValue = GetLlvmImmediateValue(value);
    }

    return operandLlvmValue;
}

llvm::Constant* InstrStatement::GetLlvmImmediateValue(double value) {
    bool isSigned = false;
    // if signed
    if (Types[0][0] == 's')
        isSigned = true;

    // check if integer or float
    if (Types[0][0] != 'f') {
        std::string nbitsStr = std::regex_replace(
            Types[0],
            std::regex("[a-z]+"),
            std::string("$1")
        );
        int nbits = stoi(nbitsStr);

        llvm::Type *type = llvm::Type::getIntNTy(
            *PtxToLlvmIrConverter::Context,
            nbits
        );

        return llvm::ConstantInt::get(
            type,
            value,
            isSigned
        );
    }
    else {
        llvm::Type *type = llvm::Type::getFloatTy(
            *PtxToLlvmIrConverter::Context
        );

        return llvm::ConstantFP::get(type, value);
    }
}

DirectStatement* InstrStatement::GetVar(std::string name) {
    std::unique_ptr<KernelDirectStatement> currKernel = GetCurrentKernel();
    DirectStatement* varStmt = nullptr;

    // check for global variables
    for (auto stmt : statements) {
        if (stmt->getId() > currKernel->getId()) break;
        LinkingDirectStatement* linkingDir =
            dynamic_cast<LinkingDirectStatement*>(stmt.get());
        if (linkingDir != nullptr) {
            if (linkingDir->getIdentifier() == name) {
                varStmt = linkingDir;
                break;
            }
        }
    }

    if (!varStmt) {    
        // check for variables in kernel
        for (auto stmt : currKernel->getBodyStatements()) {
            VarDecDirectStatement* varDecDir =
                dynamic_cast<VarDecDirectStatement*>(stmt.get());
            if (varDecDir != nullptr) {
                if (varDecDir->getIdentifier() == name) {
                    varStmt = varDecDir;
                    break;
                }
            }
        }
    }

    return varStmt;
}

std::vector<uint> InstrStatement::GetOperandWriteInstructionIds(
    InstrStatement* inst,
    uint sourceOpNum
) {
    std::unique_ptr currKernel = GetCurrentKernel();
    std::vector<std::shared_ptr<Statement>> currKernelStatements =
        currKernel->getBodyStatements();

    std::vector<uint> defInstIds;
    if (sourceOpNum > inst->getSourceOps().size() - 1) return defInstIds;

    auto opValue = inst->getSourceOps()[sourceOpNum]->getValue();
    std::string sourceOpName = std::get<std::string>(opValue);

    for (const auto stmt : currKernelStatements) {
        unsigned int stmtId = stmt->getId();
        if (stmtId >= inst->getId()) break;
        InstrStatement* instrStatement =
            dynamic_cast<InstrStatement*>(stmt.get());
        if (instrStatement == nullptr)
            continue;
        for (const auto &destOp : instrStatement->getDestOps()) {
            if (destOp->getType() == OperandType::Register) {
                std::string destOpValue = std::get<std::string>(
                    destOp->getValue()
                );

                if (destOpValue == sourceOpName) {
                    defInstIds.push_back(stmtId);
                }
            }
        }
    }

    // return defInsts;
    return defInstIds;
}

std::vector<uint> InstrStatement::GetOperandWriteInstructionIds(
    std::string operandName
) {
    std::unique_ptr currKernel = GetCurrentKernel();
    std::vector<std::shared_ptr<Statement>> currKernelStatements =
        currKernel->getBodyStatements();

    std::vector<uint> defInstIds;

    for (const auto stmt : currKernelStatements) {
        unsigned int stmtId = stmt->getId();
        if (stmtId >= this->getId()) break;
        InstrStatement* instrStatement =
            dynamic_cast<InstrStatement*>(stmt.get());
        if (instrStatement == nullptr)
            continue;
        for (const auto &destOp : instrStatement->getDestOps()) {
            if (destOp->getType() == OperandType::Register) {
                std::string destOpValue = std::get<std::string>(
                    destOp->getValue()
                );

                if (destOpValue == operandName) {
                    defInstIds.push_back(stmtId);
                }
            }
        }
    }

    // return defInsts;
    return defInstIds;
}

std::vector<InstrStatement*> InstrStatement::GetOperandWriteInstructions(
    InstrStatement* inst,
    uint sourceOpNum
) {
    std::vector<uint> instIds = GetOperandWriteInstructionIds(inst, sourceOpNum);
    std::vector<InstrStatement*> defInsts;
    for (uint id : instIds) {
        std::shared_ptr<Statement> stmt = GetStatementById(id);
        InstrStatement* defInst = dynamic_cast<InstrStatement*>(
            stmt.get()
        );
        defInsts.push_back(defInst);
    }

    return defInsts;
}

llvm::PHINode* InstrStatement::CreatePhiInBlockStart(
    llvm::Value* value,
    llvm::BasicBlock* currBasicBlock
) {

    llvm::PHINode* phi = llvm::PHINode::Create(
        value->getType(),
        2,
        "",
        &*currBasicBlock->getFirstInsertionPt()
    );

    // find previous block
    llvm::BasicBlock* prevBb = nullptr;
    for (llvm::BasicBlock &bb : *currBasicBlock->getParent()) {
        if (&bb == currBasicBlock) break;
        prevBb = &bb;
    }

    phi->addIncoming(value, prevBb);

    return phi;
}

void InstrStatement::ToLlvmIr() {
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> genLlvmInstructions;

    // if instruction has a label, change IR insert point
    std::string label = getLabel();
    if (label != "") {
        std::unique_ptr<KernelDirectStatement> currKernel = GetCurrentKernel();

        int userStmtId = -1;
        for (const auto stmt : currKernel->getBodyStatements()) {
            unsigned int stmtId = stmt->getId();
            if (stmtId < this->getId()) {
                InstrStatement* instrStatement =
                    dynamic_cast<InstrStatement*>(stmt.get());
                if (instrStatement == nullptr)
                    continue;
                
                std::string destOpValue;
                if (
                    (!instrStatement->getDestOps().empty()) &&
                    (instrStatement->getDestOps()[0])       &&
                    (instrStatement->getDestOps()[0]->getType() == OperandType::Label)
                ) {
                    destOpValue = std::get<std::string>(
                        instrStatement->getDestOps()[0]->getValue()
                    );
                }

                if (
                    instrStatement->getInst() == "bra" &&
                    destOpValue == label
                ) {
                    userStmtId = stmtId;
                    break;
                }
            }
        }

        std::string currKernelName = currKernel->getName();
        llvm::Function* currFunction =
            PtxToLlvmIrConverter::Module->getFunction(currKernelName);

        llvm::BasicBlock* newInsertBlock = nullptr;

        // if statement found
        if (userStmtId != -1) {
            llvm::Value* llvmStmt =
                PtxToLlvmIrConverter::getPtxToLlvmMapValue(userStmtId)[0].first;

            llvm::Instruction* braInst = llvm::cast<llvm::Instruction>(
                llvmStmt
            );

            // Set new insert point to true branch of bra instruction
            newInsertBlock = braInst->getSuccessor(0);
        }
        else {
            // check if basic block already exists
            newInsertBlock = PtxToLlvmIrConverter::GetBasicBlock(
                currFunction,
                label
            );

            // if it doesn't exist, create a new one
            if (newInsertBlock == nullptr) {
                // find the next basic block to insert the new one before it
                llvm::BasicBlock *nextBasicBlock = nullptr;
                int minIndex = 9999999;
                for (llvm::BasicBlock &bb : *currFunction) {
                    std::string bbName = bb.getName().str();
                    if (bbName == "") continue;
                    std::string bbIndexStr = bbName.substr(
                        bbName.find_last_of('_'),
                        bbName.length()
                    );
                    
                    std::string newBbIndexStr = label.substr(
                        label.find_last_of('_'),
                        label.length()
                    );

                    int newBbIndex = stoi(newBbIndexStr.erase(0, 1));
                    int bbIndex = stoi(bbIndexStr.erase(0, 1));
                    
                    if ((bbIndex > newBbIndex) && (bbIndex < minIndex)) {
                        minIndex = bbIndex;
                        nextBasicBlock = &bb;
                    }
                }

                newInsertBlock = llvm::BasicBlock::Create(
                    *PtxToLlvmIrConverter::Context,
                    label,
                    currFunction,
                    nextBasicBlock
                );
            }
        }

        // Update the instruction insertion point
        PtxToLlvmIrConverter::Builder->SetInsertPoint(newInsertBlock);

        // Iterate through blocks, delete empty blocks and add terminators if missing
        llvm::BasicBlock* insertBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        std::vector<llvm::BasicBlock*> blocksToRemove;
        for (auto bbIt = currFunction->begin(); bbIt != currFunction->end(); ++bbIt) {
            llvm::BasicBlock &bb = *bbIt;
            if (bb.getTerminator() || (&bb == insertBlock) || bb.hasName())
                continue;

            auto bbItNext = std::next(bbIt);

            if (bb.empty()) {
                blocksToRemove.push_back(&bb);
                if (bbItNext != currFunction->end()) {
                    llvm::BasicBlock &nextBb = *bbItNext;
                    // bb.replaceAllUsesWith(&nextBb);
                }
            }

            if (bbItNext != currFunction->end()) {
                llvm::BasicBlock &nextBb = *bbItNext;
                llvm::BranchInst* br = llvm::BranchInst::Create(&nextBb, &bb);
            }
        }

        // for (llvm::BasicBlock* block : blocksToRemove)
        //     block->eraseFromParent();
    }

    if (Inst == "ld") {
        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        // check if it's an ld.param instruction
        auto modIterator = std::find(
            Modifiers.begin(),
            Modifiers.end(),
            "param"
        );
        // if ld.param
        if (modIterator != Modifiers.end()) {
            llvm::Function *func =
                &PtxToLlvmIrConverter::Module->getFunctionList().back();

            // get value of source operand
            // ld.param's source operand's value is always an AddressExpr
            auto value = std::get<AddressExpr>(SourceOps[0]->getValue());

            auto args = func->args();
            for (auto &arg : args) {
                if (
                    (arg.getName() == value.getFirstOperand()->ToString()) ||
                    (arg.getName() == value.getSecondOperand()->ToString())
                ) {
                    llvm::Type *argType = arg.getType();
                    llvm::AllocaInst *alloca =
                        PtxToLlvmIrConverter::Builder->CreateAlloca(argType);
                    llvm::Value *store =
                        PtxToLlvmIrConverter::Builder->CreateStore(&arg,alloca);
                    llvm::Value *load =
                        PtxToLlvmIrConverter::Builder->CreateLoad(argType, alloca);

                    // Store generated instructions
                    genLlvmInstructions.push_back(
                        std::pair<llvm::Value*, llvm::BasicBlock*>(
                            alloca, currBlock)
                    );
                    genLlvmInstructions.push_back(
                        std::pair<llvm::Value*, llvm::BasicBlock*>(
                            store, currBlock)
                    );
                    genLlvmInstructions.push_back(
                        std::pair<llvm::Value*, llvm::BasicBlock*>(
                            load, currBlock)
                    );
                }
            }
        }
        else {
            auto sourceOpAddr = std::get<AddressExpr>(SourceOps[0]->getValue());

            std::shared_ptr<Operand> addrFirstOp =
                sourceOpAddr.getFirstOperand();
            OperandType addrFirstOpType = addrFirstOp->getType();
            std::shared_ptr<Operand> addrSecondOp =
                sourceOpAddr.getSecondOperand();
            if (addrFirstOpType == OperandType::Immediate) {
                llvm::Type *type = llvm::Type::getInt32Ty(
                    *PtxToLlvmIrConverter::Context
                );

                double addrFirstOpValue = std::get<double>(
                    addrFirstOp->getValue()
                );
                llvm::Value* addrFirstOperandValue = llvm::ConstantInt::get(
                    type,
                    addrFirstOpValue,
                    false
                );
            }
            else if (addrFirstOpType == OperandType::Register) {
                std::string regName = std::get<std::string>(
                    addrFirstOp->getValue()
                );

                llvm::Value* addrFirstOpValue = GetLlvmRegisterValue(regName);
                llvm::Value* addrValue = addrFirstOpValue;

                if (addrSecondOp != nullptr) {
                    // address is the result of an operation, add an instruction
                    // with the operation and pass the result to the load
                    // instruction

                    OperandType addrSecondOpType = addrSecondOp->getType();

                    llvm::Type *type = llvm::Type::getInt32Ty(
                        *PtxToLlvmIrConverter::Context
                    );

                    double addrSecondOpValue = std::get<double>(
                        addrSecondOp->getValue()
                    );
                    llvm::Value* addrSecondOperandValue = llvm::ConstantInt::get(
                        type,
                        addrSecondOpValue,
                        true
                    );

                    addrValue = PtxToLlvmIrConverter::Builder->CreateAdd(
                        addrFirstOpValue,
                        addrSecondOperandValue
                    );

                    genLlvmInstructions.push_back(
                        std::pair<llvm::Value*, llvm::BasicBlock*>(
                            addrValue, currBlock)
                    );
                }

                llvm::Type* destType = PtxToLlvmIrConverter::GetTypeMapping(
                    Types[0]
                )(*PtxToLlvmIrConverter::Context);
                llvm::Value* ld = PtxToLlvmIrConverter::Builder->CreateLoad(
                    destType,
                    addrValue
                );

                genLlvmInstructions.push_back(
                    std::pair<llvm::Value*, llvm::BasicBlock*>(ld, currBlock)
                );
            }
        }
    }
    else if (Inst == "st") {
        auto destOpAddr = std::get<AddressExpr>(DestOps[0]->getValue());
        std::string sourceOpName = std::get<std::string>(SourceOps[0]->getValue());
        llvm::Value* sourceOpValue = GetLlvmRegisterValue(sourceOpName);

        std::shared_ptr<Operand> addrFirstOp =
            destOpAddr.getFirstOperand();
        OperandType addrFirstOpType = addrFirstOp->getType();
        std::shared_ptr<Operand> addrSecondOp =
            destOpAddr.getSecondOperand();
        if (addrFirstOpType == OperandType::Immediate) {
            llvm::Type *type = llvm::Type::getInt32Ty(
                *PtxToLlvmIrConverter::Context
            );

            double addrFirstOpValue = std::get<double>(
                addrFirstOp->getValue()
            );
            llvm::Value* addrFirstOperandValue = llvm::ConstantInt::get(
                type,
                addrFirstOpValue,
                false
            );
        }
        else if (addrFirstOpType == OperandType::Register) {
            std::string regName = std::get<std::string>(
                addrFirstOp->getValue()
            );

            llvm::Value* addrFirstOpValue = GetLlvmRegisterValue(regName);;
            llvm::Value* exprValue = addrFirstOpValue;

            if (addrSecondOp != nullptr) {
                OperandType addrSecondOpType = addrSecondOp->getType();
                // TODO
                llvm::Type *type = llvm::Type::getInt32Ty(
                    *PtxToLlvmIrConverter::Context
                );

                double addrSecondOpValue = std::get<double>(
                    addrSecondOp->getValue()
                );
                llvm::Value* addrSecondOperandValue = llvm::ConstantInt::get(
                    type,
                    addrSecondOpValue,
                    true
                );
            }

            if (sourceOpValue == nullptr || exprValue == nullptr)
                return;

            llvm::Value* st = PtxToLlvmIrConverter::Builder->CreateStore(
                sourceOpValue,
                exprValue
            );

            llvm::BasicBlock* currBlock =
                PtxToLlvmIrConverter::Builder->GetInsertBlock();
            genLlvmInstructions.push_back(
                std::pair<llvm::Value*, llvm::BasicBlock*>(st, currBlock)
            );
        }
    }
    else if (Inst == "cvta") {
        // std::string regName = std::get<std::string>(SourceOps[0]->getValue());
        // llvm::Value* op = GetLlvmRegisterValue(regName);

        // llvm::Value* load = PtxToLlvmIrConverter::Builder->CreateLoad(
        //     op->getType(),
        //     op
        // );

        // genLlvmInstructions.push_back(load);
    }
    else if (Inst == "mov") {
        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        OperandType firstOpType = SourceOps[0]->getType();
        if (firstOpType == OperandType::Register) {
            std::string firstOpValue = std::get<std::string>(
                SourceOps[0]->getValue()
            );

            // check if the source operand is a special register
            if (firstOpValue == "%ctaid" || firstOpValue == "%ntid" || firstOpValue == "%tid") {
                // generate call to intrinsic
                llvm::Type *funcType = PtxToLlvmIrConverter::GetTypeMapping(
                    Types[0]
                )(*PtxToLlvmIrConverter::Context);

                llvm::FunctionType *ft = llvm::FunctionType::get(
                    funcType,
                    false
                );
                std::string funcName = "llvm.nvvm.read.ptx.sreg."
                                        +firstOpValue.erase(0,1)+"."
                                        +SourceOps[0]->getDimension();

                llvm::FunctionCallee func =
                    PtxToLlvmIrConverter::Module->getOrInsertFunction(funcName, ft);
                llvm::Value *call =
                    PtxToLlvmIrConverter::Builder->CreateCall(func);

                genLlvmInstructions.push_back(
                    std::pair<llvm::Value*, llvm::BasicBlock*>(call, currBlock)
                );
            }
        }
        else if (firstOpType == OperandType::Immediate) {
            // initialization instruction, create an add with zero instead
            double secondOp = std::get<double>(SourceOps[0]->getValue());

            llvm::Value* firstOpValue = GetLlvmOperandValue(SourceOps[0]);
            llvm::Type* firstOpValueType = firstOpValue->getType();
            llvm::Constant* zeroValue;

            if (firstOpValueType->isIntegerTy()) {
                zeroValue = llvm::ConstantInt::get(
                    firstOpValueType, 0, false
                );
            }
            else {
                zeroValue = llvm::ConstantFP::get(
                    firstOpValueType, 0
                );
            }

            llvm::Value* add = PtxToLlvmIrConverter::Builder->CreateAdd(
                zeroValue,
                firstOpValue
            );

            genLlvmInstructions.push_back(
                std::pair<llvm::Value*, llvm::BasicBlock*>(add, currBlock)
            );
        }
    }
    else if (Inst == "add") {
        // check previous instructions for patterns
        // unsigned int currInstIndex = 0;
        // for (auto stmt : currKernelStatements) {
        //     if (stmt->getId() == getId())
        //         break;
        //     currInstIndex++;
        // }

        // const InstrStatement* prevStmt =
        //     dynamic_cast<const InstrStatement*>(
        //         currKernelStatements[currInstIndex-1].get()
        //     );

        // const InstrStatement* prevPrevStmt =
        //     dynamic_cast<const InstrStatement*>(
        //         currKernelStatements[currInstIndex-2].get()
        //     );

        // bool isPrevMul = prevStmt && prevStmt->getInst() == "mul";
        // bool isPrevCvta = prevStmt && prevStmt->getInst() == "cvta";
        // bool isPrevPrevCvta = prevPrevStmt && prevPrevStmt->getInst() == "cvta";
        // bool cvtaContainsToGlobal = false;

        // std::vector<std::string> cvtaStmtMods;
        // if (isPrevPrevCvta)
        //     cvtaStmtMods = prevPrevStmt->getModifiers();
        // else if (isPrevCvta)
        //     cvtaStmtMods = prevStmt->getModifiers();

        // if (isPrevPrevCvta || isPrevCvta) {
        //     cvtaContainsToGlobal = std::find(
        //         cvtaStmtMods.begin(),
        //         cvtaStmtMods.end(),
        //         "to.global"
        //     ) != cvtaStmtMods.end();
        // }

        // // check for getelementptr pattern
        // if (
        //     (isPrevMul && isPrevPrevCvta && cvtaContainsToGlobal) ||
        //     (isPrevCvta && cvtaContainsToGlobal)
        // ) {
        //     // remove possible mul instruction before this
        //     if (isPrevMul) {
        //         std::vector<llvm::Value*> mappedLlvmInsts = 
        //             PtxToLlvmIrConverter::getPtxToLlvmMapValue(
        //                 prevStmt->getId()
        //             );
        //         llvm::Value* mulValue = mappedLlvmInsts[0];
        //         if (mappedLlvmInsts.size() == 1) {
        //             PtxToLlvmIrConverter::removePtxToLlvmMapValue(
        //                 prevStmt->getId()
        //             );
        //         }
        //         llvm::Instruction* mul = llvm::cast<llvm::Instruction>(
        //             mulValue
        //         );
        //         mul->eraseFromParent();
        //         // also remove it from mapped instructions
        //         genLlvmInstructions.erase(
        //             std::remove_if(
        //                 genLlvmInstructions.begin(),
        //                 genLlvmInstructions.end(),
        //                 [](llvm::Value* value) {
        //                     llvm::Instruction* inst =
        //                         llvm::cast<llvm::Instruction>(value);

        //                     if (inst && (inst->getOpcodeName() == "mul"))
        //                         return true;
        //                     return false;
        //                 }
        //             ),
        //             genLlvmInstructions.end()
        //         );
        //     }

        //     std::string ptrName = std::get<std::string>(
        //         SourceOps[0]->getValue()
        //     );
        //     llvm::Value* ptr = GetLlvmRegisterValue(ptrName, true);

        //     auto indexValue = SourceOps[1]->getValue();
        //     std::string indexRegName = std::get<std::string>(indexValue);
        //     llvm::Value* index = GetLlvmRegisterValue(indexRegName, true);

        //     llvm::Type* type = llvm::Type::getInt32Ty(
        //         *PtxToLlvmIrConverter::Context
        //     );
        //     llvm::Value* indexList[] = { index };
        //     llvm::Value* gep = PtxToLlvmIrConverter::Builder->CreateInBoundsGEP(
        //         type,
        //         ptr,
        //         llvm::ArrayRef<llvm::Value*>(indexList, 1)
        //     );

        //     genLlvmInstructions.push_back(gep);
        // }
        // else {
        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();

        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        if (firstOperandValue == nullptr || secondOperandValue == nullptr)
            return;

        // Check if destination and source registers are the same (possible
        // induction variable), if yes create phi node in the beginning of this
        // basic block
        llvm::Value* add = nullptr;
        std::string firstSourceOpName = std::get<std::string>(
            SourceOps[0]->getValue()
        );
        std::string destName = std::get<std::string>(DestOps[0]->getValue());
        if (destName == firstSourceOpName) {
            llvm::BasicBlock* currBasicBlock
                = PtxToLlvmIrConverter::Builder->GetInsertBlock();

            llvm::PHINode* phi = CreatePhiInBlockStart(
                firstOperandValue,
                currBasicBlock
            );
            // phi->print(llvm::outs());
            // llvm::outs() << "\n";

            add = PtxToLlvmIrConverter::Builder->CreateAdd(
                phi,
                secondOperandValue
            );
            phi->addIncoming(add, currBasicBlock);
        }
        else {
            add = PtxToLlvmIrConverter::Builder->CreateAdd(
                firstOperandValue,
                secondOperandValue
            );
        }

        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(add, currBlock)
        );

        // check operands for global variables and update global variable
        // if found
        InstrStatement* currInst = this;
        while (
            (currInst != nullptr) && 
            (currInst->getInst() == "add") || (currInst->getInst() == "cvta")
        ) {
            OperandType sourceOpType = currInst->getSourceOps()[0]->getType();

            if (sourceOpType == OperandType::Register) {
                // find when it was written
                currInst = GetOperandWriteInstructions(currInst, 0).back();
            }
            else break;
        }

        if (currInst && currInst->Inst == "mov") {
            OperandType sourceOpType = currInst->getSourceOps()[0]->getType();
            if (sourceOpType == OperandType::Label) {
                std::string sourceOpName = std::get<std::string>(
                    currInst->getSourceOps()[0]->getValue()
                );

                // find var used in mov instruction
                DirectStatement* varStmt = GetVar(sourceOpName);
                if (varStmt != nullptr) {
                    int varSize = 0;
                    if (
                        LinkingDirectStatement* globVar =
                            dynamic_cast<LinkingDirectStatement*>(varStmt)
                    ) {
                        varSize = globVar->getSize();
                    }
                    else if (
                        VarDecDirectStatement* var =
                            dynamic_cast<VarDecDirectStatement*>(varStmt)
                    ) {
                        varSize = var->getSize();
                    }

                    // get previous instruction of this instruction (add)
                    // std::shared_ptr<Statement> prevStmt = GetStatementById(
                    //     getId() - 1
                    // );
                    // InstrStatement* prevInst =
                    //     dynamic_cast<InstrStatement*>(prevStmt.get());

                    // find where the 2nd source operand of this instruction
                    // (add) was written and if it's a mul or shl instruction
                    // get the immediate value
                    InstrStatement* writeInst = GetOperandWriteInstructions(
                        this, 1
                    ).back();

                    if (
                        (writeInst != nullptr) &&
                        ((writeInst->getInst() == "mul") || (writeInst->getInst() == "shl"))
                    ) {
                        // get second operand and get value if immediate
                        OperandType writeInstType =
                            writeInst->getSourceOps()[1]->getType();
                        
                        if (writeInstType == OperandType::Immediate) {
                            int writeInstValue = std::get<double>(
                                writeInst->getSourceOps()[1]->getValue()
                            );

                            // based on this value update the type of the globar var
                            // in the IR instruction
                            llvm::Value* llvmValue =
                                PtxToLlvmIrConverter::getPtxToLlvmMapValue(
                                    currInst->getId()
                                )[0].first;

                            llvm::GlobalValue* globVarLlvmValue =
                                llvm::cast<llvm::GlobalValue>(llvmValue);

                            llvm::Type* valueType = globVarLlvmValue->getValueType();
                            uint numElements = valueType->getArrayNumElements();
                            llvm::Type* elemType = valueType->getArrayElementType();
                            
                            uint newElemTypeSize;
                            if (writeInst->getInst() == "mul")
                                newElemTypeSize = writeInstValue;
                            else
                                newElemTypeSize = 1 << writeInstValue;

                            uint newElemTypeSizeBits = newElemTypeSize * 8;

                            llvm::ArrayType* newType;
                            if (elemType->getTypeID() == llvm::Type::VoidTyID) {
                                newType = llvm::ArrayType::get(
                                    PtxToLlvmIrConverter::Builder->getIntNTy(
                                        newElemTypeSizeBits
                                    ),
                                    numElements
                                );
                            }
                            else {
                                llvm::TypeSize elemTypeSize =
                                PtxToLlvmIrConverter::Module->getDataLayout()
                                    .getTypeAllocSize(elemType);
                                uint64_t elemTypeSizeInt = elemTypeSize.getFixedSize();
                                uint rowSize = elemTypeSizeInt / newElemTypeSize;
                                uint numOfRows = numElements / (rowSize * newElemTypeSize);

                                if (numOfRows != 0) {
                                    newType = llvm::ArrayType::get(
                                        llvm::ArrayType::get(
                                            llvm::Type::getIntNTy(
                                                *PtxToLlvmIrConverter::Context,
                                                newElemTypeSizeBits
                                            ),
                                            rowSize
                                        ),
                                        numOfRows
                                    );
                                }

                            }

                            // newType->print(llvm::outs(), true);

                            if (
                                newType && 
                                (newType != globVarLlvmValue->getValueType())
                            ) {
                                llvm::GlobalValue::LinkageTypes
                                    globVarLinkage = globVarLlvmValue->getLinkage();
                                llvm::StringRef globVarName =
                                    globVarLlvmValue->getName();
                                llvm::GlobalValue::ThreadLocalMode globVartlm =
                                        globVarLlvmValue->getThreadLocalMode();
                                uint globVarAddrSpace =
                                    globVarLlvmValue->getAddressSpace();

                                int alignment;
                                if (
                                    llvm::GlobalVariable* globVar =
                                        llvm::dyn_cast<llvm::GlobalVariable>(globVarLlvmValue)
                                ) {
                                    alignment = globVar->getAlignment();
                                }

                                // globVarLlvmValue->replaceAllUsesWith(
                                //     newGlobalVarValue
                                // );
                                globVarLlvmValue->eraseFromParent();

                                llvm::GlobalVariable* newGlobalVarValue =
                                    new llvm::GlobalVariable(
                                        *PtxToLlvmIrConverter::Module,
                                        newType,
                                        false,
                                        globVarLinkage,
                                        llvm::Constant::getNullValue(newType),
                                        globVarName,
                                        nullptr,
                                        globVartlm,
                                        globVarAddrSpace
                                    );
                                newGlobalVarValue->setAlignment(
                                    llvm::MaybeAlign(alignment)
                                );

                                std::vector<std::pair<llvm::Value*,
                                                      llvm::BasicBlock*>>
                                    newLlvmInstValueMap;
                                std::pair<llvm::Value *, llvm::BasicBlock*>
                                    valueBlockPair(
                                        newGlobalVarValue,
                                        currBlock
                                    );
                                newLlvmInstValueMap.push_back(valueBlockPair);
                                PtxToLlvmIrConverter::setPtxToLlvmMapValue(
                                    currInst->getId(),
                                    newLlvmInstValueMap
                                );
                            }
                        }
                    }
                }
            }
        }
    }
    else if (Inst == "sub") {
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        if (firstOperandValue == nullptr || secondOperandValue == nullptr)
            return;

        // Create phi node for possible induction variable, if the source
        // and dest registers are the same
        llvm::Value* sub = nullptr;
        std::string firstSourceOpName = std::get<std::string>(
            SourceOps[0]->getValue()
        );
        std::string destName = std::get<std::string>(DestOps[0]->getValue());
        if (destName == firstSourceOpName) {
            llvm::BasicBlock* currBasicBlock
                = PtxToLlvmIrConverter::Builder->GetInsertBlock();

            llvm::PHINode* phi = CreatePhiInBlockStart(
                firstOperandValue,
                currBasicBlock
            );

            sub = PtxToLlvmIrConverter::Builder->CreateSub(
                phi,
                secondOperandValue
            );
            phi->addIncoming(sub, currBasicBlock);
        }
        else {
            sub = PtxToLlvmIrConverter::Builder->CreateSub(
                firstOperandValue,
                secondOperandValue
            );
        }

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(sub, currBlock)
        );
    }
    else if (Inst == "mul") {
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        if (firstOperandValue == nullptr || secondOperandValue == nullptr)
            return;

        // Create phi node for possible induction variable, if the source
        // and dest registers are the same
        llvm::Value* mul = nullptr;
        std::string firstSourceOpName = std::get<std::string>(
            SourceOps[0]->getValue()
        );
        std::string destName = std::get<std::string>(DestOps[0]->getValue());
        if (destName == firstSourceOpName) {
            llvm::BasicBlock* currBasicBlock
                = PtxToLlvmIrConverter::Builder->GetInsertBlock();

            llvm::PHINode* phi = CreatePhiInBlockStart(
                firstOperandValue,
                currBasicBlock
            );

            mul = PtxToLlvmIrConverter::Builder->CreateMul(
                phi,
                secondOperandValue
            );
            phi->addIncoming(mul, currBasicBlock);
        }
        else {
            mul = PtxToLlvmIrConverter::Builder->CreateMul(
                firstOperandValue,
                secondOperandValue
            );
        }

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(mul, currBlock)
        );
    }
    else if (Inst == "shl") {
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        if (firstOperandValue == nullptr || secondOperandValue == nullptr)
            return;

        llvm::Value* shl = PtxToLlvmIrConverter::Builder->CreateShl(
            firstOperandValue,
            secondOperandValue
        );

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(shl, currBlock)
        );
    }
    else if (Inst == "and") {
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        if (firstOperandValue == nullptr || secondOperandValue == nullptr)
            return;

        llvm::Value* andInst = PtxToLlvmIrConverter::Builder->CreateAnd(
            firstOperandValue,
            secondOperandValue
        );

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(andInst, currBlock)
        );
    }
    else if (Inst == "or") {
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        if (firstOperandValue == nullptr || secondOperandValue == nullptr)
            return;

        llvm::Value* orInst = PtxToLlvmIrConverter::Builder->CreateOr(
            firstOperandValue,
            secondOperandValue
        );

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(orInst, currBlock)
        );
    }
    else if (Inst == "mad") {
        
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        if (firstOperandValue == nullptr || secondOperandValue == nullptr)
            return;

        llvm::Value* mul = PtxToLlvmIrConverter::Builder->CreateMul(
            firstOperandValue,
            secondOperandValue
        );

        llvm::Value* thirdOperandValue = GetLlvmOperandValue(SourceOps[2]);

        if (thirdOperandValue == nullptr) return;

        llvm::Value* add = PtxToLlvmIrConverter::Builder->CreateAdd(
            mul,
            thirdOperandValue
        );

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(mul, currBlock)
        );
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(add, currBlock)
        );
    }
    else if (Inst == "fma") {
        
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        if (firstOperandValue == nullptr || secondOperandValue == nullptr)
            return;

        llvm::FastMathFlags fmf;
        fmf.setAllowContract();
        llvm::Value* fmul = PtxToLlvmIrConverter::Builder->CreateFMul(
            firstOperandValue,
            secondOperandValue
        );
        llvm::cast<llvm::Instruction>(fmul)->setFastMathFlags(fmf);

        llvm::Value* thirdOperandValue = GetLlvmOperandValue(SourceOps[2]);

        if (thirdOperandValue == nullptr) return;

        llvm::Value* fadd = PtxToLlvmIrConverter::Builder->CreateFAdd(
            fmul,
            thirdOperandValue
        );
        llvm::cast<llvm::Instruction>(fadd)->setFastMathFlags(fmf);

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(fmul, currBlock)
        );
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(fadd, currBlock)
        );
    }
    else if (Inst == "setp") {
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        if (firstOperandValue == nullptr || secondOperandValue == nullptr)
            return;

        // get comparison operation
        llvm::ICmpInst::Predicate llvmPred =
            PtxToLlvmIrConverter::ConvertPtxToLlvmPred(Modifiers[0]);
        

        llvm::Value* icmp = PtxToLlvmIrConverter::Builder->CreateICmp(
            llvmPred,
            firstOperandValue,
            secondOperandValue
        );

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(icmp, currBlock)
        );

    }
    else if (Inst == "bra") {
        // remove @ from pred
        Pred = Pred.erase(0,1);

        llvm::Value* cond = GetLlvmRegisterValue(Pred);

        // get name of current kernel, in order to add
        // the block targets of the branch to this kernel
        std::unique_ptr currKernel = GetCurrentKernel();
        std::string currKernelName = currKernel->getName();
        llvm::Function* kernelFunc = PtxToLlvmIrConverter::Module->getFunction(
            currKernelName
        );

        // create the false block and move it after the current block
        llvm::BasicBlock* currBasicBlock
            = PtxToLlvmIrConverter::Builder->GetInsertBlock();

        llvm::BasicBlock* falseBlock = llvm::BasicBlock::Create(
            *PtxToLlvmIrConverter::Context,
            "",
            PtxToLlvmIrConverter::Module->getFunction(currKernelName)
        );

        falseBlock->moveAfter(currBasicBlock);

        std::string targetValue = std::get<std::string>(
            DestOps[0]->getValue()
        );

        // check if label exists before the current instruction
        bool labelExistsBeforeInst = false;
        for (const auto stmt : statements) {
            if (stmt->getId() >= getId()) break;
            if (targetValue == stmt->getLabel())
                labelExistsBeforeInst = true;
        }

        // if it exists, find the basic block that was created,
        // when converting that instruction, else create a new one
        llvm::BasicBlock* trueBlock = PtxToLlvmIrConverter::GetBasicBlock(
            kernelFunc,
            targetValue
        );
        
        if (trueBlock == nullptr) {
            // find the next basic block to insert the new one before it
            llvm::BasicBlock *nextBasicBlock = nullptr;
            int minIndex = 9999999;
            for (llvm::BasicBlock &bb : *kernelFunc) {
                std::string bbName = bb.getName().str();
                if (bbName == "") continue;
                std::string bbIndexStr = bbName.substr(
                    bbName.find_last_of('_'),
                    bbName.length()
                );
                
                std::string newBbIndexStr = targetValue.substr(
                    targetValue.find_last_of('_'),
                    targetValue.length()
                );

                int newBbIndex = stoi(newBbIndexStr.erase(0, 1));
                int bbIndex = stoi(bbIndexStr.erase(0, 1));
                
                if ((bbIndex > newBbIndex) && (bbIndex < minIndex)) {
                    minIndex = bbIndex;
                    nextBasicBlock = &bb;
                }
            }

            trueBlock = llvm::BasicBlock::Create(
                *PtxToLlvmIrConverter::Context,
                targetValue,
                kernelFunc,
                nextBasicBlock
            );
        }

        // set target as null for now, need to patch it later
        llvm::Value* br = PtxToLlvmIrConverter::Builder->CreateCondBr(
            cond,
            trueBlock,
            falseBlock
        );

        PtxToLlvmIrConverter::Builder->SetInsertPoint(falseBlock);

        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(br, currBasicBlock)
        );
    }
    else if (Inst == "ret") {
        llvm::Value* ret = PtxToLlvmIrConverter::Builder->CreateRetVoid();
        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(ret, currBlock)
        );
    }
    else {
        llvm::outs() << "\033[1;31m"+Inst+" not handled!\033[0m\n";
    }

    // check if instruction contains .wide modifier
    auto wideIter = std::find(Modifiers.begin(), Modifiers.end(), "wide");
    if (wideIter != Modifiers.end()) {
        std::string type = Types[0];
        // std::string regName = std::get<std::string>(SourceOps[0]->getValue());
        // llvm::Value* operand = GetLlvmRegisterValue(regName);
        std::string typeBitsStr = std::regex_replace(
            type,
            std::regex("[a-z]+"),
            std::string("$1")
        );
        int typeBits = stoi(typeBitsStr);
        std::string extTypeStr = type[0] + std::to_string(2 * typeBits);
        llvm::Type* extType = PtxToLlvmIrConverter::GetTypeMapping(
            extTypeStr
        )(*PtxToLlvmIrConverter::Context);

        llvm::Value* lastInst = genLlvmInstructions.back().first;
        // if instruction type is signed, create sext
        llvm::Value* ext;
        if (type[0] == 's') {
            ext = PtxToLlvmIrConverter::Builder->CreateSExt(
                lastInst,
                extType
            );
        }
        else {
            ext = PtxToLlvmIrConverter::Builder->CreateZExt(
                lastInst,
                extType
            );
        }

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(ext, currBlock)
        );
    }

    // Check source operand. If it's a label check if it's a variable
    if (
        (SourceOps.size() > 0) &&
        (SourceOps[0] != nullptr) && 
        (SourceOps[0]->getType() == OperandType::Label)
    ) {
        std::string value = std::get<std::string>(
            SourceOps[0]->getValue()
        );

        DirectStatement* varStmt = GetVar(value);
    
        // Check if global variable already exists. If yes, do nothing
        llvm::GlobalVariable* globVar =
            PtxToLlvmIrConverter::Module->getGlobalVariable(value);

        // If not a variable, return
        if (varStmt && (globVar == nullptr)) {

            std::string ptxAddrSpace;
            int globVarSize;
            int alignment;
            if (
                LinkingDirectStatement* linkStmt =
                    dynamic_cast<LinkingDirectStatement*>(varStmt)
            ) {
                ptxAddrSpace = linkStmt->getAddressSpace();
                globVarSize = linkStmt->getSize();
                globVarSize = linkStmt->getAlignment();
            }
            else if (
                VarDecDirectStatement* varDecStmt =
                    dynamic_cast<VarDecDirectStatement*>(varStmt)
            ) {
                ptxAddrSpace = varDecStmt->getAddressSpace();
                globVarSize = varDecStmt->getSize();
                alignment = varDecStmt->getAlignment();
            }

            uint addrSpace = PtxToLlvmIrConverter::ConvertPtxToLlvmAddrSpace(
                ptxAddrSpace
            );

            // set global var's type
            if (globVarSize > 0) {
                llvm::Type* globVarType = llvm::ArrayType::get(
                    PtxToLlvmIrConverter::Builder->getVoidTy(),
                    globVarSize
                );

                globVar = new llvm::GlobalVariable(
                    *PtxToLlvmIrConverter::Module,
                    globVarType,
                    false,
                    llvm::GlobalValue::LinkOnceODRLinkage,
                    llvm::Constant::getNullValue(globVarType),
                    value,
                    nullptr,
                    llvm::GlobalVariable::ThreadLocalMode::NotThreadLocal,
                    addrSpace
                );
            }
            else {
                llvm::Type* globVarType =
                    PtxToLlvmIrConverter::Builder->getVoidTy();

                globVar = new llvm::GlobalVariable(
                    *PtxToLlvmIrConverter::Module,
                    globVarType,
                    false,
                    llvm::GlobalValue::LinkOnceODRLinkage,
                    llvm::Constant::getNullValue(globVarType),
                    value,
                    nullptr,
                    llvm::GlobalVariable::ThreadLocalMode::NotThreadLocal,
                    addrSpace
                );
            }
            globVar->setAlignment(llvm::MaybeAlign(alignment));

            llvm::BasicBlock* currBlock =
                PtxToLlvmIrConverter::Builder->GetInsertBlock();
            genLlvmInstructions.push_back(
                std::pair<llvm::Value*, llvm::BasicBlock*>(globVar, currBlock)
            );
        }

        // PtxToLlvmIrConverter::Module->print(llvm::outs(), nullptr, false, true);
    }

    PtxToLlvmIrConverter::setPtxToLlvmMapValue(getId(), genLlvmInstructions);

    // for (auto stmt : statements) {

    //     KernelDirectStatement* kernelstmt;
    //     if ((kernelstmt = dynamic_cast<KernelDirectStatement*>(stmt.get())) != nullptr)
    //     {
    //         for (auto bodystmt : kernelstmt->getBodyStatements()) {
    //             std::vector<llvm::Value*> kernelvalues = PtxToLlvmIrConverter::getPtxToLlvmMapValue(bodystmt->getId());
    //             if (kernelvalues.size() > 0) {
    //                 kernelvalues[0]->print(llvm::outs(), true);
    //                 std::cout << std::endl;
    //             }
    //         }
    //     }

    //     std::vector<llvm::Value*> values = PtxToLlvmIrConverter::getPtxToLlvmMapValue(stmt->getId());
    //     if (values.size() > 0) {
    //         values[0]->print(llvm::outs(), true);
    //         std::cout << std::endl;
    //     }
    // }
    
}

void InstrStatement::dump() const {
    const int colWidth = 20;

    std::cout << std::left << std::setw(colWidth) << "Label: " + getLabel()
              << std::setw(colWidth-8) << "Pred: " + Pred;

    if (isatty(fileno(stdout)))
        std::cout << std::setw(colWidth + 5) << "Instr: \033[33m" + Inst + "\033[0m";
    else
        std::cout << std::setw(colWidth + 5) << "Instr: " + Inst;
              
    std::cout << "Mods: ";

    int index = 0;
    std::string output = "";
    for (std::string modifier : Modifiers) {
        output += modifier;
        index++;
        if (index < Modifiers.size()) output += ", ";
    }

    std::cout << std::left << std::setw(colWidth) << output;

    std::cout << "Types: ";

    index = 0;
    output = "";
    for (std::string type : Types) {
        output += type;
        index++;
        if (index < Types.size()) output += ", ";
    }

    std::cout << std::left << std::setw(colWidth-5) << output;

    std::cout << "D.Ops: ";
    index = 0;
    output = "";
    for (auto const& dest : DestOps) {
        // if (const auto strPtr (std::get_if<std::string>(&dest)); strPtr) 
        //     output += *strPtr;
        // else if (const auto doublePtr (std::get_if<double>(&dest)); doublePtr) 
        //     output += std::to_string(*doublePtr);
        output += dest->ToString() + " (" + operandTypeToString(dest->getType()) + ")";
        // output += dest;
        index++;
        if (index < DestOps.size()) output += ", ";
    }

    std::cout << std::left << std::setw(colWidth-5) << output;

    std::cout << "S.Ops: ";
    index = 0;
    output = "";
    for (auto const& source : SourceOps) {
        // if (const auto strPtr (std::get_if<std::string>(&source)); strPtr) 
        //     output += *strPtr;
        // else if (const auto doublePtr (std::get_if<double>(&source)); doublePtr) 
        //     output += std::to_string(*doublePtr);
        output += source->ToString();
        if (source->getDimension() != "") output += "->" + source->getDimension();
        output +=  " (" + operandTypeToString(source->getType()) + ")";
        // output += source;
        index++;
        if (index < SourceOps.size()) output += ", ";
    }

    std::cout << std::left << std::setw(colWidth) << output;
}