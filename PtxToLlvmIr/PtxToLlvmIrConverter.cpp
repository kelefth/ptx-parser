#include "PtxToLlvmIrConverter.h"
#include "../PTX/Statement.h"
#include "../PTX/KernelDirectStatement.h"

const std::map<std::string, typeFunc> PtxToLlvmIrConverter::TypeMap {
    {"s8", llvm::Type::getInt8Ty},
    {"s16", llvm::Type::getInt16Ty},
    {"s32", llvm::Type::getInt32Ty},
    {"s64", llvm::Type::getInt64Ty},
    {"u8", llvm::Type::getInt8Ty},
    {"u16", llvm::Type::getInt16Ty},
    {"u32", llvm::Type::getInt32Ty},
    {"u64", llvm::Type::getInt64Ty},
    {"b8", llvm::Type::getInt8Ty},
    {"b16", llvm::Type::getInt16Ty},
    {"b32", llvm::Type::getInt32Ty},
    {"b64", llvm::Type::getInt64Ty},
    {"f16", llvm::Type::getHalfTy},
    {"f32", llvm::Type::getFloatTy},
    {"f64", llvm::Type::getDoubleTy},
    {"pred", llvm::Type::getInt1Ty}
};

std::unordered_map<int, std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>>>
PtxToLlvmIrConverter::PtxToLlvmMap;

std::map<llvm::BasicBlock*, std::map<std::string, llvm::Value*>>
PtxToLlvmIrConverter::RegToValueInMap;

std::map<llvm::BasicBlock*, std::map<std::string, llvm::Value*>>
PtxToLlvmIrConverter::RegToValueOutMap;


std::unique_ptr<llvm::LLVMContext> PtxToLlvmIrConverter::Context;
std::unique_ptr<llvm::IRBuilder<llvm::NoFolder>> PtxToLlvmIrConverter::Builder;
std::unique_ptr<llvm::Module> PtxToLlvmIrConverter::Module;

void PtxToLlvmIrConverter::Initialize() {
    Context = std::make_unique<llvm::LLVMContext>();
    Module = std::make_unique<llvm::Module>("LLVM Module", *Context);
    Builder = std::make_unique<llvm::IRBuilder<llvm::NoFolder>>(*Context);
}

std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>>
PtxToLlvmIrConverter::getPtxToLlvmMapValue(int stmtId) {
    return PtxToLlvmMap[stmtId];
}

void PtxToLlvmIrConverter::setPtxToLlvmMapValue(
    int stmtId,
    std::vector<std::pair<llvm::Value*, llvm::BasicBlock*>> val
) {
    PtxToLlvmMap[stmtId] = val;
}

void PtxToLlvmIrConverter::removePtxToLlvmMapValue(int stmtId) {
    PtxToLlvmMap.erase(stmtId);
}

std::map<llvm::BasicBlock*, std::map<std::string, llvm::Value*>>
PtxToLlvmIrConverter::getRegToValueInMaps() {
    return RegToValueInMap;
}

std::map<llvm::BasicBlock*, std::map<std::string, llvm::Value*>>
PtxToLlvmIrConverter::getRegToValueOutMaps() {
    return RegToValueOutMap;
}

llvm::Value* PtxToLlvmIrConverter::getRegToValueInMapValue(
    llvm::BasicBlock* bb,
    std::string regName
) {
    llvm::Value* value = nullptr;
    if (!RegToValueInMap[bb].empty())
        value = RegToValueInMap[bb][regName];
    
    return value;
}

void PtxToLlvmIrConverter::setRegToValueInMapValue(
    llvm::BasicBlock* bb,
    std::string regName,
    llvm::Value* value
) {
    RegToValueInMap[bb][regName] = value;
}

void PtxToLlvmIrConverter::setRegToValueInMap(
    llvm::BasicBlock* bb,
    std::map<std::string, llvm::Value*> map
) {
    RegToValueInMap[bb] = map;
}

std::map<std::string, llvm::Value*>
PtxToLlvmIrConverter::getRegToValueInMap(llvm::BasicBlock* bb) {
    return RegToValueInMap[bb];
}

void PtxToLlvmIrConverter::clearRegToValueInMap(llvm::BasicBlock* bb) {
    RegToValueInMap[bb].clear();
}

llvm::Value* PtxToLlvmIrConverter::getRegToValueOutMapValue(
    llvm::BasicBlock* bb,
    std::string regName
) {
    llvm::Value* value = nullptr;
    if (!RegToValueOutMap[bb].empty())
        value = RegToValueOutMap[bb][regName];
    
    return value;
}

void PtxToLlvmIrConverter::setRegToValueOutMapValue(
    llvm::BasicBlock* bb,
    std::string regName,
    llvm::Value* value
) {
    RegToValueOutMap[bb][regName] = value;
}

void PtxToLlvmIrConverter::setRegToValueOutMap(
    llvm::BasicBlock* bb,
    std::map<std::string, llvm::Value*> map
) {
    RegToValueOutMap[bb] = map;
}

std::map<std::string, llvm::Value*>
PtxToLlvmIrConverter::getRegToValueOutMap(llvm::BasicBlock* bb) {
    return RegToValueOutMap[bb];
}

void PtxToLlvmIrConverter::clearRegToValueOutMap(llvm::BasicBlock* bb) {
    RegToValueOutMap[bb].clear();
}

