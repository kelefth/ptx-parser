#include <iostream>
#include <variant>
#include <algorithm>
#include <regex>
#include <cmath>

#include "parser.h"
#include "../InstrStatement.h"
#include "../DirectStatement.h"
#include "../ModuleDirectStatement.h"
#include "../ParamDirectStatement.h"
#include "../KernelDirectStatement.h"
#include "../LinkingDirectStatement.h"
#include "../VarDecDirectStatement.h"
#include "../Operand.h"

#include "llvm/Pass.h"
#include "llvm/IR/Dominators.h"
#include "llvm/IR/PassManager.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Verifier.h"
#include "llvm/IR/InstIterator.h"
#include "llvm/Passes/PassBuilder.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Utils.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include "llvm/Transforms/Scalar/Reassociate.h"
#include "llvm/Transforms/Scalar/SimplifyCFG.h"
#include "llvm/Analysis/LoopInfo.h"
#include "llvm/Analysis/AliasAnalysis.h"
#include "llvm/Analysis/CGSCCPassManager.h"
#include "llvm/Analysis/AssumptionCache.h"
#include "llvm/Analysis/BasicAliasAnalysis.h"
#include "llvm/Analysis/MemoryDependenceAnalysis.h"
#include "llvm/Analysis/MemorySSA.h"
#include "llvm/Analysis/OptimizationRemarkEmitter.h"
#include "llvm/Analysis/ProfileSummaryInfo.h"
#include "llvm/Analysis/TargetTransformInfo.h"
#include "llvm/Analysis/ScalarEvolution.h"
#include "llvm/Analysis/LoopAnalysisManager.h"
#include "llvm/Support/raw_ostream.h"

#include "z3++.h"
#include "z3.h"

//////////////////////////////////////////////////////////////////////////////
// #include "llvm/IRReader/IRReader.h"
// #include "llvm-c/IRReader.h"
// #include "llvm/AsmParser/Parser.h"
// #include "llvm/Bitcode/BitcodeReader.h"
// #include "llvm/IR/LLVMContext.h"
// #include "llvm/IR/Module.h"
// #include "llvm/Support/MemoryBuffer.h"
// #include "llvm/Support/SourceMgr.h"
// #include "llvm/Support/Timer.h"
// #include "llvm/Support/raw_ostream.h"
//////////////////////////////////////////////////////////////////////////////

using namespace llvm;

int currentToken;
// std::vector<std::unique_ptr<Statement>> statements;
// std::vector<InstrStatement> statements;
unsigned int IdCounter = 0;
std::vector<std::shared_ptr<Statement>> statements;

bool isInstrToken(int token) {

    switch (token) {
        case token_abs:
        case token_activemask:
        case token_add:
        case token_addc:
        case token_alloca:
        case token_and:
        case token_applypriority:
        case token_atom:
        case token_bar:
        case token_barrier:
        case token_bfe:
        case token_bfi:
        case token_bfind:
        case token_bmsk:
        case token_bra:
        case token_brev:
        case token_brkpt:
        case token_brx:
        case token_call:
        case token_cIz:
        case token_cnot:
        case token_copysign:
        case token_cos:
        case token_cp:
        case token_createpolicy:
        case token_cvt:
        case token_cvta:
        case token_discard:
        case token_div:
        case token_dp2a:
        case token_dp4a:
        case token_elect:
        case token_ex2:
        case token_exit:
        case token_fence:
        case token_fma:
        case token_fns:
        case token_getctarank:
        case token_griddepcontrol:
        case token_Idu:
        case token_isspacep:
        case token_istypep:
        case token_ld:
        case token_ldmatrix:
        case token_lg2:
        case token_lop3:
        case token_mad:
        case token_mad24:
        case token_madc:
        case token_mapa:
        case token_match:
        case token_max:
        case token_mbarrier:
        case token_membar:
        case token_min:
        case token_mma:
        case token_mov:
        case token_movmatrix:
        case token_mu124:
        case token_mul:
        case token_multimem:
        case token_nanosleep:
        case token_neg:
        case token_not:
        case token_or:
        case token_pmevent:
        case token_popc:
        case token_prefetch:
        case token_prefetchu:
        case token_prmt:
        case token_rcp:
        case token_red:
        case token_redux:
        case token_rem:
        case token_ret:
        case token_rsqrt:
        case token_sad:
        case token_selp:
        case token_set:
        case token_setmaxnreg:
        case token_setp:
        case token_shf:
        case token_shfl:
        case token_shl:
        case token_shr:
        case token_sin:
        case token_slct:
        case token_sqrt:
        case token_st:
        case token_stackrestore:
        case token_stacksave:
        case token_stmatrix:
        case token_sub:
        case token_subc:
        case token_suld:
        case token_suq:
        case token_sured:
        case token_sust:
        case token_szext:
        case token_tanh:
        case token_testp:
        case token_tex:
        case token_tld4:
        case token_trap:
        case token_txq:
        case token_vabsdiff:
        case token_vabsdiff2:
        case token_vabsdiffh:
        case token_vadd:
        case token_vadd2:
        case token_vadd4:
        case token_vavrg2:
        case token_vavrgh:
        case token_vmad:
        case token_vmax:
        case token_vmax2:
        case token_vmax4:
        case token_vmin:
        case token_vmin2:
        case token_vmin4:
        case token_vote:
        case token_vset:
        case token_vset2:
        case token_vset4:
        case token_vshl:
        case token_vshr:
        case token_vsub:
        case token_vsub2:
        case token_vsub4:
        case token_wgmma:
        case token_wmma:
        case token_xor:
            return true;
        default:
            return false;
    }
}

