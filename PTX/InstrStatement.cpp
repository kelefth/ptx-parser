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

#include "llvm/IR/InstIterator.h"

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

llvm::Value* InstrStatement::GetLlvmRegisterValue(
    std::string ptxOperandName,
    bool isComplex = true
) {
    std::unique_ptr<KernelDirectStatement> currKernel = GetCurrentKernel();
    if (currKernel == nullptr) return nullptr;

    // find the last statement before the current
    // that the operand has been used as destination
    int userStmtId = -1;
    for (const auto stmt : currKernel->getBodyStatements()) {
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

                if (destOpValue == ptxOperandName) {
                    userStmtId = stmtId;
                    break;
                }
            }
        }
    }

    // if not found, return
    if (userStmtId == -1) return nullptr;

    std::vector<llvm::Value*> llvmStmts =
        PtxToLlvmIrConverter::getPtxToLlvmMapValue(userStmtId);

    if (llvmStmts.empty() && isComplex) {
        std::shared_ptr<Statement> userStmt = GetStatementById(userStmtId);
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

    // generate a load and get its value if the last generate instruction is a
    // store, else get the value of the last generated llvm instruction
    // that returns a value
    llvm::Value* lastLlvmStmt = llvmStmts.back();
    llvm::Instruction* lastLlvmInst = llvm::cast<llvm::Instruction>(
        lastLlvmStmt
    );
    std::string lastLlvmStmtName = lastLlvmInst->getOpcodeName();
    llvm::Value* instValue = nullptr;
    if (lastLlvmStmtName == "store") {
        llvm::Value* firstOperand = lastLlvmInst->getOperand(0);
        llvm::Value* secondOperand = lastLlvmInst->getOperand(1);
        llvm::Type* loadType = firstOperand->getType();
        instValue = PtxToLlvmIrConverter::Builder->CreateLoad(
            loadType,
            secondOperand
        );
    }
    else {
        for (auto it = llvmStmts.rbegin(); it != llvmStmts.rend(); ++it) {
            if ((*it)->getType()->getTypeID() != llvm::Type::VoidTyID) {
                instValue = *it;
                break;
            }
        }
    }

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
        operandLlvmValue = GetImmediateValue(value);
    }

    return operandLlvmValue;
}