void PtxToLlvmIrConverter::UpdateMapsAndGeneratePhis(llvm::BasicBlock* bb) {
    // Update the in map of the next block and generate phis
    std::map<std::string, llvm::Value*> inMap;
    std::set<std::string> multiIncomingRegs;
    for (llvm::BasicBlock* pred : llvm::predecessors(bb)) {
        std::map<std::string, llvm::Value*> predOutMap =
            PtxToLlvmIrConverter::getRegToValueOutMap(pred);
            // if (predOutMap["%rd67"]) {
            //     predOutMap["%rd67"]->print(llvm::outs());
            //     llvm::outs() << "\n";
            // }
        for (const auto& mapPair : predOutMap) {
            auto result = inMap.insert(mapPair);
            bool isInsertSuccessful = result.second;
            if (!isInsertSuccessful) {
                // If the insertion is unsuccesful there are incoming values
                // for the same register from multiple preds
                multiIncomingRegs.insert(mapPair.first);
            }
        }
    }

    // Generate phi node if the reg is in the out map of multiple preds
    for (const std::string& reg : multiIncomingRegs) {
        llvm::PHINode* phi = nullptr;
        for (llvm::BasicBlock* pred : llvm::predecessors(bb)) {
            std::map<std::string, llvm::Value*> predOutMap =
                PtxToLlvmIrConverter::getRegToValueOutMap(pred);
            llvm::Value* value =
                PtxToLlvmIrConverter::getRegToValueOutMapValue(pred, reg);
            // if (reg == "%rd67" && value) {
            //     value->print(llvm::outs());
            //     llvm::outs() << "\n";
            // }
            // TODO: review
            if (!value) break;
            if (!phi) {
                llvm::BasicBlock::iterator firstInsertionPt = bb->getFirstInsertionPt();
                // Check if the block is empty or not
                if (firstInsertionPt == bb->end())
                    phi = llvm::PHINode::Create(value->getType(), 0, "", bb);
                else 
                    phi = llvm::PHINode::Create(value->getType(), 0, "", &*firstInsertionPt);
            }
            // if (value == phi) {
            //     phi->print(llvm::outs());
            //     llvm::outs() << "\n";
            // }
            phi->addIncoming(value, pred);
        }

        if (phi && phi->getNumIncomingValues() < llvm::pred_size(bb)) {
            phi->eraseFromParent();
            phi = nullptr;
        }
        else if (phi) {
            std::vector<llvm::Value*> values;
            for (llvm::Value* incVal : phi->incoming_values()) {
                if(std::find(values.begin(), values.end(), incVal) != values.end()) {
                    phi->eraseFromParent();
                    inMap[reg] = incVal;
                    phi = nullptr;
                    break;
                }
                values.push_back(incVal);
            }

            if (phi) {
                bool haveSameIncValues = false;
                uint incValNum = phi->getNumIncomingValues();
                for (uint i = 0; i < incValNum - 1; ++i) {
                    llvm::Value* incVal = phi->getIncomingValue(i);
                    llvm::PHINode* innerPhi = llvm::dyn_cast<llvm::PHINode>(incVal);
                    llvm::Value* nextIncVal = phi->getIncomingValue(i + 1);
                    llvm::PHINode* nextInnerPhi = llvm::dyn_cast<llvm::PHINode>(nextIncVal);
                    if (innerPhi && nextInnerPhi) {
                        uint inIncValNum = innerPhi -> getNumIncomingValues();
                        uint nextInIncValNum = nextInnerPhi -> getNumIncomingValues();
                        if (incValNum != nextInIncValNum) break;
                        for (uint i = 0; i < inIncValNum; ++i) {
                            haveSameIncValues =
                                innerPhi->getIncomingValue(i) == nextInnerPhi->getIncomingValue(i) &&
                                innerPhi->getIncomingBlock(i) == nextInnerPhi->getIncomingBlock(i);
                        }
                    }
                }
                if (haveSameIncValues) {
                    inMap[reg] = phi->getIncomingValue(0);
                    phi->eraseFromParent();
                    phi = nullptr;
                    break;
                }
            }
        }

        if (phi) {
            inMap[reg] = phi;
            // phi->print(llvm::outs());
            // llvm::outs() << "\n";
        }
    }

    PtxToLlvmIrConverter::setRegToValueInMap(bb, inMap);
    // Initialize the out map too, with the values of in map
    PtxToLlvmIrConverter::setRegToValueOutMap(bb, inMap);
}

typeFunc PtxToLlvmIrConverter::GetTypeMapping(std::string type) {
    return TypeMap.at(type);
}

llvm::ICmpInst::Predicate PtxToLlvmIrConverter::ConvertPtxToLlvmPred(
    std::string pred
) {
    if (pred == "eq")
        return llvm::CmpInst::ICMP_EQ;
    else if (pred == "ne")
        return llvm::CmpInst::ICMP_NE;
    else if (pred == "ge")
        return llvm::CmpInst::ICMP_SGE;
    else if (pred == "le")
        return llvm::CmpInst::ICMP_SLE;
    else if (pred == "gt")
        return llvm::CmpInst::ICMP_SGT;
    else if (pred == "lt")
        return llvm::CmpInst::ICMP_SLT;

    return llvm::CmpInst::BAD_ICMP_PREDICATE;
}

llvm::BasicBlock* PtxToLlvmIrConverter::GetBasicBlock(
    llvm::Function* func,
    std::string name
 ) {
    for (llvm::BasicBlock &block : *func) {
        if (block.getName() == name)
            return &block;
    }

    return nullptr;
}

uint PtxToLlvmIrConverter::ConvertPtxToLlvmAddrSpace(std::string addressSpace) {
    if (addressSpace == "global") return 1;
    else if (addressSpace == "shared") return 3;
    else if (addressSpace == "local") return 5;

    return 0;
}