// bool isAddress() {
//     char nextChar = getchar();
//     bool isOpAddress = nextChar == ']';

//     // Unread next character
//     ungetc(nextChar, stdin);

//     return isOpAddress;
// }

void ParseInstrStatement() {

    std::string label = "";
    std::string pred = "";
    std::string inst = "";
    std::vector<std::string> modifiers;
    std::vector<std::string> types;
    // std::vector<std::variant<std::string, double>> destOps;
    // std::vector<std::variant<std::string, double>> sourceOps;
    std::vector<std::unique_ptr<Operand>> destOps;
    std::vector<std::unique_ptr<Operand>> sourceOps;

    bool passedDest = false;
    bool inAddress = false;
    bool isAlreadyAddress = false;
    bool inVector = false;

    while (currentToken != token_semicolon) {

        if (isInstrToken(currentToken)) {
            inst = currStrVal;
        }
        else {
            OperandType opType;

            switch (currentToken) {
                case token_label:
                    label = currStrVal;
                    break;
                case token_pred:
                    pred = currStrVal;
                    break;
                case token_modifier:
                    // remove dot
                    modifiers.push_back(currStrVal.erase(0,1));
                    break;
                case token_type:
                    // remove dot
                    types.push_back(currStrVal.erase(0,1));
                    break;
                case token_reg:
                    if (inAddress) {
                        isAlreadyAddress = true;

                        opType = OperandType::Address;
                        OperandType addrOpType = OperandType::Register;
                        AddressExpr addressExpr(std::make_shared<Operand>(currStrVal, addrOpType));

                        if (passedDest)
                            sourceOps.push_back(std::move(std::make_unique<Operand>(addressExpr, opType)));
                        else
                            destOps.push_back(std::move(std::make_unique<Operand>(addressExpr, opType)));
                    }
                    else {
                        opType = OperandType::Register;

                        if (passedDest)
                            sourceOps.push_back(std::move(std::make_unique<Operand>(currStrVal, opType)));
                        else
                            destOps.push_back(std::move(std::make_unique<Operand>(currStrVal, opType)));
                    }

                    break;
                case token_number:
                    if (inAddress) {
                        opType = OperandType::Address;

                        if (isAlreadyAddress) {
                            if (passedDest) {
                                AddressExpr lastAddressExpr = std::get<AddressExpr>(sourceOps.back()->getValue());
                                lastAddressExpr.setSecondOperand(std::make_shared<Operand>(currNumVal, OperandType::Immediate));
                                sourceOps.back()->setValue(lastAddressExpr);
                            }
                            else {
                                AddressExpr lastAddressExpr = std::get<AddressExpr>(destOps.back()->getValue());
                                lastAddressExpr.setSecondOperand(std::make_shared<Operand>(currNumVal, OperandType::Immediate));
                                destOps.back()->setValue(lastAddressExpr);
                            }
                        }
                    }
                    else {
                        opType = OperandType::Immediate;                

                        if (passedDest)
                            sourceOps.push_back(std::move(std::make_unique<Operand>(currNumVal, opType)));
                        else
                            destOps.push_back(std::move(std::make_unique<Operand>(currNumVal, opType)));
                    }

                    break;
                case token_id:
                    if (inAddress) {
                        isAlreadyAddress = true;

                        opType = OperandType::Address;
                        
                        AddressExpr addressExpr(std::make_shared<Operand>(currStrVal, opType));

                        if (passedDest)
                            sourceOps.push_back(std::move(std::make_unique<Operand>(addressExpr, opType)));
                        else
                            destOps.push_back(std::move(std::make_unique<Operand>(addressExpr, opType)));
                    }
                    else {
                        opType = OperandType::Label;

                        if (passedDest)
                            sourceOps.push_back(std::move(std::make_unique<Operand>(currStrVal, opType)));
                        else
                            destOps.push_back(std::move(std::make_unique<Operand>(currStrVal, opType)));
                    }

                    break;
                case token_dim:
                    // Set dimension in previous operand (remove dot)
                    if (passedDest)
                        sourceOps.back()->setDimension(currStrVal.erase(0,1));
                    else
                        destOps.back()->setDimension(currStrVal.erase(0,1));
                    break;
                case token_comma:
                    if (!inVector)
                        passedDest = true;
                    break;
                case token_leftbracket:
                    inAddress = true;
                    break;
                case token_rightbracket:
                    inAddress = false;
                    break;
                case token_leftcurlybracket:
                    inVector = true;
                    break;
                case token_rightcurlybracket:
                    inVector = false;
                    break;
                case token_plus:
                    // Update operation on last address expression
                    if (isAlreadyAddress) {
                        if (passedDest) {
                            AddressExpr lastAddressExpr = std::get<AddressExpr>(sourceOps.back()->getValue());
                            lastAddressExpr.setOperation("+");
                            sourceOps.back()->setValue(lastAddressExpr);
                        }
                        else {
                            AddressExpr lastAddressExpr = std::get<AddressExpr>(destOps.back()->getValue());
                            lastAddressExpr.setOperation("+");
                            destOps.back()->setValue(lastAddressExpr);
                        }
                    }

                    break;
                default:
                    break;
            }
        }

        currentToken = getToken();
    }

    // statements.push_back(std::unique_ptr<Statement>(new InstrStatement(label, inst, modifiers, type, destOps, sourceOps)));
    // statements.push_back(std::make_unique<InstrStatement>(label, pred, inst, modifiers, types, std::move(destOps), std::move(sourceOps)));

    // Find last kernel statement and add the current statement to it
    auto lastKernelStmtPtr = std::find_if(statements.rbegin(), statements.rend(),
                                        [](const std::shared_ptr<Statement>& stmt) {
                                            return dynamic_cast<const KernelDirectStatement*>(stmt.get()) != nullptr;
                                        });

    KernelDirectStatement* kernelStmtPtr = static_cast<KernelDirectStatement*>(lastKernelStmtPtr->get());
    // auto kernelStmtPtr = std::make_shared<KernelDirectStatement>(*(static_cast<KernelDirectStatement*>(lastKernelStmtPtr->get())));
    unsigned int kernelId = kernelStmtPtr->getId();
    kernelStmtPtr->AddBodyStatement(
        std::make_shared<InstrStatement>(
            IdCounter++,
            label,
            kernelId,
            pred,
            inst,
            modifiers,
            types,
            std::move(destOps),
            std::move(sourceOps)
        )
    );

}

