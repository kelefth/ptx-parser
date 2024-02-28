#include <iostream>
#include <iomanip>
#include <unistd.h>
#include <algorithm>
#include <regex>
#include <set>

// #include "KernelDirectStatement.h"

#include "parser/parser.h"
#include "InstrStatement.h"
#include "../PtxToLlvmIr/PtxToLlvmIrConverter.h"
#include "Operand.h"
#include "LinkingDirectStatement.h"
#include "VarDecDirectStatement.h"

#include "llvm/IR/InstIterator.h"
#include "llvm/IR/CFG.h"
// #include "llvm/Transforms/Utils/ValueMapper.h"

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

std::vector<std::string> InstrStatement::getTypes() const {
    return Types;
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

bool isInPredecessors(
    llvm::BasicBlock* block,
    llvm::BasicBlock *blockToFind,
    std::vector<llvm::BasicBlock*> visitedBlocks = {},
    std::vector<llvm::BasicBlock*>* visitedPath = nullptr
) {
    visitedBlocks.push_back(block);
    if (block == blockToFind) return true;

    for (auto predIt = pred_begin(block), predEnd = pred_end(block); predIt != predEnd; ++predIt) {
        llvm::BasicBlock* predecessor = *predIt;
        if(std::find(visitedBlocks.begin(), visitedBlocks.end(), predecessor) != visitedBlocks.end())
            continue;
        if(isInPredecessors(predecessor, blockToFind, visitedBlocks, visitedPath)) {
            if (visitedPath) visitedPath->push_back(predecessor);
            return true;
        }
    }

    return false;
}

bool isInSuccessors(
    llvm::BasicBlock* block,
    llvm::BasicBlock *blockToFind,
    std::vector<llvm::BasicBlock*> visitedBlocks,
    std::vector<llvm::BasicBlock*>* visitedPath
) {
    visitedBlocks.push_back(block);
    if (block == blockToFind) return true;

    for (auto succIt = succ_begin(block), succEnd = succ_end(block); succIt != succEnd; ++succIt) {
        // if (*succIt == block) continue;

        llvm::BasicBlock* successorBlock = *succIt;
        if(std::find(visitedBlocks.begin(), visitedBlocks.end(), successorBlock) != visitedBlocks.end())
            continue;
        if(isInSuccessors(successorBlock, blockToFind, visitedBlocks, visitedPath)) {
            visitedPath->push_back(successorBlock);
            return true;
        }
    }

    return false;
}

llvm::PHINode* findPhiInPreds(
    llvm::BasicBlock* block,
    llvm::Value* valueToFind,
    std::vector<llvm::BasicBlock*> visitedBlocks = {},
    std::vector<llvm::BasicBlock*>* visitedPath = nullptr
) {
    visitedBlocks.push_back(block);
    
    llvm::PHINode* phiInst = nullptr;
    for (auto &phi : block->phis()) {
        uint index = 0;
        for (auto &incVal : phi.incoming_values()) {
            if (phi.getIncomingValue(index) == valueToFind) {
                phiInst = &phi;
                return phiInst;
            }
            index++;
        }
    }

    for (auto predIt = pred_begin(block), predEnd = pred_end(block); predIt != predEnd; ++predIt) {

        llvm::BasicBlock* predecessor = *predIt;
        if(std::find(visitedBlocks.begin(), visitedBlocks.end(), predecessor) != visitedBlocks.end())
            continue;

        phiInst = findPhiInPreds(predecessor, valueToFind, visitedBlocks, visitedPath);
        if (visitedPath) visitedPath->push_back(predecessor);
        if (phiInst) break;
    }

    return phiInst;
}

// llvm::PHINode* InstrStatement::CheckAndGeneratePhiNode(
//     std::string opName,
//     std::pair<llvm::Value*, llvm::BasicBlock*> llvmStmt,
//     std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> llvmStmts,
//     llvm::Instruction* lastLlvmInst,
//     llvm::BasicBlock* currBlock,
//     std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>>* incomingValBlocksToAdd,
//     std::vector<llvm::BasicBlock*> visitedBlocks,
//     llvm::PHINode* phi = nullptr
// ) {
//     visitedBlocks.push_back(currBlock);

//     llvm::Value* llvmStmtValue = llvmStmt.first;
//     llvm::BasicBlock* llvmStmtBlock = llvmStmt.second;

//     llvm::pred_iterator pred = llvm::pred_begin(currBlock);
//     llvm::pred_iterator end = llvm::pred_end(currBlock);
//     for (pred; pred != end; ++pred) {
//         if(std::find(visitedBlocks.begin(), visitedBlocks.end(), *pred) != visitedBlocks.end())
//             continue;

//         // bool isPredAfterCurr = false;
//         // for (llvm::BasicBlock &bb : *currBlock->getParent())  {
//         //     if (&bb == *pred) break;
//         //     if (&bb == currBlock) {
//         //         isPredAfterCurr = true;
//         //         break;
//         //     }
//         // }

//         // if (isPredAfterCurr) continue;

//         std::vector<llvm::BasicBlock*> visitedPreds;
//         if (isInSuccessors(llvmStmtBlock, *pred, visitedPreds)) {
//             // Check if an incoming value from the same block already exists,
//             // and if yes the phi node must be inserted into a predecessor
//             bool isPhiInPred = false;
//             llvm::PHINode* predPhi = nullptr;
//             for (auto valBlock : *incomingValBlocksToAdd) {
//                 if (valBlock.second == *pred) {
//                     incomingValBlocksToAdd->clear();
//                     predPhi = CheckAndGeneratePhiNode(
//                         opName,
//                         valBlock,
//                         llvmStmts,
//                         lastLlvmInst,
//                         *pred,
//                         incomingValBlocksToAdd,
//                         visitedBlocks
//                     );
//                     isPhiInPred = true;

//                     if (predPhi) {
//                         incomingValBlocksToAdd->push_back(
//                             std::pair<llvm::Value*, llvm::BasicBlock*>(
//                                 predPhi, *pred
//                             )
//                         );
//                     }
//                 }
//             }

//             if ((incomingValBlocksToAdd->size() > 0) && !phi) {
//                 phi = llvm::PHINode::Create(
//                     lastLlvmInst->getType(),
//                     0,
//                     "",
//                     &*currBlock->getFirstInsertionPt()
//                 );
//                 // IR comment
//                 llvm::Constant *Str = llvm::ConstantDataArray::getString(*PtxToLlvmIrConverter::Context, opName);
//                 llvm::Metadata *MD = llvm::ConstantAsMetadata::get(Str);
//                 llvm::NamedMDNode *NMD = PtxToLlvmIrConverter::Module->getOrInsertNamedMetadata("comments");
//                 NMD->addOperand(llvm::MDNode::get(*PtxToLlvmIrConverter::Context, MD));
//                 phi->setMetadata("comments", NMD->getOperand(NMD->getNumOperands() - 1));
//                 /////////////////////////////////////////////////////////
//                 // phi->print(llvm::outs());
//                 // llvm::outs() << "\n";
//             }
//             if (!isPhiInPred) {
//                 if ((*pred)->getName() == "$L__BB4_22")
//                     llvm::outs() << "";
//                 incomingValBlocksToAdd->push_back(
//                     std::pair<llvm::Value*, llvm::BasicBlock*>(
//                         llvmStmtValue, *pred
//                     )
//                 );
//             }
//                 if (phi) {
//                     uint incValuesNum = phi->getNumIncomingValues();
//                     for (auto valBlock : *incomingValBlocksToAdd) {
//                         bool incValueFound = false;
//                         for (int i = 0; i < incValuesNum; ++i) {
//                             if (
//                                 phi->getIncomingValue(i) == valBlock.first &&
//                                 phi->getIncomingBlock(i) == valBlock.second
//                             ) {
//                                 incValueFound = true;
//                                 break;
//                             }
                                
//                         }
//                         if (!incValueFound)
//                             phi->addIncoming(valBlock.first, valBlock.second);
//                             if (valBlock.second->getName() == "$L__BB4_22")
//                                 PtxToLlvmIrConverter::Module->print(llvm::outs(), nullptr, false, true);
//                     }
//                 }

//             // }
//             // else {
//             //     incomingValBlocksToAdd->clear();
//             //     for (std::pair<llvm::Value*, llvm::BasicBlock*> newLlvmStmt : llvmStmts) {
//             //         phi = CheckAndGeneratePhiNode(
//             //             newLlvmStmt,
//             //             llvmStmts,
//             //             lastLlvmInst,
//             //             *pred,
//             //             incomingValBlocksToAdd,
//             //             visitedBlocks,
//             //             phi
//             //         );
//             //     }
//             // }
//         }
//     }
//     return phi;
// }

std::vector<llvm::BasicBlock*> findBlockIntersection(std::vector<std::vector<llvm::BasicBlock*>> blocks) {
    if (blocks.size() == 0) return {};

    std::vector<llvm::BasicBlock*> v_intersection = blocks[0];

    for (auto &b : blocks) {
        std::sort(b.begin(), b.end());
    }

    for (size_t i = 1; i < blocks.size(); i++) {
        std::vector<llvm::BasicBlock*> v_temp;
        std::set_intersection(v_intersection.begin(), v_intersection.end(), blocks[i].begin(), blocks[i].end(), back_inserter(v_temp));
        v_intersection = v_temp;
    }

    return v_intersection;
}

llvm::PHINode* InstrStatement::CheckAndGeneratePhiNode(
    std::string opName,
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>>* incomingValBlocksToAdd
) {

    llvm::BasicBlock* currBlock = PtxToLlvmIrConverter::Builder->GetInsertBlock();
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> valBlocksToAdd;

    std::vector<std::vector<llvm::BasicBlock*>> visitedPaths;
    for (auto valBlock : *incomingValBlocksToAdd) {
        llvm::pred_iterator pred = llvm::pred_begin(currBlock);
        llvm::pred_iterator end = llvm::pred_end(currBlock);
        for (pred; pred != end; ++pred) {
            std::vector<llvm::BasicBlock*> visitedPreds;
            std::vector<llvm::BasicBlock*> visitedPath;
            if (isInSuccessors(valBlock.second, *pred, visitedPreds, &visitedPath)) {
                // Skip values from the same block that not defined in the loop
                if ((currBlock == *pred) && (valBlock.second != currBlock))
                    continue;
                // Skip duplicates
                if(std::find(
                    valBlocksToAdd.begin(),
                    valBlocksToAdd.end(),
                    std::pair<llvm::Value*, llvm::BasicBlock*>(valBlock.first, *pred)
                    ) != valBlocksToAdd.end()
                ) continue;
                bool blockExists = false;
                for (auto valBlock : valBlocksToAdd) {
                    if (valBlock.second == *pred) {
                        valBlock.first->print(llvm::outs());
                        llvm::outs() << "\n";
                        blockExists = true;
                        break;
                    }
                }
                if (blockExists) continue;
                visitedPaths.push_back(visitedPath);
                // llvm::outs() << opName + " Pred before: \n";
                // valBlock.first->print(llvm::outs());
                // (*pred)->print(llvm::outs());
                valBlocksToAdd.push_back(
                    std::pair<llvm::Value*, llvm::BasicBlock*>(valBlock.first, *pred)
                );
                visitedPath.clear();
            }
            visitedPath.clear();
        }
    }

    std::vector<llvm::BasicBlock*> commonBlocks = findBlockIntersection(visitedPaths);
    uint currBlocksOccurs = 0;
    for (auto &nodes : visitedPaths) {
        currBlocksOccurs += std::count(nodes.begin(), nodes.end(), currBlock);
    }

    if (commonBlocks.size() == 0 && currBlocksOccurs < valBlocksToAdd.size()) {
        llvm::PHINode* phi = llvm::PHINode::Create(
            valBlocksToAdd[0].first->getType(),
            0,
            "",
            &*currBlock->getFirstInsertionPt()
        );

        for (auto valBlock : valBlocksToAdd) {
            phi->addIncoming(valBlock.first, valBlock.second);
        }

        return phi;
    }

    return nullptr;
}

void InstrStatement::GeneratePhiNodes(std::string ptxOperandName, int stStmtId) {

    std::vector<uint> defStmtIds = GetOperandWriteInstructionIds(
        ptxOperandName,
        stStmtId
    );

    // if none found, return
    if (defStmtIds.empty()) return;

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

    if (llvmStmts.empty()) return;

    llvm::Value* lastLlvmStmt = llvmStmts.back().first;
    llvm::Instruction* lastLlvmInst = llvm::dyn_cast<llvm::Instruction>(
        lastLlvmStmt
    );

    llvm::BasicBlock* currBlock = PtxToLlvmIrConverter::Builder->GetInsertBlock();
    lastLlvmStmt->print(llvm::outs());
    llvm::outs() << "\n";

    if (lastLlvmInst) {
        llvm::PHINode* phi = nullptr;
        bool blockChanged = false;
        // llvm::BasicBlock* prevBlock = lastLlvmStmt->getParent();
        // reverse the vector to get the last generated instruction from each
        // block
        reverse(llvmStmts.begin(), llvmStmts.end());
        std::pair<llvm::Value*, llvm::BasicBlock*> prevStmt = llvmStmts.back();
        std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> incomingValBlocksToAdd;
        std::vector<llvm::BasicBlock*> phiVisitPath;
        std::vector<llvm::BasicBlock*> valueVisitPath;
        isInPredecessors(currBlock, prevStmt.second, {}, &valueVisitPath);
        llvm::PHINode* prevPhi = findPhiInPreds(currBlock, prevStmt.first, {}, &phiVisitPath);
        if (prevPhi && (phiVisitPath.size() < valueVisitPath.size())) {
            incomingValBlocksToAdd.push_back(
                std::pair<llvm::Value*, llvm::BasicBlock*>(prevPhi, prevPhi->getParent())
            );
        }
        else {
            incomingValBlocksToAdd.push_back(
                std::pair<llvm::Value*, llvm::BasicBlock*>(prevStmt)
            );
        }
        for (std::pair<llvm::Value*, llvm::BasicBlock*> llvmStmt : llvmStmts) {
            llvm::Value* llvmStmtValue = llvmStmt.first;
            llvm::BasicBlock* llvmStmtBlock = llvmStmt.second;
            llvm::BasicBlock* prevBlock = prevStmt.second;
            if (prevBlock != llvmStmtBlock) {
                phiVisitPath.clear();
                valueVisitPath.clear();
                isInPredecessors(currBlock, llvmStmtBlock, {}, &valueVisitPath);
                llvm::PHINode* phi = findPhiInPreds(currBlock, llvmStmtValue, {}, &phiVisitPath);
                
                // bool blockExists = false;
                // for (auto valBlock : incomingValBlocksToAdd) {
                //     if (valBlock.second == llvmStmtBlock) {
                //         blockExists = true;
                //         break;
                //     }
                // }

                if (phi && (phiVisitPath.size() < valueVisitPath.size())) {
                // if (phi) {
                    // if (!blockExists) {
                        incomingValBlocksToAdd.push_back(
                            std::pair<llvm::Value*, llvm::BasicBlock*>(phi, phi->getParent())
                        );
                    // }
                }
                else {
                    // if (!blockExists) {
                        incomingValBlocksToAdd.push_back(
                            std::pair<llvm::Value*, llvm::BasicBlock*>(llvmStmt)
                        );
                        llvmStmt.first->print(llvm::outs());
                        llvm::outs() << "\n";
                    // }
                }

                prevStmt = llvmStmt;
            }
        }

        if (incomingValBlocksToAdd.size() > 1) {
            phi = CheckAndGeneratePhiNode(
                ptxOperandName,
                &incomingValBlocksToAdd
            );
            if (phi && (phi->getNumIncomingValues() < 2)) {
                phi->eraseFromParent();
                phi = nullptr;
            }
        }
    }
}

llvm::Value* InstrStatement::GetLlvmRegisterValue(
    std::string ptxOperandName,
    int stStmtId = -1
) {
    llvm::BasicBlock* currBlock = PtxToLlvmIrConverter::Builder->GetInsertBlock();

    llvm::Value* value = PtxToLlvmIrConverter::getRegToValueOutMapValue(
        currBlock, ptxOperandName
    );

    return value;
}

// llvm::Value* InstrStatement::GetLlvmRegisterValue(
//     std::string ptxOperandName,
//     int stStmtId = -1
// ) {
//     if (stStmtId == -1) stStmtId = getId();

//     // llvm::outs() << ptxOperandName << "\n";

//     std::vector<uint> defStmtIds = GetOperandWriteInstructionIds(
//         ptxOperandName,
//         stStmtId
//     );

//     // if none found, return
//     if (defStmtIds.empty()) return nullptr;

//     std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> llvmStmts;
//     for (uint stmtId : defStmtIds) {
//         std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> mappedStmts =
//             PtxToLlvmIrConverter::getPtxToLlvmMapValue(stmtId);
//         llvmStmts.insert(
//             llvmStmts.end(),
//             mappedStmts.begin(),
//             mappedStmts.end()
//         );
//     }

//     if (llvmStmts.empty()) {
//         std::shared_ptr<Statement> userStmt = GetStatementById(defStmtIds.back());
//         InstrStatement* userStmtInst = dynamic_cast<InstrStatement*>(
//             userStmt.get()
//         );

//         if (!userStmt) return nullptr;

//         for (auto &op : userStmtInst->getSourceOps()) {
//             if (op->getType() == OperandType::Register) {
//                 std::string opName = std::get<std::string>(op->getValue());
//                 llvm::Value* value = GetLlvmRegisterValue(opName);
//                 return value;
//             }
//             else if (op->getType() == OperandType::Label) {
//                 // Check if the operand is a global variable
//                 std::string opName = std::get<std::string>(op->getValue());
//                 llvm::GlobalVariable* globVar =
//                     PtxToLlvmIrConverter::Module->getGlobalVariable(opName);
//                 return globVar;
//             }
//         }
//     }
    
//     if (llvmStmts.empty()) return nullptr;

//     llvm::Value* lastLlvmStmt = llvmStmts.back().first;
//     llvm::Instruction* lastLlvmInst = llvm::dyn_cast<llvm::Instruction>(
//         lastLlvmStmt
//     );

//     llvm::BasicBlock* currBlock = PtxToLlvmIrConverter::Builder->GetInsertBlock();
//     lastLlvmStmt->print(llvm::outs());
//     llvm::outs() << "\n";

//     if (lastLlvmInst) {
//         llvm::PHINode* phi = nullptr;
//         // llvm::BasicBlock* prevBlock = lastLlvmStmt->getParent();
//         // reverse the vector to get the last generated instruction from each
//         // block
//         reverse(llvmStmts.begin(), llvmStmts.end());
//         std::pair<llvm::Value*, llvm::BasicBlock*> prevStmt = llvmStmts.back();
//         std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> incomingValBlocksToAdd;
//         for (std::pair<llvm::Value*, llvm::BasicBlock*> llvmStmt : llvmStmts) {
//             llvm::Value* llvmStmtValue = llvmStmt.first;
//             llvm::BasicBlock* llvmStmtBlock = llvmStmt.second;
//             llvm::BasicBlock* prevBlock = prevStmt.second;
//             if (prevBlock != llvmStmtBlock) {
//                 if (!isInPredecessors(currBlock, llvmStmtBlock)) continue;

//                 // std::vector<llvm::BasicBlock*> phiVisitPath;
//                 // std::vector<llvm::BasicBlock*> valueVisitPath;
//                 // isInPredecessors(currBlock, llvmStmtBlock, {}, &valueVisitPath);
//                 // llvm::PHINode* phi = findPhiInPreds(currBlock, llvmStmtValue, {}, &phiVisitPath);
//                 if (llvm::PHINode* phi = findPhiInPreds(currBlock, llvmStmtValue)) {
//                 // if (phi && (phiVisitPath.size() < valueVisitPath.size())) {
//                     return phi;
//                 }
//                 else return llvmStmtValue;

//                 prevStmt = llvmStmt;
//             }
//         }
//         reverse(llvmStmts.begin(), llvmStmts.end());
//     }

//     llvm::Value* instValue = nullptr;
//     for (auto it = llvmStmts.rbegin(); it != llvmStmts.rend(); ++it) {
//         if ((*it).first->getType()->getTypeID() != llvm::Type::VoidTyID) {
//             instValue = (*it).first;
//             break;
//         }
//     }
//     return instValue;


//     // get last if in the same block, or create a phi node with the last
//     // of each predecessor block, or find the value from an already created
//     // phi node
//     // llvm::BasicBlock* currBlock = PtxToLlvmIrConverter::Builder->GetInsertBlock();
//     // if (lastLlvmInst) {
//     //     llvm::PHINode* phi = nullptr;
//     //     bool blockChanged = false;
//     //     llvm::BasicBlock* prevBlock = currBlock;
//     //     // reverse the vector to get the last generated instruction from each
//     //     // block
//     //     reverse(llvmStmts.begin(), llvmStmts.end());
//     //     std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> incomingValBlocksToAdd;
//     //     for (std::pair<llvm::Value*, llvm::BasicBlock*> llvmStmt : llvmStmts) {
//     //         llvm::Value* llvmStmtValue = llvmStmt.first;
//     //         llvm::BasicBlock* llvmStmtBlock = llvmStmt.second;
//     //         if (prevBlock != llvmStmtBlock) {
//     //             if (isBlockInPredecessors(llvmStmtBlock, currBlock)) {
//     //                 // If in direct predecessor

//     //                 // if already shown in another predecessor create a phi node,
//     //                 // or add a value to an existing one
//     //                 if ((incomingValBlocksToAdd.size() > 0) && !phi) {
//     //                     phi = llvm::PHINode::Create(
//     //                         lastLlvmInst->getType(),
//     //                         0,
//     //                         "",
//     //                         &*currBlock->getFirstInsertionPt()
//     //                     );
//     //                 }
//     //                 incomingValBlocksToAdd.push_back(
//     //                     std::pair<llvm::Value*, llvm::BasicBlock*>(
//     //                         llvmStmtValue, llvmStmtBlock
//     //                     )
//     //                 );
//     //             }
//     //             else {
//     //                 // If not in direct predecessor
//     //                 std::vector<llvm::BasicBlock*> visitedBlocks;
//     //                 phi = CheckAndGeneratePhiNode(
//     //                     llvmStmt,
//     //                     llvmStmts,
//     //                     lastLlvmInst,
//     //                     currBlock,
//     //                     &incomingValBlocksToAdd,
//     //                     visitedBlocks,
//     //                     phi
//     //                 );
//     //             }
//     //         }

//     //         prevBlock = llvmStmtBlock;
//     //     }

//     //     if (phi) {
//     //         // Add all the incoming values to the generated phi node
//     //         uint incValuesNum = phi->getNumIncomingValues();
//     //         for (auto valBlock : incomingValBlocksToAdd) {
//     //             bool incValueFound = false;
//     //             // If the value for the same block already exists, don't add
//     //             // it again
//     //             for (int i = 0; i < incValuesNum; ++i) {
//     //                 if (
//     //                     phi->getIncomingValue(i) == valBlock.first &&
//     //                     phi->getIncomingBlock(i) == valBlock.second
//     //                 ) {
//     //                     incValueFound = true;
//     //                     break;
//     //                 }
//     //             }
//     //             if (!incValueFound)
//     //                 phi->addIncoming(valBlock.first, valBlock.second);
//     //         }

//     //         uint blockPredNum = llvm::pred_size(currBlock);
//     //         if ((blockPredNum < 2) || (phi->getNumIncomingValues() < blockPredNum)) {
//     //             phi->eraseFromParent();
//     //             return nullptr;
//     //         }

//     //         // std::vector<llvm::BasicBlock*> incBlocks;
//     //         // for (int i = 0; i < incValuesNum; ++i) {
//     //         //     llvm::Value* incValue = phi->getIncomingValue(i);
//     //         //     llvm::BasicBlock* incBlock = phi->getIncomingBlock(i);

//     //         //     if (
//     //         //         std::find(incBlocks.begin(), incBlocks.end(), incBlock) != incBlocks.end()
//     //         //     ) {
                    
//     //         //     }

//     //         //     incBlocks.push_back(incBlock);
//     //         // }
//     //         // phi->print(llvm::outs());
//     //         // llvm::outs() << "\n";
//     //         // PtxToLlvmIrConverter::Module->print(llvm::outs(), nullptr, false, true);
//     //         return phi;
//     //     }

//     //     reverse(llvmStmts.begin(), llvmStmts.end());
//     // }

//     // generate a load and get its value if the last generate instruction is a
//     // store, else get the value of the last generated llvm instruction
//     // that returns a value
//     // std::string lastLlvmStmtName = lastLlvmInst->getOpcodeName();
//     // llvm::Value* instValue = nullptr;
//     // // if (lastLlvmInst && (lastLlvmInst->getOpcode() == llvm::Instruction::Store)) {
//     // //     llvm::Value* firstOperand = lastLlvmInst->getOperand(0);
//     // //     llvm::Value* secondOperand = lastLlvmInst->getOperand(1);
//     // //     llvm::Type* loadType = firstOperand->getType();
//     // //     instValue = PtxToLlvmIrConverter::Builder->CreateLoad(
//     // //         loadType,
//     // //         secondOperand
//     // //     );
//     // // }
//     // // else {
//     //     for (auto it = llvmStmts.rbegin(); it != llvmStmts.rend(); ++it) {
//     //         if ((*it).first->getType()->getTypeID() != llvm::Type::VoidTyID) {
//     //             instValue = (*it).first;
//     //             break;
//     //         }
//     //     }
//     // // }

//     // return instValue;
// }

llvm::Value* InstrStatement::GetLlvmOperandValue(
    const std::unique_ptr<Operand>& operand,
    std::string ptxType = "",
    int stStmtId = -1
) {
    if (stStmtId == -1) stStmtId = getId();

    auto operandValue = operand->getValue();
    OperandType operandType = operand->getType();

    llvm::Value* operandLlvmValue = nullptr;
    if (operandType == OperandType::Register) {
        std::string regName = std::get<std::string>(operandValue);
        operandLlvmValue = GetLlvmRegisterValue(regName, stStmtId);
    }
    else if (operandType == OperandType::Immediate) {
        if (ptxType == "") ptxType = Types[0];
        double value = std::get<double>(operandValue);
        operandLlvmValue = GetLlvmImmediateValue(value, ptxType);
    }

    return operandLlvmValue;
}

llvm::Constant* InstrStatement::GetLlvmImmediateValue(
    double value,
    std::string ptxType
) {
    bool isSigned = false;

    char typePrefix = ptxType[0];
    // if signed
    if (typePrefix == 's')
        isSigned = true;

    llvm::Type* type = PtxToLlvmIrConverter::GetTypeMapping(
        ptxType
    )(*PtxToLlvmIrConverter::Context);

    if (type->isFloatingPointTy())
        return llvm::ConstantFP::get(type, value);
    else
        return llvm::ConstantInt::get(type, value, isSigned);
}

DirectStatement* InstrStatement::GetVar(std::string name) {
    std::unique_ptr<KernelDirectStatement> currKernel = GetCurrentKernel();
    DirectStatement* varStmt = nullptr;

    // check for global variables
    for (auto stmt : statements) {
        if (stmt->getId() > currKernel->getId()) break;
        LinkingDirectStatement* linkingDir =
            dynamic_cast<LinkingDirectStatement*>(stmt.get());
        VarDecDirectStatement* varDecDir =
            dynamic_cast<VarDecDirectStatement*>(stmt.get());
        if (linkingDir != nullptr) {
            if (linkingDir->getIdentifier() == name) {
                varStmt = linkingDir;
                break;
            }
        }
        else if (varDecDir != nullptr) {
            if (varDecDir->getIdentifier() == name) {
                varStmt = varDecDir;
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
    std::string operandName,
    int stStmtId = -1
) {
    if (stStmtId == -1) stStmtId = getId();

    std::unique_ptr currKernel = GetCurrentKernel();
    std::vector<std::shared_ptr<Statement>> currKernelStatements =
        currKernel->getBodyStatements();

    std::vector<uint> defInstIds;

    for (const auto stmt : currKernelStatements) {
        unsigned int stmtId = stmt->getId();
        if (stmtId >= stStmtId) break;
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

void changeRegTypePtrToInt(std::string regName) {
    llvm::Type* intType = llvm::Type::getInt64Ty(
        *PtxToLlvmIrConverter::Context
    );

    std::set<llvm::Instruction*> instsToDelete;

    // Update in and out maps
    std::vector<llvm::Instruction*> updatedInsts;
    std::vector<llvm::Instruction*> updatedMutInsts;
    auto inMaps = PtxToLlvmIrConverter::getRegToValueInMaps();
    for (const auto inMap : inMaps) {
        llvm::Value* inValue =
            PtxToLlvmIrConverter::getRegToValueInMapValue(inMap.first, regName);
        if (!inValue) continue;

        llvm::Instruction* regInst =
            llvm::dyn_cast<llvm::Instruction>(inValue);
        assert(regInst);

        llvm::Instruction* newSourceOpInst = nullptr;
        // Check if the instruction has been updated already
        auto instIt = updatedInsts.begin();
        auto instEnd = updatedInsts.end();
        auto value = std::find(updatedInsts.begin(), updatedInsts.end(), regInst);
        if (value != updatedInsts.end()) {
            newSourceOpInst = updatedMutInsts[value - updatedInsts.begin()];
        }
        else {
            newSourceOpInst = regInst->clone();
            newSourceOpInst->mutateType(intType);
            newSourceOpInst->insertBefore(regInst);
            updatedInsts.push_back(regInst);
            updatedMutInsts.push_back(newSourceOpInst);
        }

        // update already generated phi nodes
        if (regInst->getOpcode() == llvm::Instruction::PHI) {
            llvm::PHINode* phi = llvm::cast<llvm::PHINode>(regInst);
            uint incomingValuesNum = phi->getNumIncomingValues();
            for (uint i = 0; i < incomingValuesNum; ++i) {
                llvm::Value* incVal = phi->getIncomingValue(i);
                llvm::BasicBlock* incBlock = phi->getIncomingBlock(i);

                llvm::Value* newVal =
                    PtxToLlvmIrConverter::getRegToValueOutMapValue(incBlock, regName);
                phi->setIncomingValueForBlock(incBlock, newVal);
            }
        }

        inValue->replaceAllUsesWith(newSourceOpInst);
        
        PtxToLlvmIrConverter::setRegToValueInMapValue(
            inMap.first, regName, newSourceOpInst
        );

        instsToDelete.insert(regInst);
    }

    auto outMaps = PtxToLlvmIrConverter::getRegToValueOutMaps();
    for (const auto outMap : outMaps) {
        llvm::Value* outValue =
            PtxToLlvmIrConverter::getRegToValueOutMapValue(outMap.first, regName);
        if (!outValue) continue;

        llvm::Instruction* regInst =
            llvm::dyn_cast<llvm::Instruction>(outValue);
        assert(regInst);

        llvm::Instruction* newSourceOpInst = nullptr;
        // Check if the instruction has been updated already
        auto instIt = updatedInsts.begin();
        auto instEnd = updatedInsts.end();
        auto value = std::find(updatedInsts.begin(), updatedInsts.end(), regInst);
        if (value != updatedInsts.end()) {
            newSourceOpInst = updatedMutInsts[value - updatedInsts.begin()];
        }
        else {
            newSourceOpInst = regInst->clone();
            newSourceOpInst->mutateType(intType);
            newSourceOpInst->insertBefore(regInst);
            updatedInsts.push_back(regInst);
            updatedMutInsts.push_back(newSourceOpInst);
        }

        // update already generated phi node incoming values
        if (newSourceOpInst->getOpcode() == llvm::Instruction::PHI) {
            llvm::PHINode* phi = llvm::cast<llvm::PHINode>(newSourceOpInst);
            uint incomingValuesNum = phi->getNumIncomingValues();
            for (uint i = 0; i < incomingValuesNum; ++i) {
                llvm::Value* incVal = phi->getIncomingValue(i);
                llvm::BasicBlock* incBlock = phi->getIncomingBlock(i);

                llvm::Value* newVal =
                    PtxToLlvmIrConverter::getRegToValueInMapValue(outMap.first, regName);
                phi->setIncomingValueForBlock(incBlock, newVal);
            }
        }

        outValue->replaceAllUsesWith(newSourceOpInst);
        
        PtxToLlvmIrConverter::setRegToValueOutMapValue(
            outMap.first, regName, newSourceOpInst
        );

        instsToDelete.insert(regInst);
    }

    for (llvm::Instruction* val : instsToDelete) {
        if (val) {
            val->eraseFromParent();
            val = nullptr;
        }
    }
}

void InstrStatement::ToLlvmIr() {
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> genLlvmInstructions;

    // Fix parameter types, if not address
    if (Inst == "mul" || Inst == "shl" || Inst == "div" || Inst == "sub") {
        llvm::Value* firstSourceOperand = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secSourceOperand = GetLlvmOperandValue(SourceOps[1]);

        llvm::Type* firstOpType = firstSourceOperand->getType();
        llvm::Type* secOpType = secSourceOperand->getType();

        // llvm::BasicBlock* currBlock =
        //     PtxToLlvmIrConverter::Builder->GetInsertBlock();

        if (firstOpType->getTypeID() == llvm::Type::PointerTyID) {
            // firstSourceOperand->mutateType(intType);
            std::string firstOpName = std::get<std::string>(
                SourceOps[0]->getValue()
            );

            changeRegTypePtrToInt(firstOpName);
        }

        if (secOpType->getTypeID() == llvm::Type::PointerTyID) {
            // secSourceOperand->mutateType(intType);
            std::string secOpName = std::get<std::string>(
                SourceOps[1]->getValue()
            );

            changeRegTypePtrToInt(secOpName);
            secSourceOperand = GetLlvmOperandValue(SourceOps[1]);
            secSourceOperand->print(llvm::outs());
        }
    }

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

        // Update operands with phi nodes in the current block, where needed
        // for (llvm::Instruction &inst : *currBlock) {
        //     if (inst.getOpcode() == llvm::Instruction::PHI)
        //         continue;
        //     uint index = 0;
        //     for (auto operand : inst.operand_values()) {
        //         if (llvm::PHINode* phi = findPhiInPreds(currBlock, operand)) {
        //             inst.setOperand(index, phi);
        //         }
        //         index++;
        //     }
        // }

        llvm::BasicBlock* currBlock = PtxToLlvmIrConverter::Builder->GetInsertBlock();
        llvm::BasicBlock* prevBlock  = currBlock;

        // Update the instruction insertion point
        PtxToLlvmIrConverter::Builder->SetInsertPoint(newInsertBlock);

        currBlock = PtxToLlvmIrConverter::Builder->GetInsertBlock();

        // Iterate through blocks, delete empty blocks and add terminators if missing
        std::vector<llvm::BasicBlock*> blocksToRemove;

        for (auto bbIt = currFunction->begin(); bbIt != currFunction->end(); ++bbIt) {
            llvm::BasicBlock &bb = *bbIt;
            if (bb.getTerminator() || (&bb == currBlock))
                continue;

            auto bbItNext = std::next(bbIt);


            bool blockContainsValues = false;
            for (const auto stmt : currKernel->getBodyStatements()) {
                unsigned int stmtId = stmt->getId();
                
                auto values = PtxToLlvmIrConverter::getPtxToLlvmMapValue(stmtId);
                for (auto valPair : values) {
                    llvm::BasicBlock* block = valPair.second;
                    if (block == &bb) {
                        blockContainsValues = true;
                        break;
                    }
                }
            }

            if (!blockContainsValues && !bb.hasName()) {
                blocksToRemove.push_back(&bb);
                if (bbItNext != currFunction->end()) {
                    llvm::BasicBlock &nextBb = *bbItNext;
                    bb.replaceAllUsesWith(&nextBb);
                }
            }

            // If named block and instruction insertion has finished or unnamed
            // block, add terminator
            if ((bbItNext != currFunction->end()) && 
                ((bb.hasName() && bb.size() > 0) || !bb.hasName())
            ){
                llvm::BasicBlock &nextBb = *bbItNext;
                llvm::BranchInst* br = llvm::BranchInst::Create(&nextBb, &bb);
            }
        }

        for (llvm::BasicBlock* block : blocksToRemove) {
            block->eraseFromParent();
            block = nullptr;
        }

        PtxToLlvmIrConverter::UpdateMapsAndGeneratePhis(currBlock);

        // PtxToLlvmIrConverter::Module->print(llvm::outs(), nullptr, false, true);
        // for (const auto stmt : currKernel->getBodyStatements()) {
        //     if (stmt->getId() >= getId()) break;
        //     InstrStatement* instStmt =
        //         dynamic_cast<InstrStatement*>(stmt.get());
        //     if (!instStmt) continue;
        //     if (instStmt->getDestOps().size() > 0) {
        //         if (instStmt->getDestOps()[0]->getType() == OperandType::Register) {
        //             std::string destOpName = std::get<std::string>(
        //                 instStmt->getDestOps()[0]->getValue()
        //             );
        //             GeneratePhiNodes(destOpName, instStmt->getId() + 1);
        //         }
        //     }
        // }

        // for (llvm::BasicBlock* block : blocksToRemove)
        //     block->eraseFromParent();

        // PtxToLlvmIrConverter::Module->print(llvm::outs(), nullptr, false, true);

    }

    if (Inst == "ld") {
        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();

        // get operands of address expression
        auto sourceOpAddr = std::get<AddressExpr>(SourceOps[0]->getValue());

        std::shared_ptr<Operand> addrFirstOp =
            sourceOpAddr.getFirstOperand();
        OperandType addrFirstOpType = addrFirstOp->getType();
        std::shared_ptr<Operand> addrSecondOp =
            sourceOpAddr.getSecondOperand();
        
        // check if it's an ld.param instruction
        auto modIterator = std::find(
            Modifiers.begin(),
            Modifiers.end(),
            "param"
        );
        // if ld.param
        if (modIterator != Modifiers.end()) {
            llvm::Function *func = nullptr;
            std::string currKernelName = GetCurrentKernel()->getName();
            for(llvm::Function &f : PtxToLlvmIrConverter::Module->getFunctionList()) {
                if (f.getName().contains(currKernelName))
                    func = &f;
            }

            auto args = func->args();
            for (auto &arg : args) {
                if (
                    (arg.getName() == sourceOpAddr.getFirstOperand()->ToString()) ||
                    (arg.getName() == sourceOpAddr.getSecondOperand()->ToString())
                ) {
                    llvm::Type *argType = arg.getType();
                    // if (argType->getTypeID() != llvm::Type::PointerTyID) {
                    //     arg.mutateType(llvm::Type::getInt32PtrTy(
                    //         *PtxToLlvmIrConverter::Context
                    //     ));
                    // }
                    llvm::Value* addressValue = &arg;
                    if (addrSecondOp != nullptr) {
                        // address is the result of an operation, add an instruction
                        // with the operation and pass the result to the store
                        // instruction

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

                        llvm::Value* indexList[] = { addrSecondOperandValue };
                        llvm::Value* addrValue = PtxToLlvmIrConverter::Builder->CreateInBoundsGEP(
                            argType,
                            &arg,
                            llvm::ArrayRef<llvm::Value*>(indexList, 1),
                            ""
                        );

                        genLlvmInstructions.push_back(
                            std::pair<llvm::Value*, llvm::BasicBlock*>(
                                addrValue, currBlock)
                        );

                        addressValue = addrValue;
                    }

                    llvm::AllocaInst *alloca =
                        PtxToLlvmIrConverter::Builder->CreateAlloca(argType);
                    llvm::Value *store =
                        PtxToLlvmIrConverter::Builder->CreateStore(addressValue, alloca);
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
            else if (addrFirstOpType == OperandType::Label) {
                
            }
        }
    }
    else if (Inst == "st") {
        auto destOpAddr = std::get<AddressExpr>(DestOps[0]->getValue());
        std::string sourceOpName = std::get<std::string>(SourceOps[0]->getValue());
        llvm::Value* sourceOpValue = GetLlvmRegisterValue(sourceOpName);

        llvm::BasicBlock* currBlock =
                PtxToLlvmIrConverter::Builder->GetInsertBlock();

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

            llvm::Value* addrFirstOpValue = GetLlvmRegisterValue(regName);
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

                exprValue = PtxToLlvmIrConverter::Builder->CreateAdd(
                    addrFirstOpValue,
                    addrSecondOperandValue
                );

                genLlvmInstructions.push_back(
                    std::pair<llvm::Value*, llvm::BasicBlock*>(
                        exprValue, currBlock)
                );
            }

            if (sourceOpValue == nullptr || exprValue == nullptr)
                return;

            llvm::Value* st = PtxToLlvmIrConverter::Builder->CreateStore(
                sourceOpValue,
                exprValue
            );

            genLlvmInstructions.push_back(
                std::pair<llvm::Value*, llvm::BasicBlock*>(st, currBlock)
            );
        }
    }
    else if (Inst == "cvta") {
        llvm::Value* sourceOpValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(sourceOpValue, currBlock)
        );
    }
    else if (Inst == "cvt") {
        llvm::Value* sourceValue = GetLlvmOperandValue(SourceOps[0]);

        std::string sourceType = Types[1];
        std::string destType = Types[0];
        char sourceTypePrefix = sourceType[0];
        char destTypePrefix = destType[0];

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();

        std::string sourceTypeBitsStr = std::regex_replace(
            sourceType,
            std::regex("[a-z]+"),
            std::string("$1")
        );
        int sourceTypeBits = stoi(sourceTypeBitsStr);

        std::string destTypeBitsStr = std::regex_replace(
            destType,
            std::regex("[a-z]+"),
            std::string("$1")
        );
        int destTypeBits = stoi(destTypeBitsStr);

        if (sourceTypePrefix == destTypePrefix) {
            // Create zext, sext or trunc instruction
            llvm::Type* destLlvmType = PtxToLlvmIrConverter::GetTypeMapping(
                destType
            )(*PtxToLlvmIrConverter::Context);

            // if instruction type is signed, create sext
            llvm::Value* extTrunc;
            if (sourceTypePrefix == 's') {
                extTrunc = PtxToLlvmIrConverter::Builder->CreateSExtOrTrunc(
                    sourceValue,
                    destLlvmType
                );
            }
            else {
                extTrunc = PtxToLlvmIrConverter::Builder->CreateZExtOrTrunc(
                    sourceValue,
                    destLlvmType
                );
            }

            llvm::BasicBlock* currBlock =
                PtxToLlvmIrConverter::Builder->GetInsertBlock();
            genLlvmInstructions.push_back(
                std::pair<llvm::Value*, llvm::BasicBlock*>(extTrunc, currBlock)
            );
        }
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
            if (
                firstOpValue == "%ctaid"    || 
                firstOpValue == "%nctaid"   || 
                firstOpValue == "%ntid"     || 
                firstOpValue == "%tid"
            ) {
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
            else {
                // if bit type convert to address
                llvm::Value* opValue = GetLlvmOperandValue(SourceOps[0]);
                if (Types.front()[0] == 'b') {
                    opValue->mutateType(
                        llvm::PointerType::get(*PtxToLlvmIrConverter::Context, 0)
                    );

                }
                
                genLlvmInstructions.push_back(
                    std::pair<llvm::Value*, llvm::BasicBlock*>(opValue, currBlock)
                );
            }
        }
        else if (firstOpType == OperandType::Immediate) {
            // initialization instruction, store const in generated instructions
            double opValue = std::get<double>(SourceOps[0]->getValue());

            llvm::Value* firstOpValue = GetLlvmOperandValue(SourceOps[0]);
            llvm::Type* firstOpValueType = firstOpValue->getType();
            llvm::Constant* constValue;

            if (firstOpValueType->isIntegerTy()) {
                constValue = llvm::ConstantInt::get(
                    firstOpValueType, opValue, false
                );
            }
            else {
                constValue = llvm::ConstantFP::get(
                    firstOpValueType, opValue
                );
            }

            genLlvmInstructions.push_back(
                std::pair<llvm::Value*, llvm::BasicBlock*>(constValue, currBlock)
            );
        }
        else if (firstOpType == OperandType::Label) {
            std::string value = std::get<std::string>(
                SourceOps[0]->getValue()
            );

            DirectStatement* varStmt = GetVar(value);
        
            // Check if global variable already exists. If yes, do nothing
            llvm::GlobalVariable* globVar =
                PtxToLlvmIrConverter::Module->getGlobalVariable(value);

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
                    alignment = linkStmt->getAlignment();
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
                    llvm::Type* globVarType = PtxToLlvmIrConverter::GetTypeMapping(
                        Types[0]
                    )(*PtxToLlvmIrConverter::Context);

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
            }
            
            if (globVar) {
                llvm::BasicBlock* currBlock =
                    PtxToLlvmIrConverter::Builder->GetInsertBlock();
                genLlvmInstructions.push_back(
                    std::pair<llvm::Value*, llvm::BasicBlock*>(globVar, currBlock)
                );
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
        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();

        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        assert(firstOperandValue && secondOperandValue);

        // Check if destination and source registers are the same (possible
        // induction variable), if yes create phi node in the beginning of this
        // basic block
        llvm::Value* add = nullptr;
        std::string firstSourceOpName = std::get<std::string>(
            SourceOps[0]->getValue()
        );
        std::string destName = std::get<std::string>(DestOps[0]->getValue());
        
        llvm::Instruction* firstOperandInst = 
                llvm::dyn_cast<llvm::Instruction>(firstOperandValue);

        bool isFloatInst = firstOperandValue->getType()->isFloatingPointTy() ||
                           secondOperandValue->getType()->isFloatingPointTy();

        // if ((destName == firstSourceOpName) && !firstOperandInst) {
        //     llvm::BasicBlock* currBasicBlock
        //         = PtxToLlvmIrConverter::Builder->GetInsertBlock();

        //     llvm::PHINode* phi = CreatePhiInBlockStart(
        //         firstOperandValue,
        //         currBasicBlock
        //     );

        //     if (isFloatInst) {
        //         add = PtxToLlvmIrConverter::Builder->CreateFAdd(
        //             phi,
        //             secondOperandValue
        //         );
        //     }
        //     else {
        //         add = PtxToLlvmIrConverter::Builder->CreateAdd(
        //             phi,
        //             secondOperandValue
        //         );
        //     }

        //     // phi->addIncoming(add, currBasicBlock);
        // }
        // else {
            if (isFloatInst) {
                add = PtxToLlvmIrConverter::Builder->CreateFAdd(
                    firstOperandValue,
                    secondOperandValue
                );
            }
            else {
                add = PtxToLlvmIrConverter::Builder->CreateAdd(
                    firstOperandValue,
                    secondOperandValue
                );
            }
        // }

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
                            // llvm::Value* llvmValue =
                            //     PtxToLlvmIrConverter::getPtxToLlvmMapValue(
                            //         currInst->getId()
                            //     )[0].first;
                            llvm::Value* llvmValue =
                                PtxToLlvmIrConverter::Module->getGlobalVariable(
                                    sourceOpName
                                );
                            
                            llvm::GlobalValue* globVarLlvmValue =
                                llvm::cast<llvm::GlobalValue>(llvmValue);

                            llvm::Type* valueType = globVarLlvmValue->getValueType();
                            if (valueType->isArrayTy()) {
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
    }
    else if (Inst == "sub") {
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        assert(firstOperandValue && secondOperandValue);

        // Create phi node for possible induction variable, if the source
        // and dest registers are the same
        llvm::Value* sub = nullptr;
        std::string firstSourceOpName = std::get<std::string>(
            SourceOps[0]->getValue()
        );
        std::string destName = std::get<std::string>(DestOps[0]->getValue());

        llvm::Instruction* firstOperandInst = 
                llvm::dyn_cast<llvm::Instruction>(firstOperandValue);
            
        bool isFloatInst = firstOperandValue->getType()->isFloatingPointTy() ||
                           secondOperandValue->getType()->isFloatingPointTy();
        
        // if ((destName == firstSourceOpName) && !firstOperandInst) {
        //     llvm::BasicBlock* currBasicBlock
        //         = PtxToLlvmIrConverter::Builder->GetInsertBlock();

        //     llvm::PHINode* phi = CreatePhiInBlockStart(
        //         firstOperandValue,
        //         currBasicBlock
        //     );

        //     if (isFloatInst) {
        //         sub = PtxToLlvmIrConverter::Builder->CreateFSub(
        //             phi,
        //             secondOperandValue
        //         );
        //     }
        //     else {
        //         sub = PtxToLlvmIrConverter::Builder->CreateSub(
        //             phi,
        //             secondOperandValue
        //         );
        //     }

        //     // phi->addIncoming(sub, currBasicBlock);
        // }
        // else {
            if (isFloatInst) {
                sub = PtxToLlvmIrConverter::Builder->CreateFSub(
                    firstOperandValue,
                    secondOperandValue
                );
            }
            else {
                sub = PtxToLlvmIrConverter::Builder->CreateSub(
                    firstOperandValue,
                    secondOperandValue
                );
            }
        // }

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(sub, currBlock)
        );
    }
    else if (Inst == "mul") {
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        assert(firstOperandValue && secondOperandValue);

        // Create phi node for possible induction variable, if the source
        // and dest registers are the same
        llvm::Value* mul = nullptr;
        std::string firstSourceOpName = std::get<std::string>(
            SourceOps[0]->getValue()
        );
        std::string destName = std::get<std::string>(DestOps[0]->getValue());

        llvm::Instruction* firstOperandInst = 
                llvm::dyn_cast<llvm::Instruction>(firstOperandValue);

        bool isFloatInst = firstOperandValue->getType()->isFloatingPointTy() ||
                           secondOperandValue->getType()->isFloatingPointTy();

        // if ((destName == firstSourceOpName) && !firstOperandInst) {
        //     llvm::BasicBlock* currBasicBlock
        //         = PtxToLlvmIrConverter::Builder->GetInsertBlock();

        //     llvm::PHINode* phi = CreatePhiInBlockStart(
        //         firstOperandValue,
        //         currBasicBlock
        //     );

        //     if (isFloatInst) {
        //         mul = PtxToLlvmIrConverter::Builder->CreateFMul(
        //             phi,
        //             secondOperandValue
        //         );
        //     }
        //     else {
        //         mul = PtxToLlvmIrConverter::Builder->CreateMul(
        //             phi,
        //             secondOperandValue
        //         );
        //     }

        //     // phi->addIncoming(mul, currBasicBlock);
        // }
        // else {
            if (isFloatInst) {
                mul = PtxToLlvmIrConverter::Builder->CreateFMul(
                    firstOperandValue,
                    secondOperandValue
                );
            }
            else {
                mul = PtxToLlvmIrConverter::Builder->CreateMul(
                    firstOperandValue,
                    secondOperandValue
                );
            }
        // }

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(mul, currBlock)
        );
    }
    else if (Inst == "div") {
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        assert(firstOperandValue && secondOperandValue);

        bool isSigned = Types.front()[0] == 's';

        // Create phi node for possible induction variable, if the source
        // and dest registers are the same
        llvm::Value* div = nullptr;
        std::string firstSourceOpName = std::get<std::string>(
            SourceOps[0]->getValue()
        );
        std::string destName = std::get<std::string>(DestOps[0]->getValue());

        llvm::Instruction* firstOperandInst = 
                llvm::dyn_cast<llvm::Instruction>(firstOperandValue);

        bool isFloatInst = firstOperandValue->getType()->isFloatingPointTy() ||
                           secondOperandValue->getType()->isFloatingPointTy();

        // if ((destName == firstSourceOpName) && !firstOperandInst) {
        //     llvm::BasicBlock* currBasicBlock
        //         = PtxToLlvmIrConverter::Builder->GetInsertBlock();

        //     llvm::PHINode* phi = CreatePhiInBlockStart(
        //         firstOperandValue,
        //         currBasicBlock
        //     );

        //     if (isFloatInst) {
        //         div = PtxToLlvmIrConverter::Builder->CreateFDiv(
        //             phi,
        //             secondOperandValue
        //         );
        //     }
        //     else {    
        //         if (isSigned) {    
        //             div = PtxToLlvmIrConverter::Builder->CreateSDiv(
        //                 phi,
        //                 secondOperandValue
        //             );
        //         }
        //         else {
        //             div = PtxToLlvmIrConverter::Builder->CreateUDiv(
        //                 phi,
        //                 secondOperandValue
        //             );
        //         }
        //     }

        //     // phi->addIncoming(div, currBasicBlock);
        // }
        // else {
            if (isFloatInst) {
                div = PtxToLlvmIrConverter::Builder->CreateFDiv(
                    firstOperandValue,
                    secondOperandValue
                );
            }
            else {    
                if (isSigned) {
                    div = PtxToLlvmIrConverter::Builder->CreateSDiv(
                        firstOperandValue,
                        secondOperandValue
                    );
                }
                else {
                    div = PtxToLlvmIrConverter::Builder->CreateUDiv(
                        firstOperandValue,
                        secondOperandValue
                    );
                }
            }
        // }

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(div, currBlock)
        );
    }
    else if (Inst == "shl") {
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        assert(firstOperandValue && secondOperandValue);

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
    else if (Inst == "min" || Inst == "max") {
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        assert(firstOperandValue && secondOperandValue);

        llvm::Type *funcType = PtxToLlvmIrConverter::GetTypeMapping(
            Types[0]
        )(*PtxToLlvmIrConverter::Context);

        std::string funcTypeStr;
        llvm::raw_string_ostream ostream(funcTypeStr);
        funcType->print(ostream);
        funcTypeStr = ostream.str();

        std::vector<llvm::Type*> params;
        params.push_back(firstOperandValue->getType());
        params.push_back(secondOperandValue->getType());

        llvm::FunctionType *ft = llvm::FunctionType::get(
            funcType,
            params,
            false
        );

        bool isSigned = Types.front()[0] == 's';
        
        std::string funcNamePrefix = "";
        if (isSigned) funcNamePrefix = "llvm.u" + Inst + ".";
        else funcNamePrefix = "llvm.s" + Inst + ".";
        std::string funcName = funcNamePrefix + funcTypeStr;

        llvm::FunctionCallee func =
            PtxToLlvmIrConverter::Module->getOrInsertFunction(funcName, ft);
        llvm::Value *call =
            PtxToLlvmIrConverter::Builder->CreateCall(
                func,
                {firstOperandValue, secondOperandValue}
            );

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(call, currBlock)
        );
    }
    else if (Inst == "and") {
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        assert(firstOperandValue && secondOperandValue);

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

        assert(firstOperandValue && secondOperandValue);

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

        assert(firstOperandValue && secondOperandValue);

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

        assert(firstOperandValue && secondOperandValue);

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
    else if (Inst == "rcp") {
        llvm::Value* sourceOpValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Type* sourceOpType = sourceOpValue->getType();

        if (sourceOpValue == nullptr)
            return;

        char sourceTypePrefix = Types.front()[0];
        llvm::Value* div = nullptr;
        llvm::Value* constOne = nullptr;

        if (sourceOpType->isFloatingPointTy()) {
            constOne = llvm::ConstantFP::get(sourceOpType, 1.0);
            div = PtxToLlvmIrConverter::Builder->CreateFDiv(
                constOne, sourceOpValue
            );
        }
        else {
            constOne = llvm::ConstantInt::get(sourceOpType, 1);
            if (sourceTypePrefix == 's') {
                div = PtxToLlvmIrConverter::Builder->CreateSDiv(
                    constOne, sourceOpValue
                );
            }
            else {
                div = PtxToLlvmIrConverter::Builder->CreateUDiv(
                    constOne, sourceOpValue
                );
            }
        }


        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(div, currBlock)
        );
    }
    else if (Inst == "setp") {
        llvm::Value* firstOperandValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Value* secondOperandValue = GetLlvmOperandValue(SourceOps[1]);

        assert(firstOperandValue && secondOperandValue);

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

        llvm::BasicBlock* falseBlock = nullptr;
        if (Pred != "") {
            falseBlock = llvm::BasicBlock::Create(
                *PtxToLlvmIrConverter::Context,
                "",
                PtxToLlvmIrConverter::Module->getFunction(currKernelName)
            );

            falseBlock->moveAfter(currBasicBlock);
        }

        std::string targetValue = std::get<std::string>(
            DestOps[0]->getValue()
        );

        // check if label exists before the current instruction
        bool labelExistsBeforeInst = false;
        int firstLoopInstId = -1;
        for (const auto stmt : currKernel->getBodyStatements()) {
            if (stmt->getId() >= getId()) break;
            if (targetValue == stmt->getLabel()) {
                labelExistsBeforeInst = true;
                firstLoopInstId = stmt->getId();
            }
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

        // Check if conditional branch or not
        llvm::Value* br = nullptr;
        if (falseBlock) {
            // set target as null for now, need to patch it later
            llvm::Value* cond = GetLlvmRegisterValue(Pred);
            br = PtxToLlvmIrConverter::Builder->CreateCondBr(
                cond,
                trueBlock,
                falseBlock
            );

            // Update operands in the current block, after generating phi nodes
            llvm::BasicBlock* currBlock = PtxToLlvmIrConverter::Builder->GetInsertBlock();

            llvm::BasicBlock* prevBlock = currBlock;
            PtxToLlvmIrConverter::Builder->SetInsertPoint(falseBlock);
            currBlock = PtxToLlvmIrConverter::Builder->GetInsertBlock();

            PtxToLlvmIrConverter::UpdateMapsAndGeneratePhis(currBlock);
        }
        else br = PtxToLlvmIrConverter::Builder->CreateBr(trueBlock);

        br->print(llvm::outs());
        llvm::outs() << "\n";

        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(br, currBasicBlock)
        );

        // If in loop, iterate through loop statements again and apply fixes
        if (labelExistsBeforeInst) {
            if (trueBlock) {
                // reverse block's previous out map
                auto prevInMap = PtxToLlvmIrConverter::getRegToValueInMap(trueBlock);
                auto prevOutMap = PtxToLlvmIrConverter::getRegToValueOutMap(trueBlock);

                // Use multimap, cause there is one instance for each constant
                std::multimap<llvm::Value*, std::string> revPrevInMap;
                std::multimap<llvm::Value*, std::string> revPrevOutMap;
                for (auto& mapPair : prevInMap) {
                    revPrevInMap.insert(
                        std::pair<llvm::Value*, std::string>(
                            mapPair.second, mapPair.first
                        )
                    );
                }

                for (auto& mapPair : prevOutMap) {
                    revPrevOutMap.insert(
                        std::pair<llvm::Value*, std::string>(
                            mapPair.second, mapPair.first
                        )
                    );
                }

                PtxToLlvmIrConverter::UpdateMapsAndGeneratePhis(trueBlock);

                // Remove keys from the map that did not exist before
                // These are values that came up from the loop back
                auto newInMap = PtxToLlvmIrConverter::getRegToValueInMap(trueBlock);
                std::set<std::string> keysToRemove;
                for (const auto& mapEntry : newInMap) {
                    std::string key = mapEntry.first;
                    if (prevInMap.count(key) == 0 || !prevInMap[key])
                        keysToRemove.insert(key);
                }

                // prevInMap["%r79"]->print(llvm::outs());
                // llvm::outs() << "\n";
                // newInMap["%r79"]->print(llvm::outs());
                // llvm::outs() << "\n";

                for (const std::string key : keysToRemove)
                    newInMap.erase(key);

                PtxToLlvmIrConverter::setRegToValueInMap(trueBlock, newInMap);
                // auto newOutMap = PtxToLlvmIrConverter::getRegToValueInMap(trueBlock);
                // PtxToLlvmIrConverter::Module->print(llvm::outs(), nullptr, false, true);

                // Clear the out maps of loop's inner blocks
                std::string currKernelName = currKernel->getName();
                llvm::Function* currFunction =
                    PtxToLlvmIrConverter::Module->getFunction(currKernelName);
                bool inLoop = false;
                llvm::inst_iterator instIt = llvm::inst_begin(currFunction);
                llvm::inst_iterator instEnd = llvm::inst_end(currFunction);
                for (instIt, instEnd; instIt != instEnd; ++instIt) {
                    llvm::Instruction* inst = &*instIt;
                    llvm::BasicBlock* instBlock = inst->getParent();
                    if (instBlock == trueBlock) {
                        inLoop = true;
                        continue;
                    }
                    if (!inLoop) continue;

                    llvm::BranchInst* braInst =
                        llvm::dyn_cast<llvm::BranchInst>(inst);
                    if (braInst) {
                        uint succNum = braInst->getNumSuccessors();
                        if (
                            (braInst->getSuccessor(0) == trueBlock) ||
                            (succNum > 1 && (braInst->getSuccessor(1) == trueBlock))
                        ) break;
                    }
                    
                    PtxToLlvmIrConverter::clearRegToValueOutMap(instBlock);
                }
                

                // Update operands in the loop with the new generated out map values
                // Update inner blocks in and out maps and phi node values
                bool inCurrBlock = false;
                bool skipBlock = false;
                int innerLoopTermId = -1;
                llvm::BasicBlock* lastVisitedBlock = trueBlock;
                for (const auto stmt : currKernel->getBodyStatements()) {
                    uint stmtId = stmt->getId();

                    // find current block
                    if (stmt->getLabel() == targetValue)
                        inCurrBlock = true;
                    if (!inCurrBlock) continue;

                    // if (stmt->getId() > innerLoopTermId) {
                    //     innerLoopTermId = -1;
                    //     skipBlock = false;
                    // }

                    if (stmtId >= this->getId()) {
                        break;
                    }

                    auto llvmValues =
                        PtxToLlvmIrConverter::getPtxToLlvmMapValue(stmtId);
                    if (llvmValues.size() == 0) continue;
                        
                    llvm::Value* stmtValue = llvmValues.front().first;
                    llvm::BasicBlock* stmtBlock = llvmValues.front().second;
                    std::string stmtBlockName = stmtBlock->getName().str();


                    // // Skip nested loops
                    // if ((stmtBlock != trueBlock) && (stmtBlock != lastVisitedBlock) && stmtBlockName != "") {
                    //     for (const auto loopStmt : currKernel->getBodyStatements()) {
                    //         if (loopStmt->getId() < stmt->getId()) continue;
                    //         InstrStatement* instStmt =
                    //             dynamic_cast<InstrStatement*>(loopStmt.get());
                    //         if (!instStmt) continue;
                    //         if (instStmt->getInst() == "bra") {
                    //             std::string braSourceOp = std::get<std::string>(
                    //                 instStmt->getDestOps()[0]->getValue()
                    //             );
                    //             if (stmt->getLabel() == braSourceOp) {
                    //                 skipBlock = true;
                    //                 innerLoopTermId = instStmt->getId();
                    //             }
                    //         }
                    //     }
                    // }

                    // if (skipBlock) continue;

                    llvm::Instruction* inst =
                        llvm::dyn_cast<llvm::Instruction>(stmtValue);

                    InstrStatement* instStmt =
                        dynamic_cast<InstrStatement*>(stmt.get());

                    if (!instStmt) {
                        lastVisitedBlock = stmtBlock;
                        continue;
                    }

                    if (instStmt->getDestOps().size() == 0) {
                        lastVisitedBlock = stmtBlock;
                        continue;
                    }

                    // Check block change, update the in and out maps
                    // of the next block and regenerate phi nodes
                    if (stmtBlock != lastVisitedBlock) {
                        // PtxToLlvmIrConverter::setRegToValueOutMap(lastVisitedBlock, newOutMap);

                        // reverse block's previous maps
                        prevInMap = PtxToLlvmIrConverter::getRegToValueInMap(stmtBlock);
                        prevOutMap = PtxToLlvmIrConverter::getRegToValueOutMap(stmtBlock);
                        // Use multimap, cause there is one instance for each constant
                        revPrevInMap.clear();
                        for (auto& mapPair : prevInMap) {
                            revPrevInMap.insert(
                                std::pair<llvm::Value*, std::string>(
                                    mapPair.second, mapPair.first
                                )
                            );
                        }
                        revPrevOutMap.clear();
                        for (auto& mapPair : prevOutMap) {
                            revPrevOutMap.insert(
                                std::pair<llvm::Value*, std::string>(
                                    mapPair.second, mapPair.first
                                )
                            );
                        }

                        // uint phiIndex = 0;
                        // for (llvm::PHINode& phi : stmtBlock->phis()) {
                        //     auto regNames = revPrevInMap.equal_range(&phi);
                        //     std::string regName = regNames.first->second;

                        //     uint incValNum = phi.getNumIncomingValues();
                        //     for (uint i = 0; i < incValNum; ++i) {
                        //         llvm::BasicBlock* block = phi.getIncomingBlock(i);
                        //         llvm::Value* value = phi.getIncomingValue(i);
                        //         llvm::Value* newValue =
                        //             PtxToLlvmIrConverter::getRegToValueOutMapValue(block, regName);
                        //         newOutMap[regName] = newValue;
                        //         llvm::outs() << "Phi: ";
                        //         phi.print(llvm::outs());
                        //         llvm::outs() << "\nValue: ";
                        //         value->print(llvm::outs());
                        //         llvm::outs() << "\nNew Value: ";
                        //         newValue->print(llvm::outs());
                        //         llvm::outs() << "\n";
                        //         phi.setIncomingValue(i, newValue);
                        //     }
                        //     phiIndex++;
                        // }

                        std::map<std::string, llvm::Value*> prevPhis;
                        std::map<std::string, llvm::Value*> inMap =
                            PtxToLlvmIrConverter::getRegToValueInMap(stmtBlock);

                        for (auto mapIt = inMap.begin(); mapIt != inMap.end(); ++mapIt) {
                            if (mapIt->second) {
                                llvm::PHINode* pairPhi =
                                    llvm::dyn_cast<llvm::PHINode>(mapIt->second);
                                if (pairPhi) prevPhis.insert(*mapIt);
                            }
                        }

                        PtxToLlvmIrConverter::UpdateMapsAndGeneratePhis(stmtBlock);

                        // Remove keys from the map that did not exist before
                        // These are values that came up from the loop back
                        auto newInMap = PtxToLlvmIrConverter::getRegToValueInMap(stmtBlock);
                        std::set<std::string> keysToRemove;
                        for (const auto& mapEntry : newInMap) {
                            std::string key = mapEntry.first;
                            if (inMap.count(key) == 0 || !inMap[key])
                                keysToRemove.insert(key);
                        }

                        for (const std::string key : keysToRemove)
                            newInMap.erase(key);

                        // Keep phi values if new values are not phi nodes
                        for (const auto& mapEntry : newInMap) {
                            std::string key = mapEntry.first;
                            llvm::Value* value = mapEntry.second;
                            llvm::PHINode* newPhi =
                                llvm::dyn_cast<llvm::PHINode>(value);
                            llvm::Value* prevValue = inMap[key];
                            llvm::PHINode* prevPhi =
                                llvm::dyn_cast<llvm::PHINode>(prevValue);

                            if (prevPhi && prevPhi) {
                                newInMap[key] = prevPhi;
                            }
                        }

                        PtxToLlvmIrConverter::setRegToValueInMap(stmtBlock, newInMap);
                        PtxToLlvmIrConverter::setRegToValueOutMap(stmtBlock, newInMap);

                        std::set<llvm::PHINode*> phisToDelete;
                        for (auto mapIt = prevPhis.begin(); mapIt != prevPhis.end(); ++mapIt) {
                            std::string regName = mapIt->first;
                            llvm::PHINode* prevPhi = 
                                llvm::cast<llvm::PHINode>(mapIt->second);
                            llvm::Value* newValue =
                                PtxToLlvmIrConverter::getRegToValueInMapValue(
                                    stmtBlock, regName
                                );
                            llvm::PHINode* newPhi =
                                llvm::dyn_cast<llvm::PHINode>(newValue);
                            prevPhi->print(llvm::outs());
                            llvm::outs() << "\n";
                            newValue->print(llvm::outs());
                            llvm::outs() << "\n";
                            // if (prevPhi == newPhi) continue;

                            if (newPhi) {
                                uint prevPhiIncValuesNum = prevPhi->getNumIncomingValues();
                                uint newPhiIncValuesNum = newPhi->getNumIncomingValues();
                                for (uint i = 0; i < newPhiIncValuesNum; ++i) {
                                    llvm::Value* newIncVal = newPhi->getIncomingValue(i);
                                    llvm::BasicBlock* incBlock = newPhi->getIncomingBlock(i);
                                    if (incBlock == stmtBlock)
                                        newPhi->setIncomingValue(i, prevPhi->getIncomingValue(i));
                                    else
                                        newPhi->setIncomingValue(i, newIncVal);
                                }
                                // PtxToLlvmIrConverter::setRegToValueInMapValue(
                                //     stmtBlock, regName, prevPhi
                                // );
                                // phisToDelete.insert(newPhi);
                            }
                            
                            if ((prevPhi != newPhi) && newPhi)
                                prevPhi->replaceAllUsesWith(newPhi);

                            // // Replace the values in all maps too
                            // for (auto& bb : *kernelFunc) {
                            //     llvm::Value* regOutValue =
                            //         PtxToLlvmIrConverter::getRegToValueOutMapValue(&bb, regName);
                            //     if (regOutValue == prevPhi)
                            //         PtxToLlvmIrConverter::setRegToValueOutMapValue(&bb, regName, newPhi);

                            //     llvm::Value* regInValue =
                            //         PtxToLlvmIrConverter::getRegToValueInMapValue(&bb, regName);
                            //     if (regInValue == prevPhi)
                            //         PtxToLlvmIrConverter::setRegToValueInMapValue(&bb, regName, newPhi);
                            // }
                            // PtxToLlvmIrConverter::setRegToValueOutMapValue(
                            //     stmtBlock, regName, newPhi
                            // );
                            // phisToDelete.insert(prevPhi);
                        }
                        for (auto phi : phisToDelete) {
                            phi->eraseFromParent();
                            phi = nullptr;
                        }
                    }

                    std::string destRegName = "";
                    if (instStmt->getDestOps()[0]->getType() == OperandType::Register)
                        destRegName = std::get<std::string>(instStmt->getDestOps()[0]->getValue());
                    // else if (instStmt->getDestOps()[0]->getType() == OperandType::Address) {
                    //     auto destOpAddr = std::get<AddressExpr>(instStmt->getDestOps()[0]->getValue());
                    //     auto addrFirstOp = destOpAddr.getFirstOperand();
                    //     auto addrSecOp = destOpAddr.getSecondOperand();
                    //     if (addrFirstOp->getType() == OperandType::Register)
                    //         destRegName = std::get<std::string>(addrFirstOp->getValue());
                    //     else if (addrSecOp->getType() == OperandType::Register)
                    //         destRegName = std::get<std::string>(addrSecOp->getValue());
                    // }
                    else {
                        lastVisitedBlock = stmtBlock;
                        continue;
                    }

                    if (!inst) {
                        if (
                            llvm::isa<llvm::ConstantInt>(stmtValue) ||
                            llvm::isa<llvm::ConstantFP>(stmtValue)
                        )
                        PtxToLlvmIrConverter::setRegToValueOutMapValue(
                            stmtBlock, destRegName, stmtValue
                        );

                        lastVisitedBlock = stmtBlock;
                        continue;
                    }

                    for (auto llvmValuePair : llvmValues) {
                        llvm::Value* llvmValue = llvmValuePair.first;
                        if (llvm::dyn_cast<llvm::PHINode>(llvmValue)) {
                            // newOutMap[destRegName] = llvmValue;
                            PtxToLlvmIrConverter::setRegToValueOutMapValue(
                                stmtBlock, destRegName, llvmValue
                            );
                            continue;
                        }
                        llvm::Instruction* llvmInst =
                            llvm::dyn_cast<llvm::Instruction>(llvmValue);
                        if (!llvmInst) continue;
                        uint opIndex = 0;
                        llvmInst->print(llvm::outs());
                        llvm::outs() << "\n";
                        for (auto operand : llvmInst->operand_values()) {
                            auto regNames = revPrevInMap.equal_range(operand);
                            std::string regName = "";
                            for (auto regNameIt = regNames.first; regNameIt != regNames.second; ++regNameIt) {
                                // Check if value in map corresponds to a register
                                // in the PTX instruction
                                uint sourceOpsNum = instStmt->getSourceOps().size();
                                for (uint i = 0; i < sourceOpsNum; ++i) {
                                    std::string sourceOpName = "";
                                    if (instStmt->getSourceOps()[i]->getType() == OperandType::Register)
                                        sourceOpName = std::get<std::string>(instStmt->getSourceOps()[i]->getValue());
                                    else if (instStmt->getSourceOps()[i]->getType() == OperandType::Address) {
                                        auto sourceOpAddr = std::get<AddressExpr>(instStmt->getSourceOps()[i]->getValue());
                                        auto addrFirstOp = sourceOpAddr.getFirstOperand();
                                        auto addrSecOp = sourceOpAddr.getSecondOperand();
                                        if (addrFirstOp->getType() == OperandType::Register)
                                            sourceOpName = std::get<std::string>(addrFirstOp->getValue());
                                        else if (addrSecOp && addrSecOp->getType() == OperandType::Register)
                                            sourceOpName = std::get<std::string>(addrSecOp->getValue());
                                    }

                                    if (sourceOpName == regNameIt->second) {
                                        regName = regNameIt->second;
                                        break;
                                    }
                                    if (regName != "") break;
                                }
                            }
                            // std::string regName = revPrevInMap[operand];
                            if (regName != "") {
                                llvm::Value* newValue = nullptr;
                                llvm::Value* newInValue =
                                    PtxToLlvmIrConverter::getRegToValueInMapValue(
                                        stmtBlock, regName
                                    );
                                newInValue->print(llvm::outs());
                                // If there is phi for this register in the new
                                // IN state, get the phi node
                                llvm::Instruction* newInInst = nullptr;
                                if (newInValue)
                                    newInInst = llvm::dyn_cast<llvm::Instruction>(newInValue);
                                if (
                                    newInInst &&
                                    (newInInst->getOpcode() == llvm::Instruction::PHI)
                                ) {
                                    newValue = newInValue;
                                }
                                else  {
                                    // newValue = newOutMap[regName];
                                    newValue = PtxToLlvmIrConverter::getRegToValueOutMapValue(
                                        stmtBlock, regName
                                    );
                                }

                                // if (llvm::PHINode* instPhi = llvm::dyn_cast<llvm::PHINode>(llvmInst)) {
                                //     if (instPhi->getIncomingValue(0) == newValue) {
                                //         instPhi->print(llvm::outs());
                                //         llvm::outs() << "\n";
                                //     }
                                // }
                                // if (newValue == llvmInst) {
                                //     // PtxToLlvmIrConverter::Module->print(llvm::outs(), nullptr, false, true);
                                //     llvm::outs() << "Value same as incoming: ";
                                //     llvmInst->print(llvm::outs());
                                //     llvm::outs() << "\n";
                                // }
                                llvm::outs() << "\n";
                                llvmInst->setOperand(opIndex, newValue);
                            }
                            opIndex++;
                        }
                        // llvmInst->print(llvm::outs());
                        // llvm::outs() << "\n";
                        // newOutMap[destRegName] = llvmValue;
                        PtxToLlvmIrConverter::setRegToValueOutMapValue(
                            stmtBlock, destRegName, llvmValue
                        );
                    }

                    lastVisitedBlock = stmtBlock;
                }
                // PtxToLlvmIrConverter::Module->print(llvm::outs(), nullptr, false, true);
            }
            if (falseBlock) PtxToLlvmIrConverter::UpdateMapsAndGeneratePhis(falseBlock);
                
            //     std::string currKernelName = currKernel->getName();
            //     llvm::Function* currFunction =
            //         PtxToLlvmIrConverter::Module->getFunction(currKernelName);
            //     bool inLoop = false;
            //     bool skipBlock = false;
            //     llvm::BranchInst* innerloopBra = nullptr;
            //     llvm::BasicBlock* lastBlock = trueBlock;
            //     llvm::inst_iterator instIt = inst_begin(currFunction);
            //     llvm::inst_iterator instEnd = inst_end(currFunction);
            //     // std::map<llvm::Value*, std::string> revOutMap;
            //     for (instIt, instEnd; instIt != instEnd; ++instIt) {
            //         llvm::Instruction* inst = &*instIt;
            //         llvm::BasicBlock* instBlock = inst->getParent();
            //         std::string instBlockName = instBlock->getName().str();
            //         if (instBlock == trueBlock) inLoop = true;
            //         if (!inLoop) {
            //             lastBlock = instBlock;
            //             continue;
            //         }
                        
            //         if (inst == br) break;                    

            //         if (inst->getOpcode() == llvm::Instruction::PHI)
            //             continue;

            //         if (inst == innerloopBra) skipBlock = false;

            //         // if (lastBlock != trueBlock) {
            //         //     // reverse block's out map
            //         //     auto outMap = PtxToLlvmIrConverter::getRegToValueOutMap(instBlock);
            //         //     for (auto& mapPair : outMap) {
            //         //         revOutMap.insert(
            //         //             std::pair<llvm::Value*, std::string>(
            //         //                 mapPair.second, mapPair.first
            //         //             )
            //         //         );
            //         //     }
            //         // }

            //         // Check if there is an inner loop
            //         bool inInnerBlock = false;
            //         llvm::inst_iterator inInstIt = inst_begin(currFunction);
            //         llvm::inst_iterator inInstEnd = inst_end(currFunction);
            //         if ((instBlock != trueBlock) && (instBlock != lastBlock) && instBlockName != "") {
            //             for (inInstIt, inInstEnd; inInstIt != inInstEnd; ++inInstIt) {
            //                 llvm::Instruction* innerInst = &*inInstIt;
            //                 llvm::BasicBlock* innerInstBlock =
            //                     innerInst->getParent();
            //                 if (innerInstBlock == instBlock) inInnerBlock = true;
            //                 if (!inInnerBlock) continue;

            //                 llvm::BranchInst* bra =
            //                     llvm::dyn_cast<llvm::BranchInst>(innerInst);
            //                 if (bra) {
            //                     uint braSuccsNum = bra->getNumSuccessors();
            //                     if (
            //                         bra->getSuccessor(0) == instBlock ||
            //                         (braSuccsNum > 1 && bra->getSuccessor(1) == instBlock)
            //                     ) {
            //                         skipBlock = true;
            //                         innerloopBra = bra;
            //                     }
            //                 }
            //             }
            //         }

            //         if (skipBlock) {
            //             lastBlock = instBlock;
            //             continue;
            //         }                        

            //         uint opIndex = 0;
            //         for (auto operand : inst->operand_values()) {
            //             std::string regName = revPrevOutMap[operand];
            //             auto opInst = llvm::dyn_cast<llvm::Instruction>(operand);
            //             llvm::Value* newValue =
            //                 PtxToLlvmIrConverter::getRegToValueOutMapValue(instBlock, regName);
            //             if (newValue) {
            //                 inst->setOperand(opIndex, newValue);
            //                 // newValue->print(llvm::outs());
            //             }
            //             opIndex++;
            //         }
            //         lastBlock = instBlock;
            //     }
            // }

            // bool inCurrBlock = false;
            // bool skipBlock = false;
            // int innerLoopTermId = -1;
            // uint iterCount = 0;
            // for (const auto stmt : currKernel->getBodyStatements()) {
            //     uint stmtId = stmt->getId();
            //     if (stmtId >= this->getId()) break;

            //     if (stmt->getLabel() == targetValue)
            //         inCurrBlock = true;
            //     // find current block
            //     if (!inCurrBlock) continue;

            //     if (stmt->getId() > innerLoopTermId) {
            //         innerLoopTermId = -1;
            //         skipBlock = false;
            //     }

            //     // Skip nested loops
            //     if (!skipBlock && iterCount > 0) {
            //         for (const auto loopStmt : currKernel->getBodyStatements()) {
            //             if (loopStmt->getId() < stmt->getId()) continue;
            //             InstrStatement* instStmt =
            //                 dynamic_cast<InstrStatement*>(loopStmt.get());
            //             if (!instStmt) continue;
            //             if (instStmt->getInst() == "bra") {
            //                 std::string braSourceOp = std::get<std::string>(
            //                     instStmt->getDestOps()[0]->getValue()
            //                 );
            //                 if (stmt->getLabel() == braSourceOp) {
            //                     skipBlock = true;
            //                     innerLoopTermId = instStmt->getId();
            //                 }
            //             }
            //         }
            //     }

            //     if (skipBlock) continue;

            //     // skip if not an instruction or has no source operands
            //     InstrStatement* instStmt =
            //         dynamic_cast<InstrStatement*>(stmt.get());
            //     if (!instStmt || (instStmt->getSourceOps().size() == 0))
            //         continue;

            //     auto sourceOp = instStmt->getSourceOps()[0]->getValue();
            //     std::string* firstSourceOpName =
            //         std::get_if<std::string>(&sourceOp);

            //     auto destOp = instStmt->getDestOps()[0]->getValue();
            //     std::string* destName =
            //         std::get_if<std::string>(&destOp);

            //     // skip if first or destination operand is not a register
            //     if (!firstSourceOpName || !destName) continue;
                
            //     // if (*destName == *firstSourceOpName) {

            //     //     auto llvmValues =
            //     //         PtxToLlvmIrConverter::getPtxToLlvmMapValue(stmtId);
            //     //     if (llvmValues.size() == 0) continue;
            //     //     std::pair<llvm::Value*, llvm::BasicBlock*> llvmValue =
            //     //         llvmValues[0];
            //     //     // llvmValue.first->print(llvm::outs());
            //     //     llvm::Instruction* llvmInst =
            //     //         llvm::dyn_cast<llvm::Instruction>(llvmValue.first);

            //     //     if (!llvmInst) continue;

            //     //     // Find loop block
            //     //     llvm::BasicBlock* loopBlock = nullptr;
            //     //     for (llvm::BasicBlock &bb : *kernelFunc) {
            //     //         std::string bbName = bb.getName().str();
            //     //         if (bbName == "") continue;
            //     //         if (bbName == targetValue)
            //     //             loopBlock = &bb;
            //     //     }

            //     //     // Avoid adding duplicate phi nodes
            //     //     bool phiExists = false;
            //     //     for (llvm::PHINode &phiNode : loopBlock->phis()) {
            //     //         if (phiNode.getBasicBlockIndex(loopBlock) >= 0)
            //     //             phiExists = phiNode.getIncomingValueForBlock(loopBlock) == llvmInst;
            //     //     }

            //     //     if (phiExists) continue;

            //     //     assert(loopBlock);

            //     //     // Find def of first operand outside the loop,
            //     //     // if doesn't exist don't create phi
            //     //     llvm::Value* firstSourceOpValue = GetLlvmOperandValue(
            //     //         instStmt->getSourceOps()[0],
            //     //         firstLoopInstId
            //     //     );
            //     //     llvm::PHINode* phi = CreatePhiInBlockStart(
            //     //         firstSourceOpValue,
            //     //         loopBlock
            //     //     );

            //     //     // TODO
            //     //     // if (currBasicBlock->hasNPredecessorsOrMore(2)) {
            //     //     //     for (llvm::PHINode &phiNode : currBasicBlock->phis()) {
            //     //     //         if (phiNode.getBasicBlockIndex(currBasicBlock) >= 0)
            //     //     //             phiExists = phiNode.getIncomingValueForBlock(currBasicBlock) == llvmInst;
            //     //     //     }
            //     //     // }
            //     //     // else {
            //     //         // update first operand with phi node and add this value
            //     //         // as incoming in the phi node (loop)
            //     //         phi->addIncoming(llvmInst, currBasicBlock);
            //     //     // }

            //     //     llvm::outs() << "Inst: ";
            //     //     llvmInst->print(llvm::outs());
            //     //     llvm::outs() << "\nOperand: ";
            //     //     llvmInst->getOperand(0)->print(llvm::outs());
            //     //     llvm::outs() << "\n";
            //     //     phi->print(llvm::outs());
            //     //     llvm::outs() << "\n";
            //     //     // update all occurences of this register in this loop
            //     //     // with the phi node
            //     //     bool innerInCurrBlock = false;
            //     //     for (const auto innerStmt : currKernel->getBodyStatements()) {
            //     //         uint innerStmtId = innerStmt->getId();
            //     //         if (innerStmtId > stmtId) break;

            //     //         if (innerStmt->getLabel() == targetValue)
            //     //             innerInCurrBlock = true;
            //     //         // find current block
            //     //         if (!innerInCurrBlock) continue;

            //     //         InstrStatement* innerInstStmt =
            //     //             dynamic_cast<InstrStatement*>(innerStmt.get());

            //     //         if (!innerInstStmt) continue;

            //     //         auto innerLlvmValues =
            //     //             PtxToLlvmIrConverter::getPtxToLlvmMapValue(innerStmtId);
            //     //         if (innerLlvmValues.size() == 0) continue;
            //     //         std::pair<llvm::Value*, llvm::BasicBlock*> innerllvmValue =
            //     //             innerLlvmValues[0];

            //     //         llvm::Instruction* innerLlvmInst =
            //     //             llvm::dyn_cast<llvm::Instruction>(innerllvmValue.first);

            //     //         if (!innerLlvmInst) continue;

            //     //         // Check all source operands for the register
            //     //         // and update the value with the phi node if found
            //     //         for (const auto &sourceOp : innerInstStmt->getSourceOps()) {
            //     //             auto sourceOpValue = sourceOp->getValue();
            //     //             std::string* sourceOpName =
            //     //                 std::get_if<std::string>(&sourceOpValue);

            //     //             if (!sourceOpName) {
            //     //                 AddressExpr* addrExpr =
            //     //                     std::get_if<AddressExpr>(&sourceOpValue);
            //     //                 if (addrExpr) {
            //     //                     auto addrExprOpValue =
            //     //                         addrExpr->getFirstOperand()->getValue();
            //     //                     sourceOpName = std::get_if<std::string>(
            //     //                         &addrExprOpValue
            //     //                     );
            //     //                 }
            //     //             }
                            
            //     //             if (sourceOpName && *sourceOpName == *destName) {
            //     //                 innerLlvmInst->setOperand(0, phi);
            //     //             }
            //     //         }

            //     //         // Check destination register, in case that this is
            //     //         // a store instruction that uses the register
            //     //         if (innerInstStmt->getDestOps().size() > 0) {
            //     //             auto destOpValue =
            //     //                 innerInstStmt->getDestOps()[0]->getValue();
            //     //             AddressExpr* addrExpr =
            //     //                 std::get_if<AddressExpr>(&destOpValue);
            //     //             if (!addrExpr) continue;

            //     //             auto addrExprOpValue =
            //     //                 addrExpr->getFirstOperand()->getValue();
            //     //             std::string* destOpName = std::get_if<std::string>(
            //     //                 &addrExprOpValue
            //     //             );

            //     //             if (destOpName && *destOpName == *destName) {
            //     //                 uint opcode = innerLlvmInst->getOpcode();
            //     //                 // case that there is and addition in the
            //     //                 // accessed address
            //     //                 if (opcode == llvm::Instruction::Add)
            //     //                     innerLlvmInst->setOperand(0, phi);
            //     //                 else if (opcode == llvm::Instruction::Store)
            //     //                     innerLlvmInst->setOperand(1, phi);
            //     //             }
            //     //         }

            //     //         // llvmInst->setOperand(0, phi);
            //     //     }
                // iterCount++;
        }
    }
    else if (Inst == "ret") {
        llvm::Value* ret = PtxToLlvmIrConverter::Builder->CreateRetVoid();
        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(ret, currBlock)
        );
    }
    else if (Inst == "bar") {
        // generate call to llvm.nvvm.barrier0 intrinsic
        llvm::FunctionType *ft = llvm::FunctionType::get(
            llvm::Type::getVoidTy(*PtxToLlvmIrConverter::Context),
            false
        );
        std::string funcName = "llvm.nvvm.barrier0";

        llvm::FunctionCallee func =
            PtxToLlvmIrConverter::Module->getOrInsertFunction(funcName, ft);
        llvm::Value *call =
            PtxToLlvmIrConverter::Builder->CreateCall(func);

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(call, currBlock)
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

    // check if instruction contains rn modifier
    // and generate call to roundeven intrinsic
    auto rnIter = std::find(Modifiers.begin(), Modifiers.end(), "rn");
    if (rnIter != Modifiers.end()) {
        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();

        llvm::Value* sourceOpValue = GetLlvmOperandValue(SourceOps[0]);
        
        llvm::Type *funcType = PtxToLlvmIrConverter::GetTypeMapping(
            Types[0]
        )(*PtxToLlvmIrConverter::Context);

        std::string funcTypeStr;
        llvm::raw_string_ostream funcStream(funcTypeStr);
        funcType->print(funcStream);
        funcTypeStr = funcStream.str();

        std::string destTypeBitsStr = std::regex_replace(
            Types[0],
            std::regex("[a-z]+"),
            std::string("$1")
        );
        int destTypeBits = stoi(destTypeBitsStr);

        llvm::Value* callParamValue = sourceOpValue;
        llvm::Type* floatType = nullptr;
        if (sourceOpValue->getType()->isIntegerTy()) {
            if (destTypeBits == 32)
                floatType = llvm::Type::getFloatTy(*PtxToLlvmIrConverter::Context);
            else
                floatType = llvm::Type::getDoubleTy(*PtxToLlvmIrConverter::Context);

            llvm::Value* itofp = nullptr;
            if (Types[0][0] == 's') {    
                itofp = PtxToLlvmIrConverter::Builder->CreateUIToFP(
                    sourceOpValue, floatType
                );
            }
            else {
                itofp = PtxToLlvmIrConverter::Builder->CreateSIToFP(
                    sourceOpValue, floatType
                );
            }
            callParamValue = itofp;
            genLlvmInstructions.push_back(
                std::pair<llvm::Value*, llvm::BasicBlock*>(itofp, currBlock)
            );
        }

        std::string sourceTypeStr = 'f' + destTypeBitsStr;

        std::vector<llvm::Type*> params;
        if (floatType)
            params.push_back(floatType);
        else params.push_back(sourceOpValue->getType());

        llvm::FunctionType *ft = llvm::FunctionType::get(
            funcType,
            params,
            false
        );

        std::string funcName = "llvm.roundeven." + sourceTypeStr;

        llvm::FunctionCallee func =
            PtxToLlvmIrConverter::Module->getOrInsertFunction(funcName, ft);
        llvm::Value *call =
            PtxToLlvmIrConverter::Builder->CreateCall(func, callParamValue);

        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(call, currBlock)
        );
    }

    // Check if instruction contains lo modifier and generate an and instruction
    // to keep lower bits of the results
    auto loIter = std::find(Modifiers.begin(), Modifiers.end(), "lo");
    if (loIter != Modifiers.end()) {
        llvm::Value* firstSourceOpValue = GetLlvmOperandValue(SourceOps[0]);
        llvm::Type* firstSourceOpType = firstSourceOpValue->getType();

        llvm::Value* lastInst = genLlvmInstructions.back().first;

        llvm::APInt mask(
            firstSourceOpValue->getType()->getIntegerBitWidth(), 0xFFFFFFFF
        );

        llvm::Value* andInst = PtxToLlvmIrConverter::Builder->CreateAnd(
            lastInst,
            llvm::ConstantInt::get(lastInst->getType(), mask)
        );

        llvm::BasicBlock* currBlock =
            PtxToLlvmIrConverter::Builder->GetInsertBlock();
        genLlvmInstructions.push_back(
            std::pair<llvm::Value*, llvm::BasicBlock*>(andInst, currBlock)
        );
    }

    PtxToLlvmIrConverter::setPtxToLlvmMapValue(getId(), genLlvmInstructions);
    
    if (Inst != "bra") {
        llvm::BasicBlock* currBlock = PtxToLlvmIrConverter::Builder->GetInsertBlock();
        if (
            DestOps.size() > 0                             && 
            DestOps[0]->getType() == OperandType::Register &&
            genLlvmInstructions.size() > 0
        ) {
            std::string destOpName = std::get<std::string>(
                DestOps[0]->getValue()
            );

            PtxToLlvmIrConverter::setRegToValueOutMapValue(
                currBlock, destOpName, genLlvmInstructions.back().first
            );
        }
    }

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