llvm::Constant* InstrStatement::GetImmediateValue(double value) {
    bool isSigned = false;
    // if signed
    if (Types[0][0] == 's')
        isSigned = true;

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

LinkingDirectStatement* InstrStatement::GetGlobalVar(std::string name) {
    std::unique_ptr<KernelDirectStatement> currKernel = GetCurrentKernel();
    LinkingDirectStatement* globVarStmt = nullptr;
    for (auto stmt : statements) {
        if (stmt->getId() > currKernel->getId()) break;
        LinkingDirectStatement* linkingDir =
            dynamic_cast<LinkingDirectStatement*>(stmt.get());
        if (linkingDir != nullptr) {
            if (linkingDir->getIdentifier() == name)
                globVarStmt = linkingDir;
        }
    }

    return globVarStmt;
}

InstrStatement* InstrStatement::GetOperandWriteInstruction(
    InstrStatement* inst,
    uint sourceOpNum
) {
    std::unique_ptr currKernel = GetCurrentKernel();
    std::vector<std::shared_ptr<Statement>> currKernelStatements =
        currKernel->getBodyStatements();

    if (sourceOpNum > inst->getSourceOps().size() - 1) return nullptr;

    auto opValue = inst->getSourceOps()[sourceOpNum]->getValue();
    std::string sourceOpName = std::get<std::string>(opValue);

    std::shared_ptr<Statement> userStmt = nullptr;
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
                    userStmt = stmt;
                }
            }
        }
    }

    InstrStatement* userInst = dynamic_cast<InstrStatement*>(userStmt.get());
    return userInst;
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
    std::vector<llvm::Value*> genLlvmInstructions;

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

        // if statement found
        if (userStmtId != -1) {
            llvm::Value* llvmStmt =
                PtxToLlvmIrConverter::getPtxToLlvmMapValue(userStmtId)[0];

            llvm::Instruction* braInst = llvm::cast<llvm::Instruction>(
                llvmStmt
            );

            llvm::BasicBlock* trueBranch = braInst->getSuccessor(0);
            PtxToLlvmIrConverter::Builder->SetInsertPoint(trueBranch);
        }
        else {
            std::string currKernelName = currKernel->getName();
            llvm::Function* currFunction =
                PtxToLlvmIrConverter::Module->getFunction(currKernelName);

            // check if basic block already exists
            llvm::BasicBlock* basicBlock = PtxToLlvmIrConverter::GetBasicBlock(
                currFunction,
                label
            );

            // if it doesn't exist, create a new one
            if (basicBlock == nullptr) {
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

                basicBlock = llvm::BasicBlock::Create(
                    *PtxToLlvmIrConverter::Context,
                    label,
                    currFunction,
                    nextBasicBlock
                );
            }

            PtxToLlvmIrConverter::Builder->SetInsertPoint(basicBlock);
        }

    }

    if (Inst == "ld") {
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

                    // Store generated instructions
                    genLlvmInstructions.push_back(alloca);
                    genLlvmInstructions.push_back(store);
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

                llvm::Type* destType = PtxToLlvmIrConverter::GetTypeMapping(
                    Types[0]
                )(*PtxToLlvmIrConverter::Context);
                llvm::Value* ld = PtxToLlvmIrConverter::Builder->CreateLoad(
                    destType,
                    addrFirstOpValue
                );

                genLlvmInstructions.push_back(ld);
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

            genLlvmInstructions.push_back(st);
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
        if (SourceOps[0]->getType() == OperandType::Register) {
            std::string value = std::get<std::string>(
                SourceOps[0]->getValue()
            );

            // check if the source operand is a special register
            if (value == "%ctaid" || value == "%ntid" || value == "%tid") {
                // generate call to intrinsic
                llvm::Type *funcType = PtxToLlvmIrConverter::GetTypeMapping(
                    Types[0]
                )(*PtxToLlvmIrConverter::Context);

                llvm::FunctionType *ft = llvm::FunctionType::get(
                    funcType,
                    false
                );
                std::string funcName = "llvm.nvvm.read.ptx.sreg."
                                        +value.erase(0,1)+"."
                                        +SourceOps[0]->getDimension();

                llvm::FunctionCallee func =
                    PtxToLlvmIrConverter::Module->getOrInsertFunction(funcName, ft);
                llvm::Value *call =
                    PtxToLlvmIrConverter::Builder->CreateCall(func);

                genLlvmInstructions.push_back(call);
            }
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

        genLlvmInstructions.push_back(add);

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
                currInst = GetOperandWriteInstruction(currInst, 0);
            }
            else break;
        }

        if (currInst == nullptr) return;

        if (currInst->Inst == "mov") {
            OperandType sourceOpType = currInst->getSourceOps()[0]->getType();
            if (sourceOpType == OperandType::Label) {
                std::string sourceOpName = std::get<std::string>(
                    currInst->getSourceOps()[0]->getValue()
                );

                // find global var used in mov instruction
                LinkingDirectStatement* globVar = GetGlobalVar(sourceOpName);
                if (globVar == nullptr) return;

                int globVarSize = globVar->getSize();

                // get previous instruction of this instruction (add)
                // std::shared_ptr<Statement> prevStmt = GetStatementById(
                //     getId() - 1
                // );
                // InstrStatement* prevInst =
                //     dynamic_cast<InstrStatement*>(prevStmt.get());

                // find where the 2nd source operand of this instruction
                // (add) was written and if it's a mul or shl instruction
                // get the immediate value
                InstrStatement* writeInst = GetOperandWriteInstruction(
                    this, 1
                );

                if (
                    (writeInst == nullptr) ||
                    ((writeInst->getInst() != "mul") && (writeInst->getInst() != "shl"))
                ) return;
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
                        )[0];

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

                    // newType->print(llvm::outs(), true);

                    if (newType != globVarLlvmValue->getValueType()) {
                        // newType->print(llvm::outs());
                        // std::cout << " != ";
                        // globVarLlvmValue->getValueType()->print(llvm::outs());
                        // std::cout << std::endl;
                        llvm::GlobalValue::LinkageTypes
                            globVarLinkage = globVarLlvmValue->getLinkage();
                        llvm::StringRef globVarName =
                            globVarLlvmValue->getName();
                        llvm::GlobalValue::ThreadLocalMode globVartlm =
                                globVarLlvmValue->getThreadLocalMode();
                        uint globVarAddrSpace =
                            globVarLlvmValue->getAddressSpace();

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
                                nullptr,
                                globVarName,
                                nullptr,
                                globVartlm,
                                globVarAddrSpace
                            );

                        std::vector<llvm::Value*> newLlvmInstValueMap;
                        newLlvmInstValueMap.push_back(newGlobalVarValue);
                        PtxToLlvmIrConverter::setPtxToLlvmMapValue(
                            currInst->getId(),
                            newLlvmInstValueMap
                        );

                    }
                }
            }
        }
            
        // }
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

            sub = PtxToLlvmIrConverter::Builder->CreateAdd(
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

        genLlvmInstructions.push_back(sub);
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

            mul = PtxToLlvmIrConverter::Builder->CreateAdd(
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

        genLlvmInstructions.push_back(mul);
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

        genLlvmInstructions.push_back(shl);
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

        genLlvmInstructions.push_back(andInst);
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

        genLlvmInstructions.push_back(orInst);
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

        genLlvmInstructions.push_back(mul);
        genLlvmInstructions.push_back(add);
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

        genLlvmInstructions.push_back(icmp);

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

        genLlvmInstructions.push_back(br);
    }
    else if (Inst == "ret") {
        llvm::Value* ret = PtxToLlvmIrConverter::Builder->CreateRetVoid();
        genLlvmInstructions.push_back(ret);
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

        llvm::Value* lastInst = genLlvmInstructions.back();
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

        genLlvmInstructions.push_back(ext);
    }

    // Check source operand. If it's a label check if it's a global variable
    if (
        (SourceOps.size() > 0) &&
        (SourceOps[0] != nullptr) && 
        (SourceOps[0]->getType() == OperandType::Label)
    ) {
        std::string value = std::get<std::string>(
            SourceOps[0]->getValue()
        );

        LinkingDirectStatement* globVarStmt = GetGlobalVar(value);

        // If not a global variable, return
        if (globVarStmt == nullptr) return;

        // Check if global variable already exists. If yes, do nothing
        llvm::GlobalVariable* globVar =
            PtxToLlvmIrConverter::Module->getGlobalVariable(value);

        if (globVar != nullptr) return;

        std::string ptxAddrSpace = globVarStmt->getAddressSpace();
        uint addrSpace = PtxToLlvmIrConverter::ConvertPtxToLlvmAddrSpace(
            ptxAddrSpace
        );
        int globVarSize = globVarStmt->getSize();

        // set global var's type
        if (globVarSize > 0) {
            globVar = new llvm::GlobalVariable(
                *PtxToLlvmIrConverter::Module,
                llvm::ArrayType::get(
                    PtxToLlvmIrConverter::Builder->getVoidTy(),
                    globVarSize
                ),
                false,
                llvm::GlobalValue::LinkOnceODRLinkage,
                nullptr,
                value,
                nullptr,
                llvm::GlobalVariable::ThreadLocalMode::NotThreadLocal,
                addrSpace
            );
        }
        else {
            globVar = new llvm::GlobalVariable(
                *PtxToLlvmIrConverter::Module,
                PtxToLlvmIrConverter::Builder->getVoidTy(),
                false,
                llvm::GlobalValue::LinkOnceODRLinkage,
                nullptr,
                value,
                nullptr,
                llvm::GlobalVariable::ThreadLocalMode::NotThreadLocal,
                addrSpace
            );
        }

        genLlvmInstructions.push_back(globVar);

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