void ParseVarDecDirectStatement() {
    int prevToken;
    std::string directive = "";
    std::string addressSpace = "";
    std::string identifier = "";
    std::string type = "";
    std::string label = "";
    int alignment = 0;
    int size = 0;
    bool isArray = false;

    while (currentToken != token_semicolon) {
        switch (currentToken) {
            case token_label:
                label = currStrVal;
                break;
            case token_global_dir:
            case token_shared_dir:
            case token_local_dir:
                addressSpace = currStrVal.erase(0,1);
                directive = addressSpace;
                break;
            case token_type:
                type = currStrVal;
                break;
            case token_id:
                identifier = currStrVal;
                break;
            case token_number:
                if (prevToken == token_align_dir)
                    alignment = currNumVal;
                else if (isArray)
                    size = currNumVal;
            case token_leftbracket:
                isArray = true;
                break;
            case token_rightbracket:
                isArray = false;
                break;
            default:
                break;
        }

        prevToken = currentToken;
        currentToken = getToken();
    }

    auto lastKernelStmtPtr = std::find_if(statements.rbegin(), statements.rend(),
                                        [](const std::shared_ptr<Statement>& stmt) {
                                            return dynamic_cast<const KernelDirectStatement*>(stmt.get()) != nullptr;
                                        });

    KernelDirectStatement* kernelStmtPtr = static_cast<KernelDirectStatement*>(lastKernelStmtPtr->get());
    unsigned int kernelId = kernelStmtPtr->getId();
    kernelStmtPtr->AddBodyStatement(
        std::make_shared<VarDecDirectStatement>(
            IdCounter++,
            label,
            directive,
            addressSpace,
            alignment,
            type,
            identifier,
            size
        )
    );
    
}

void ParseModuleDirectStatement() {

    std::string directive = currStrVal;
    getToken();
    std::string value = "";
    if (currStrVal != "")
        value = currStrVal;
    else value = std::to_string(currNumVal); 

    statements.push_back(std::make_shared<ModuleDirectStatement>(
        IdCounter++,
        directive,
        value
    ));
}

void ParseLinkingDirectStatemnt() {
    int prevToken;
    std::string directive = "";
    std::string addressSpace = "";
    std::string identifier = "";
    std::string type = "";
    std::string label = "";
    int alignment = 0;
    int size = 0;
    bool isArray = false;

    while (currentToken != token_semicolon) {
        switch (currentToken) {
            case token_label:
                label = currStrVal;
                break;
            case token_weak_dir:
            case token_extern_dir:
            case token_visible_dir:
            case token_common_dir:
                directive = currStrVal;
                break;
            case token_global_dir:
            case token_shared_dir:
            case token_local_dir:
                addressSpace = currStrVal.erase(0,1);
                break;
            case token_type:
                type = currStrVal;
                break;
            case token_id:
                identifier = currStrVal;
                break;
            case token_number:
                if (prevToken == token_align_dir)
                    alignment = currNumVal;
                else if (isArray)
                    size = currNumVal;
            case token_leftbracket:
                isArray = true;
                break;
            case token_rightbracket:
                isArray = false;
                break;
            default:
                break;
        }

        prevToken = currentToken;
        currentToken = getToken();
    }

    statements.push_back(
        std::make_shared<LinkingDirectStatement>(
            IdCounter++,
            label,
            directive,
            addressSpace,
            alignment,
            type,
            identifier,
            size
        )
    );
}

