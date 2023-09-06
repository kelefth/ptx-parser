#ifndef PARAM_DIRECTSTATEMENT_H
#define PARAM_DIRECTSTATEMENT_H

#include "../PtxToLlvmIr/PtxToLlvmIrConverter.h"
#include "DirectStatement.h"

class ParamDirectStatement : public DirectStatement {

    std::string Name;
    std::string Type;
    int Alignment;
    int Size;

public:

    ParamDirectStatement(
        std::string label,
        std::string name,
        std::string type,
        int alignment,
        int size
    );

    ParamDirectStatement(
        std::string name,
        std::string type,
        int alignment,
        int size
    );

    std::string getName();
    std::string getType();
    int getAlignment();
    int getSize();

    std::string ToString();
    llvm::Value* ToLlvmIr();
    void dump() const;
};

#endif