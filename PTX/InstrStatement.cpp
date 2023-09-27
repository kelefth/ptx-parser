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

llvm::Value* InstrStatement::GetLlvmOperandValue(
    std::string ptxOperandName,
    bool isComplex = false
) {
    std::unique_ptr<KernelDirectStatement> currKernel = GetCurrentKernel();
    if (currKernel == nullptr) return nullptr;

    // find the last statement before the current
    // that the operand has been used as destination
    int userStmtId = -1;
    for (const auto stmt : currKernel->getBodyStatements()) {
        unsigned int stmtId = stmt->getId();
        if (stmtId < this->getId()) {
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
                llvm::Value* value = GetLlvmOperandValue(opName);
                return value;
            }
        }
    }

    // get the value of the last of the generated llvm instructions
    // that returns a value
    llvm::Value* instValue;
    for (auto it = llvmStmts.rbegin(); it != llvmStmts.rend(); ++it) {
        if ((*it)->getType()->getTypeID() != llvm::Type::VoidTyID) {
            instValue = *it;
            break;
        }
    }

    return instValue;
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

void InstrStatement::ToLlvmIr() {
    std::vector<llvm::Value*> genLlvmInstructions;

    // if instruction has a label, change IR insert point
    if (getLabel() != "") {
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
                    (instrStatement->getDestOps()[0])
                ) {
                    destOpValue = std::get<std::string>(
                        instrStatement->getDestOps()[0]->getValue()
                    );
                }

                if (
                    instrStatement->getInst() == "bra" &&
                    destOpValue == getLabel()
                ) {
                    userStmtId = stmtId;
                    break;
                }
            }
        }

        if (userStmtId != -1) {
            llvm::Value* llvmStmt =
                PtxToLlvmIrConverter::getPtxToLlvmMapValue(userStmtId)[0];

            llvm::Instruction* braInst = llvm::cast<llvm::Instruction>(
                llvmStmt
            );

            llvm::BasicBlock* trueBranch = braInst->getSuccessor(0);
            PtxToLlvmIrConverter::Builder->SetInsertPoint(trueBranch);

            // llvm::Function* currFunc =
            //     PtxToLlvmIrConverter::Module->getFunction(currKernel->getName());

            // llvm::Instruction* instToFind;
            // for (auto it = llvm::inst_begin(currFunc), e = llvm::inst_end(currFunc); it != e; ++it) {
            //     if ((*it).isIdenticalTo(llvmStmt)) {
            //         instToFind = *it
            //     }
            // }
                

            // auto brInst = dynamic_cast<llvm::BranchInst*>(llvmStmt);
            // llvm::BasicBlock* trueBlock = brInst->getSuccesor(0);
            // PtxToLlvmIrConverter::Builder->SetInsertPoint(trueBlock);
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
            OperandType sourceOpType = SourceOps[0]->getType();

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

                llvm::Value* addrFirstOpValue = GetLlvmOperandValue(regName);

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
    else if (Inst == "cvta") {
        std::string regName = std::get<std::string>(SourceOps[0]->getValue());
        llvm::Value* op = GetLlvmOperandValue(regName);

        llvm::Value* load = PtxToLlvmIrConverter::Builder->CreateLoad(
            op->getType(),
            op
        );

        genLlvmInstructions.push_back(load);
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
        std::unique_ptr currKernel = GetCurrentKernel();
        std::vector<std::shared_ptr<Statement>> currKernelStatements =
            currKernel->getBodyStatements();

        // check previous instructions for patterns
        unsigned int currInstIndex = 0;
        for (auto stmt : currKernelStatements) {
            if (stmt->getId() == getId())
                break;
            currInstIndex++;
        }

        const InstrStatement* prevStmt =
            dynamic_cast<const InstrStatement*>(
                currKernelStatements[currInstIndex-1].get()
            );

        const InstrStatement* prevPrevStmt =
            dynamic_cast<const InstrStatement*>(
                currKernelStatements[currInstIndex-2].get()
            );

        bool isPrevMul = prevStmt && prevStmt->getInst() == "mul";
        bool isPrevCvta = prevStmt && prevStmt->getInst() == "cvta";
        bool isPrevPrevCvta = prevPrevStmt && prevPrevStmt->getInst() == "cvta";
        bool cvtaContainsToGlobal = false;

        std::vector<std::string> cvtaStmtMods;
        if (isPrevPrevCvta)
            cvtaStmtMods = prevPrevStmt->getModifiers();
        else if (isPrevCvta)
            cvtaStmtMods = prevStmt->getModifiers();

        if (isPrevPrevCvta || isPrevCvta) {
            cvtaContainsToGlobal = std::find(
                cvtaStmtMods.begin(),
                cvtaStmtMods.end(),
                "to.global"
            ) != cvtaStmtMods.end();
        }

        // check for getelementptr pattern
        if (
            (isPrevMul && isPrevPrevCvta && cvtaContainsToGlobal) ||
            (isPrevCvta && cvtaContainsToGlobal)
        ) {

            std::string ptrName = std::get<std::string>(
                SourceOps[0]->getValue()
            );
            llvm::Value* ptr = GetLlvmOperandValue(ptrName, true);

            auto indexValue = SourceOps[1]->getValue();
            std::string indexRegName = std::get<std::string>(indexValue);
            llvm::Value* index = GetLlvmOperandValue(indexRegName, true);

            llvm::Type* type = llvm::Type::getInt32Ty(
                *PtxToLlvmIrConverter::Context
            );
            llvm::Value* indexList[] = { index };
            llvm::Value* gep = PtxToLlvmIrConverter::Builder->CreateInBoundsGEP(
                type,
                ptr,
                llvm::ArrayRef<llvm::Value*>(indexList, 1)
            );

            genLlvmInstructions.push_back(gep);
        }
        else {
            auto firstOpValue = SourceOps[0]->getValue();
            auto secondOpValue = SourceOps[1]->getValue();

            OperandType firstOpType = SourceOps[0]->getType();
            OperandType secondOpType = SourceOps[1]->getType();

            llvm::Value* firstOperandValue = nullptr;
            if (firstOpType == OperandType::Register) {
                std::string regName = std::get<std::string>(firstOpValue);
                firstOperandValue = GetLlvmOperandValue(regName);
            }
            else if (firstOpType == OperandType::Immediate) {
                double value = std::get<double>(firstOpValue);
                firstOperandValue = GetImmediateValue(value);
            }

            llvm::Value* secondOperandValue = nullptr;
            if (secondOpType == OperandType::Register) {
                std::string regName = std::get<std::string>(secondOpValue);
                secondOperandValue = GetLlvmOperandValue(regName);
            }
            else if (secondOpType == OperandType::Immediate) {
                double value = std::get<double>(secondOpValue);
                secondOperandValue = GetImmediateValue(value);
            }

            llvm::Value* add = PtxToLlvmIrConverter::Builder->CreateAdd(
                firstOperandValue,
                secondOperandValue
            );

            genLlvmInstructions.push_back(add);
        }
    }
    else if (Inst == "mad") {
        auto firstOpValue = SourceOps[0]->getValue();
        auto secondOpValue = SourceOps[1]->getValue();
        auto thirdOpValue = SourceOps[2]->getValue();

        OperandType firstOpType = SourceOps[0]->getType();
        OperandType secondOpType = SourceOps[1]->getType();
        OperandType thirdOpType = SourceOps[2]->getType();
        
        llvm::Value* firstOperandValue = nullptr;
        if (firstOpType == OperandType::Register) {
            // get register's name
            std::string regName = std::get<std::string>(firstOpValue);
            firstOperandValue = GetLlvmOperandValue(regName);
        }

        llvm::Value* secondOperandValue = nullptr;
        if (secondOpType == OperandType::Register) {
            std::string regName = std::get<std::string>(secondOpValue);
            secondOperandValue = GetLlvmOperandValue(regName);
        }

        if (firstOperandValue == nullptr || secondOperandValue == nullptr)
            return;

        llvm::Value* mul = PtxToLlvmIrConverter::Builder->CreateMul(
            firstOperandValue,
            secondOperandValue
        );

        llvm::Value* thirdOperandValue = nullptr;
        if (thirdOpType == OperandType::Register) {
            std::string regName = std::get<std::string>(thirdOpValue);
            thirdOperandValue = GetLlvmOperandValue(regName);
        }

        if (thirdOperandValue == nullptr) return;

        llvm::Value* add = PtxToLlvmIrConverter::Builder->CreateAdd(
            mul,
            thirdOperandValue
        );

        genLlvmInstructions.push_back(mul);
        genLlvmInstructions.push_back(add);
    }
    else if (Inst == "setp") {
        auto firstOpValue = SourceOps[0]->getValue();
        auto secondOpValue = SourceOps[1]->getValue();

        OperandType firstOpType = SourceOps[0]->getType();
        OperandType secondOpType = SourceOps[1]->getType();

        llvm::Value* firstOperandValue = nullptr;
        if (firstOpType == OperandType::Register) {
            std::string regName = std::get<std::string>(firstOpValue);
            firstOperandValue = GetLlvmOperandValue(regName);
        }
        else if (firstOpType == OperandType::Immediate) {
            double value = std::get<double>(firstOpValue);
            firstOperandValue = GetImmediateValue(value);
        }

        llvm::Value* secondOperandValue = nullptr;
        if (secondOpType == OperandType::Register) {
            std::string regName = std::get<std::string>(secondOpValue);
            secondOperandValue = GetLlvmOperandValue(regName);
        }
        else if (secondOpType == OperandType::Immediate) {
            double value = std::get<double>(secondOpValue);
            secondOperandValue = GetImmediateValue(value);
        }

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

        llvm::Value* cond = GetLlvmOperandValue(Pred);

        // get name of current kernel, in order to add
        // the block targets of the branch to this kernel
        std::unique_ptr currKernel = GetCurrentKernel();
        std::string currKernelName = currKernel->getName();

        llvm::BasicBlock* falseBlock = llvm::BasicBlock::Create(
            *PtxToLlvmIrConverter::Context,
            "",
            PtxToLlvmIrConverter::Module->getFunction(currKernelName)
        );

        std::string targetValue = std::get<std::string>(
            DestOps[0]->getValue()
        );
        llvm::BasicBlock* trueBlock = llvm::BasicBlock::Create(
            *PtxToLlvmIrConverter::Context,
            targetValue,
            PtxToLlvmIrConverter::Module->getFunction(currKernelName)
        );

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
        std::string regName = std::get<std::string>(SourceOps[0]->getValue());
        llvm::Value* operand = GetLlvmOperandValue(regName);
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

        // if instruction type is signed, create sext
        llvm::Value* ext;
        if (type[0] == 's') {
            ext = PtxToLlvmIrConverter::Builder->CreateSExt(
                operand,
                extType
            );
        }
        else {
            ext = PtxToLlvmIrConverter::Builder->CreateZExt(
                operand,
                extType
            );
        }

        genLlvmInstructions.push_back(ext);
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