void ParseParamDirectStatement() {

    std::string label = "";
    std::string type = "";
    std::string name = "";
    int alignment = 0;
    int size = 0;
    int prevToken;
    bool isArray = false;

    while ((currentToken != token_comma) && (currentToken != token_rightparenth)) {

        switch (currentToken) {
            case token_label:
                label = currStrVal;
                break;
            case token_type:
                // remove dot
                type = currStrVal.erase(0,1);
                break;
            case token_id:
                name = currStrVal;
                break;
            case token_number:
                if (prevToken == token_align_dir)
                    alignment = currNumVal;
                else if (isArray)
                    size = currNumVal;
                break;
            case token_leftbracket:
                isArray = true;
                break;
            case token_rightbracket:
                isArray = false;
                break;
            default:
                break;
        }

        prevToken = currentToken;
        currentToken = getToken();
    }

    
    // Get pointer to the last statement stored, which is an entry directive
    KernelDirectStatement* stmtPtr = static_cast<KernelDirectStatement*>(
        statements.back().get()
    );
    // auto stmtPtr = std::make_shared<KernelDirectStatement>(*(static_cast<KernelDirectStatement*>(statements.back().get())));
    // Add parameter to last kernel's parameters
    stmtPtr->AddParameter(
        std::make_shared<ParamDirectStatement>(
            IdCounter++, label, name, type, alignment, size
        )
    );
}

void ParseKernelDirectStatement() {
    std::string label = "";

    while (currentToken != token_leftcurlybracket) {

        switch (currentToken) {
            case token_label:
                label = currStrVal;
                break;
            case token_id:
                // Create kernel statement and add it to statements
                statements.push_back(
                    std::make_shared<KernelDirectStatement>(IdCounter++, label, currStrVal)
                );
                break;
            case token_param_dir:
                ParseParamDirectStatement();
            default:
                break;
        }

        currentToken = getToken();
    }
}

void dump_statements() {

    for(auto const& statement : statements) {
        statement->dump();
        std::cout << std::endl;
    }
}

Instruction* FindMulShlInUses(
    llvm::iterator_range<llvm::Value::use_iterator> uses
) {
    for (llvm::Use &use : uses) {
        Instruction* useInst = dyn_cast<Instruction>(use.get());

        if (useInst == nullptr) continue;
        
        uint instOpcode = useInst->getOpcode();
        if (
            (instOpcode == Instruction::Mul || instOpcode == Instruction::Shl) &&
            dyn_cast<ConstantInt>(useInst->getOperand(1)) != nullptr
        )
            return useInst;

        if (useInst->getNumOperands() > 0)
            return FindMulShlInUses(useInst->getOperand(0)->uses());
    }

    return nullptr;
}

// Value* FindValueLeaf(Value* value) {
//     Instruction* valueInst = dyn_cast<Instruction>(value);
//     if (!valueInst) return value;

//     for (Use &use : value->uses()) {
        
//     }
// }

std::string ConvertToZ3Syntax(Value* value, bool isBitwiseOp) {
    SmallString<64> smResult;
    if (const ConstantInt* intConstValue = dyn_cast<ConstantInt>(value)) {
        intConstValue->getValue().toString(smResult, 10, true);
        if (isBitwiseOp)
            return "((_ int2bv 32) " + std::string(smResult.str()) + ")";
        
        return std::string(smResult.str());
    }
    else if (const ConstantFP* fpConstValue = dyn_cast<ConstantFP>(value)) {
        fpConstValue->getValueAPF().toString(smResult, 10, true);
        return std::string(smResult.str());
    }
    else {
        if (isBitwiseOp)
            return ("((_ int2bv 32) %{" + value->getName().str() + "})");

        return ("%{" + value->getName().str() + "}");
    }
}

// Traverse values and convert the expression to string
std::string UnfoldValue(
    Value* value,
    std::vector<std::string>* decls,
    std::set<Value*>& visitedValues,
    // std::map<Value*, std::string> loopVarsMap,
    bool isBitwiseOp = false
) {
    const std::map<std::string, std::string> llvmZ3OperationMap {
        {"add",     "+"},
        {"fadd",    "+"},
        {"sub",     "-"},
        {"fsub",    "-"},
        {"mul",     "*"},
        {"fmul",    "*"},
        {"div",     "/"},
        {"fdiv",    "/"},
        {"and",     "bvand"},
        {"or",      "bvor"}
    };

    visitedValues.insert(value);
    Instruction* valueInst = dyn_cast<Instruction>(value);
    std::string unfoldedStr = "";
    // raw_string_ostream stream(unfoldedStr);
    // Check if terminal value
    if (!valueInst) {
        std::string z3Value = ConvertToZ3Syntax(value, isBitwiseOp);
        // decls->push_back(z3Value);
        return z3Value;
        // value->print(stream);
        // return stream.str();
    }

    // If it's a phi instruction and it's stored as an induction variable
    // get the variables generated name
    // if (
    //     valueInst->getOpcode() == Instruction::PHI &&
    //     loopVarsMap.find(value) != loopVarsMap.end()
    // ) {
    //     return loopVarsMap.at(value);
    // }
    // If it's a phi instruction get the value's name
    if (valueInst->getOpcode() == Instruction::PHI) {
        return value->getName().str();
    }

    // std::string opcode = valueInst->getOpcodeName();
    std::string llvmOpcodeStr = valueInst->getOpcodeName();
    bool isInstInMap =
        llvmZ3OperationMap.find(llvmOpcodeStr) != llvmZ3OperationMap.end();
    
    if (isInstInMap) {
        std::string opcode = llvmZ3OperationMap.at(llvmOpcodeStr);
        unfoldedStr = "(" + opcode;
    }

    // visit the instruction's operands
    uint operandNum = valueInst->getNumOperands();
    for (uint i = 0; i < operandNum; ++i) {
        Value* operand = valueInst->getOperand(i);
        outs() << "Operand:\n";
        operand->print(outs());
        outs() << "\n";
        std::string opValueStr = "";
        if (visitedValues.find(operand) == visitedValues.end()) {
            opValueStr = UnfoldValue(
                operand,
                decls,
                visitedValues,
                // loopVarsMap,
                valueInst->isBitwiseLogicOp()
            );
        }

        // Check if the operand instruction is a bitwise operation
        // and convert the value if the current instruction not a bitwise
        // operation
        Instruction* opInst = dyn_cast<Instruction>(operand);
        if (
            opInst                      &&
            opInst->isBitwiseLogicOp()  &&
            !valueInst->isBitwiseLogicOp()
        ) {
            unfoldedStr += " (bv2int " + opValueStr + ")";
        }
        else
            unfoldedStr += " " + opValueStr;
    }

    if (isInstInMap)
        unfoldedStr += ")";

    return unfoldedStr;
    
}

