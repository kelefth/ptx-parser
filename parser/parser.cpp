#include <iostream>
#include <variant>
#include <algorithm>

#include "parser.h"
#include "../InstrStatement.h"
#include "../DirectStatement.h"
#include "../ModuleDirectStatement.h"
#include "../ParamDirectStatement.h"
#include "../KernelDirectStatement.h"
#include "../Operand.h"

int currentToken;
// std::vector<std::unique_ptr<Statement>> statements;
// std::vector<InstrStatement> statements;
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
                    modifiers.push_back(currStrVal);
                    break;
                case token_type:
                    types.push_back(currStrVal);
                    break;
                case token_reg:
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
    kernelStmtPtr->AddBodyStatement(std::make_shared<InstrStatement>(label, pred, inst, modifiers, types, std::move(destOps), std::move(sourceOps)));

}

// void ParseDirectStatement() {

//     std::string label = "";
//     std::string type = "";
//     std::vector<std::string> directives;
//     std::vector<std::string> arguments;
//     std::vector<std::unique_ptr<Statement>> incStatements;

//     bool isFunctionEntryDirective = false;
//     bool isParamDirective = false;

//     while (currentToken != token_semicolon) {
//         bool isParamDirectiveEnd = false;

//         // Check cases that directive statements end
//         if(
//             (isFunctionEntryDirective && currentToken == token_leftparenth) ||
//             (currentToken == token_newline)
//         ) {
//             break;
//         }

//         switch (currentToken) {
//             case token_label:
//                     label = currStrVal;
//                     break;
//             case token_direct:
//                 if (currStrVal == "entry" || currStrVal == "func")
//                     isFunctionEntryDirective = true;
//                 else if (currStrVal == "param")
//                     isParamDirective = true;

//                 directives.push_back(currStrVal);

//                 break;
//             case token_type:
//                 type = currStrVal;
//                 break;
//             case token_reg:
//                 arguments.push_back(currStrVal);
//             case token_number:
//                 arguments.push_back(std::to_string(currNumVal));
//             case token_id:
//                 arguments.push_back(currStrVal);

//                 if (isParamDirective) isParamDirectiveEnd = true;

//                 break;
//             default:
//                 break;
//         }

//         if (isParamDirectiveEnd) break;

//         currentToken = getToken();
//     }

//     statements.push_back(std::make_unique<DirectStatement>(label, directives, type, arguments, std::move(incStatements)));
// }

void ParseModuleDirectStatement() {

    std::string directive = currStrVal;
    getToken();
    std::string value = "";
    if (currStrVal != "")
        value = currStrVal;
    else value = std::to_string(currNumVal); 

    statements.push_back(std::make_shared<ModuleDirectStatement>(directive, value));
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
                type = currStrVal;
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
    KernelDirectStatement* stmtPtr = static_cast<KernelDirectStatement*>(statements.back().get());
    // auto stmtPtr = std::make_shared<KernelDirectStatement>(*(static_cast<KernelDirectStatement*>(statements.back().get())));
    // Add parameter to last kernel's parameters
    stmtPtr->AddParameter(std::make_shared<ParamDirectStatement>(label, name, type, alignment, size));
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
                statements.push_back(std::make_shared<KernelDirectStatement>(label, currStrVal));
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

        currentToken = getToken();
    }

    dump_statements();
}