std::string generateLoopVariable(int index) {
    return "loopVar" + std::to_string(index);
}

std::string generateLoopNumVariable(int index) {
    return "loopNum" + std::to_string(index);
}

std::string generateIndexVariable() {
    static uint counter = 0;
    return "indexVar" + counter++;
}

int main() {
    currentToken = getToken();
    while(currentToken != token_eof) {
        if (isInstrToken(currentToken) || currentToken == token_label || currentToken == token_pred)
            ParseInstrStatement();
        else if (
            currentToken == token_version_dir       ||
            currentToken == token_target_dir        ||
            currentToken == token_address_size_dir 
        ) {
            ParseModuleDirectStatement();
        }
        else if (currentToken == token_entry_dir)
            ParseKernelDirectStatement();
        else if (
            // currentToken == token_weak_dir      ||
            currentToken == token_extern_dir    ||
            currentToken == token_common_dir
        ) {
            ParseLinkingDirectStatemnt();
        }
        else if (
            currentToken == token_local_dir     ||
            currentToken == token_shared_dir    ||
            currentToken == token_global_dir
        ) {
            ParseVarDecDirectStatement();
        }

        currentToken = getToken();
    }

    PtxToLlvmIrConverter::Initialize();

    for(auto statement : statements) {
        statement->ToLlvmIr();
    }

    Module::FunctionListType &funcList =
        PtxToLlvmIrConverter::Module->getFunctionList();
        
    PtxToLlvmIrConverter::Module->print(outs(), nullptr, false, true);
    // dump_statements();

    // Apply modifications to the IR to fix errors
    DominatorTree* dt;
    for (Function &func : funcList) {
        if (func.isDeclaration()) continue;
        // Replace mul/shl and add instruction with pointers as operands
        // with gep instructions
        inst_iterator instIt = inst_begin(func);
        inst_iterator endIt = inst_end(func);
        // store instructions in this vector to remove them after the iteration
        std::vector<Instruction*> instsToRemove;
        for (instIt, endIt; instIt != endIt; ++instIt) {
            Instruction* currInst = &*instIt;
            if (currInst == nullptr) continue;
            uint currInstOpcode = currInst->getOpcode();

            if (currInst->getNumOperands() == 0) continue;

            llvm::Value* firstOperand = currInst->getOperand(0);
            Type::TypeID firstOperandTypeId = firstOperand->getType()->getTypeID();

            if (
                (currInstOpcode != Instruction::Add) || 
                (firstOperandTypeId != Type::PointerTyID)
            )  continue;

            llvm::Value* secondOperand = currInst->getOperand(1);

            // remove previous mul instruction
            llvm::iterator_range<llvm::Value::use_iterator> uses = secondOperand->uses();
            Instruction* mul = FindMulShlInUses(uses);
            if (mul) {
                mul->replaceAllUsesWith(mul->getOperand(0));
                instsToRemove.push_back(mul);
            }

            Value* offset = currInst->getOperand(1);
            // TODO: fix other types
            Type* pointeeType = llvm::Type::getInt32Ty(
                *PtxToLlvmIrConverter::Context
            );
            uint pointeeSize = pointeeType->getIntegerBitWidth() / 8;
            Value* pointeeSizeValue = ConstantInt::get(
                offset->getType(),
                pointeeSize
            );

            // Convert memory offset to index
            // PtxToLlvmIrConverter::Module->print(outs(), nullptr, false, true);
            // outs() << "\n";
            // offset->print(outs());
            // outs() << "\n";

            ConstantInt* constInt = dyn_cast<ConstantInt>(offset);
            Value* indexValue = nullptr;
            if (constInt) {
                int index =
                    constInt->getSExtValue() / pointeeSize;
                indexValue = ConstantInt::get(pointeeType, index);
            }
            else {
                // Create a div instruction if the offset is not a constant
                Value* div = BinaryOperator::CreateUDiv(
                    offset,
                    pointeeSizeValue,
                    "",
                    currInst
                );
                indexValue = div;
            }
            
            Value* indexList[] = { indexValue };
            GetElementPtrInst* gep = GetElementPtrInst::Create(
                pointeeType,
                firstOperand,
                llvm::ArrayRef<llvm::Value*>(indexList, 1),
                "",
                currInst
            );
            
            // replace use of the add instruction and erase it
            currInst->replaceAllUsesWith(gep);
            instsToRemove.push_back(currInst);
        }

        // remove the instructions
        for (Instruction* inst : instsToRemove)
            inst->eraseFromParent();
    }

    // Verification

    PtxToLlvmIrConverter::Module->print(outs(), nullptr, false, true);

    raw_ostream* errStream = &errs();
    for (Function &func : funcList) {
        if (func.isDeclaration()) continue;
        std::string funcName = func.getName().str();
        outs() << "=============================\n"
               << funcName
               << " Verification Results:\n";
        bool verifyFuncRes = verifyFunction(func, errStream);
        // verifyFuncRes ? outs() << "OK\n" : outs() << "NOT OK\n";
        bool verifyModRes = verifyModule(*PtxToLlvmIrConverter::Module, errStream);
        // verifyModRes ? outs() << "OK\n" : outs() << "NOT OK\n";
        outs() << "=============================\n";
    }

    // PtxToLlvmIrConverter::Module->print(outs(), nullptr, false, true);

    // dump_statements();

    // Analysis

    // std::cout << "=============================\nVerification Results:" << std::endl;
    // verifyFunction(*kernel, errStream);
    // verifyModule(*PtxToLlvmIrConverter::Module, errStream);
    // std::cout << "=============================" << std::endl;

    ///////////////////////////////////////////////////////////////////////////
    // static LLVMContext TheContext;
    // SMDiagnostic Err;
    // // std::unique_ptr<Module> Mod = parseIRFile("../CUDA/PTX-samples/CUDA-11.6/matrixMul/matrixMul-cuda-nvptx64-nvidia-cuda-sm_35.ll", Err, TheContext);
    // // std::unique_ptr<Module> Mod = parseIRFile("../../../CUDA/PTX-samples/CUDA-11.6/matrixMul/matrixMul-cuda-nvptx64-nvidia-cuda-sm_35.ll", Err, TheContext);
    // // std::unique_ptr<Module> Mod = parseIRFile("../../../CUDA/PTX-samples/CUDA-11.6/matrixMul/matrixMul-cuda-nvptx64-nvidia-cuda-sm_35-O0.ll", Err, TheContext);
    // std::unique_ptr<Module> Mod = parseIRFile("../../../CUDA/PTX-samples/my-samples/MySimpleMultiCopy/my-output.ll", Err, TheContext);
    // // std::unique_ptr<Module> Mod = parseIRFile("../../../CUDA/PTX-samples/matrixMul/matrixMul-cuda-nvptx64-nvidia-cuda-sm_20.ll", Err, TheContext);
    // // std::unique_ptr<Module> Mod = parseIRFile("../../../CUDA/PTX-samples/LoopInfoTest.ll", Err, TheContext);

    // if (!Mod) {
    //     Err.print("test", errs());
    //     return 1;
    // }

    // Function* function = &Mod->getFunctionList().front();
    // std::cout << function->getName().str() << std::endl;
    ///////////////////////////////////////////////////////////////////////////

    // Apply passes and get loop bounds
    legacy::FunctionPassManager fpm(PtxToLlvmIrConverter::Module.get());    
    fpm.add(createPromoteMemoryToRegisterPass());
    fpm.add(createLoopRotatePass());

    std::vector<std::string> decls;
    // std::map<Value*, std::string> loopIndValuesMap;
    uint loopIndex = 0;
    for (Function &func : funcList) {
        if (func.isDeclaration()) continue;

        fpm.doInitialization();
        fpm.run(func);
        fpm.doFinalization();

        dt = new DominatorTree(func);
        TargetLibraryInfoImpl tlii;
        TargetLibraryInfo tli(tlii);
        AssumptionCache ac(func);
        LoopInfo loopInfo(*dt);
        llvm::ScalarEvolution se(func, tli, ac, *dt, loopInfo);

        std::vector<std::string> generatedVars;
        std::set<std::string> constraints;
        // Get loop bound constraints
        for (BasicBlock &bb : func) {
            Loop* loop = loopInfo.getLoopFor(&bb);
            if (!loop) continue;

            // Generate or get loop variable
            Value* indVar = loop->getInductionVariable(se);
            std::string loopVarName;
            if (indVar->getName() == "") {
                loopVarName = generateLoopVariable(loopIndex);
                indVar->setName(loopVarName);
            }
            else loopVarName = indVar->getName();
            // if (loopIndValuesMap.find(indVar) != loopIndValuesMap.end()) {
            //     loopVarName = loopIndValuesMap.at(indVar);
            // }
            // else {
            //     loopVarName = generateLoopVariable();
            //     loopIndValuesMap[indVar] = loopVarName;
            // }

            // Get loop bounds
            std::optional<llvm::Loop::LoopBounds> bounds = loop->getBounds(se);
            if (bounds == std::nullopt) continue;
            Value* initialValue = &bounds->getInitialIVValue();
            Value* finalValue = &bounds->getFinalIVValue();

            Instruction* initialValueInst = dyn_cast<Instruction>(initialValue);
            Instruction* finalValueInst = dyn_cast<Instruction>(finalValue);
            // outs() << "Initial value: ";
            // initialValue->print(outs());
            // outs() << "\n";
            // outs() << "Final value: ";
            // finalValue->print(outs());
            // outs() << "\n";
            std::string initialValueStr;
            std::string finalValueStr;
            std::set<Value*>visitedValues;
            if (initialValueInst && initialValueInst->isBitwiseLogicOp()) {
                initialValueStr =
                    "(bv2int " +
                    UnfoldValue(initialValue, &decls, visitedValues) +
                    ")";
            }
            else
                initialValueStr =
                    UnfoldValue(initialValue, &decls, visitedValues);

            visitedValues.clear();
            if (finalValueInst && finalValueInst->isBitwiseLogicOp()) {
                finalValueStr =
                    "(bv2int " +
                    UnfoldValue(finalValue, &decls, visitedValues) +
                    ")";
            }
            else
                finalValueStr =
                    UnfoldValue(finalValue, &decls, visitedValues);

            Loop::LoopBounds::Direction loopDirection = bounds->getDirection();

            // Find loop step
            PHINode* indVarPhi = dyn_cast<PHINode>(indVar);
            Value* stepValue = indVarPhi->getIncomingValueForBlock(&bb);
            Instruction* stepInst = dyn_cast<Instruction>(stepValue);
            ConstantInt* stepValueInt = dyn_cast<ConstantInt>(
                stepInst->getOperand(1)
            );
            std::string stepValueStr = std::to_string(
                std::abs(stepValueInt->getSExtValue())
            );
            // Number of loops, taken by the initial/final value of the loop
            // variable divided by the step value
            std::string loopNumExpr;

            std::string constraint;
            generatedVars.push_back(loopVarName);
            if (loopDirection == Loop::LoopBounds::Direction::Increasing) {
                loopNumExpr =
                    "(to_int (ceiling (/ " + finalValueStr + " " + 
                    stepValueStr + ")))";
                constraint =
                    "<= " + initialValueStr + " " + loopVarName + " " + finalValueStr;
            }
            if (loopDirection == Loop::LoopBounds::Direction::Decreasing) {
                loopNumExpr =
                    "(to_int (ceiling (/ " + initialValueStr + " " + 
                    stepValueStr + ")))";
                constraint =
                    "<= " + finalValueStr + " " + loopVarName + " " + initialValueStr;
            }

            // Add a z3 variable for the number of loops
            // std::string loopNumVarName = generateLoopNumVariable(loopIndex);
            // generatedVars.push_back(loopNumVarName);
            
            // Fixed values for testing
            ///////////// TEMP
            constraint = std::regex_replace(
                constraint,
                std::regex("%\\{_Z9incKernelPiS_i_param_2\\}"),
                "1000000"
            );
            constraint = std::regex_replace(
                constraint,
                std::regex("%\\{llvm.nvvm.read.ptx.sreg.ctaid.x\\}"),
                "100"
            );
            constraint = std::regex_replace(
                constraint,
                std::regex("%\\{llvm.nvvm.read.ptx.sreg.ntid.x\\}"),
                "1000"
            );
            constraint = std::regex_replace(
                constraint,
                std::regex("%\\{llvm.nvvm.read.ptx.sreg.tid.x\\}"),
                "10"
            );
            /////////////

            constraints.insert(constraint);

            // Find number of loops to be executed
            outs() << "Loop variable:\n";
            outs() << "\n";

            // Get array access constraints
            outs() << "\nIndices:\n";
            visitedValues.clear();
            // inst_iterator inst = inst_begin(bb);
            // inst_iterator instEnd = inst_end(bb);
            for (Instruction &inst : bb) {
                GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(&inst);
                if (!gep) continue;

                for (const Use &index : gep->indices()) {
                    // index->print(outs());
                    // index.get()->print(outs());
                    Instruction* indexValueInst = dyn_cast<Instruction>(index.get());
                    std::string indexStr;
                    std::string indexVarName = generateIndexVariable();
                    if (indexValueInst && indexValueInst->isBitwiseLogicOp())
                        indexStr =
                            "(bv2int " +
                            UnfoldValue(index.get(), &decls, visitedValues) +
                            ")";
                    else {
                        // raw_string_ostream stream(indexStr);
                        // index.get()->print(stream);
                        // indexStr = stream.str();
                        // std::set<Value*> visitedValues;
                        indexStr =
                            UnfoldValue(index.get(), &decls, visitedValues);
                    }

                    std::string constraint = 
                        "<= 0 (* " + loopNumExpr + " " + indexStr +") 1000000";

                    ///////////// TEMP
                    constraint = std::regex_replace(
                        constraint,
                        std::regex("%\\{_Z9incKernelPiS_i_param_2\\}"),
                        "1000000"
                    );
                    constraint = std::regex_replace(
                        constraint,
                        std::regex("%\\{llvm\\.nvvm\\.read\\.ptx\\.sreg\\.ctaid\\.x\\}"),
                        "100"
                    );
                    constraint = std::regex_replace(
                        constraint,
                        std::regex("%\\{llvm\\.nvvm\\.read\\.ptx\\.sreg\\.ntid\\.x\\}"),
                        "1000"
                    );
                    constraint = std::regex_replace(
                        constraint,
                        std::regex("%\\{llvm\\.nvvm\\.read\\.ptx\\.sreg\\.tid\\.x\\}"),
                        "10"
                    );
                    /////////////

                    constraints.insert(constraint);
                    // outs() << "\n";
                }

            }
            loopIndex++;
        }

        // Get array access constraints
        // outs() << "\nIndices:\n";
        // std::set<Value*>visitedValues;
        // inst_iterator inst = inst_begin(func);
        // inst_iterator instEnd = inst_end(func);        
        // for (inst; inst != instEnd; ++inst) {
        //     GetElementPtrInst* gep = dyn_cast<GetElementPtrInst>(&*inst);
        //     if (!gep) continue;

        //     for (const Use &index : gep->indices()) {
        //         // index->print(outs());
        //         // index.get()->print(outs());
        //         Instruction* indexValueInst = dyn_cast<Instruction>(index.get());
        //         std::string indexStr;
        //         if (indexValueInst && indexValueInst->isBitwiseLogicOp())
        //             indexStr = "(bv2int " + indexStr + ")";
        //         else {
        //             // raw_string_ostream stream(indexStr);
        //             // index.get()->print(stream);
        //             // indexStr = stream.str();
        //             // std::set<Value*> visitedValues;
        //             indexStr =
        //                 UnfoldValue(index.get(), &decls, visitedValues);
        //         }

        //         std::string constraint = "<= 0 " + indexStr + " 1000000";

        //         ///////////// TEMP
        //         constraint = std::regex_replace(
        //             constraint,
        //             std::regex("%\\{_Z9incKernelPiS_i_param_2\\}"),
        //             "1000000"
        //         );
        //         constraint = std::regex_replace(
        //             constraint,
        //             std::regex("%\\{llvm\\.nvvm\\.read\\.ptx\\.sreg\\.ctaid\\.x\\}"),
        //             "100"
        //         );
        //         constraint = std::regex_replace(
        //             constraint,
        //             std::regex("%\\{llvm\\.nvvm\\.read\\.ptx\\.sreg\\.ntid\\.x\\}"),
        //             "1000"
        //         );
        //         constraint = std::regex_replace(
        //             constraint,
        //             std::regex("%\\{llvm\\.nvvm\\.read\\.ptx\\.sreg\\.tid\\.x\\}"),
        //             "10"
        //         );
        //         /////////////

        //         constraints.insert(constraint);
        //         // outs() << "\n";
        //     }

        // }

        PtxToLlvmIrConverter::Module->print(outs(), nullptr, false, true);

        std::string funcName = func.getName().str();
        outs() << "\n" << funcName << " Constraints:" << "\n\t";
        for (std::string constraint : constraints)
            outs() << constraint << "\n\t";
        outs() << "\n";

        z3::context solverContext;
        z3::solver solver(solverContext);


        // z3::expr x = solverContext.int_const("x");
        // z3::expr y = solverContext.int_const("y");
        // z3::expr i = solverContext.int_const("i");
        // z3::expr_vector expr = solverContext.parse_string("(declare-const x Int)(declare-const y Int)(= (- x y) (+ x (- y) 1))");
        // solver.from_string("(declare-const x Int)(declare-const y Int)(assert (= x y))");
        // solver.add(expr);
        // outs() << "Solver: " << solver << "\n";
        // outs() << "Solver smt2: " << solver.to_smt2() << "\n";
        // switch (solver.check()) {
        //     case z3::unsat: outs() << "Not Satisfied!\n"; break;
        //     case z3::sat: outs() << "Satisfied!\n"; break;
        //     case z3::unknown: outs() << "No result\n"; break;
        // }

        std::string z3Expr = "(define-fun ceiling ((x Real)) Int "
            "(+ (div (+ x 1) 1) (- (div (div (+ x 1) 1) x) 1)))";
        // Keep unique declarations and add them in z3 expression
        // std::unique(decls.begin(), decls.end());
        // for (std::string decl : decls)
        //     z3Expr += "(declare-const " + decl +" Int)";
        for (std::string constraint : constraints) {
            // z3::expr_vector expression = solverContext.parse_string(constraint.c_str());
            // solver.add(expression);
            z3Expr += "(assert (" + constraint + "))";
        }
        z3Expr += "(check-sat)";
        outs() << "expr: " << z3Expr << "\n";
        // solver.from_string(z3Expr.c_str());
        // solverContext.check_parser_error();
        // z3::expr_vector expr = solverContext.parse_string(z3Expr.c_str());
        try {
            // z3::expr i = solverContext.int_const("i");
            // z3::expr_vector expr = solverContext.parse_string("(declare-const i Int)(assert (< i 0))(assert (> i 1))(check-sat)");
            // z3::expr_vector expr = solverContext.parse_string("(declare-const i Int)(assert(< 0 i (bv2int (bvand ((_ int2bv 32) 16) ((_ int2bv 32) 3)))))");
            std::string decls = "";
            for (std::string loopVar : generatedVars)
                decls += "(declare-const " + loopVar + " Int)";

            z3Expr = decls + z3Expr;
            z3::expr_vector expr = solverContext.parse_string(z3Expr.c_str());
            // solverContext.check_parser_error();
            solver.add(expr);

            // outs() << "Solver: " << solver << "\n";
            outs() << "Solver smt2: " << solver.to_smt2() << "\n";
            switch (solver.check()) {
                case z3::unsat: outs() << "Not Satisfied!\n"; break;
                case z3::sat: outs() << "Satisfied\n"; break;
                case z3::unknown: outs() << "No result\n"; break;
            }
        }
        catch(z3::exception& e) {
            outs() << e.msg() << "\n";
        }

        // if (expr.check_error() != Z3_error_code::Z3_OK) {
        //     outs() << "Z3 expression error!" << "\n";
        // }

    }
}