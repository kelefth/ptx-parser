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
        unsigned int id,
        std::string label,
        std::string name,
        std::string type,
        int alignment,
        int size
    );

    ParamDirectStatement(
        unsigned int id,
        std::string name,
        std::string type,
        int alignment,
        int size
    );

    std::string getName();
    std::string getType();
    int getAlignment();
    int getSize();

    bool operator==(const Statement stmt) const;

    std::string ToString();
    void ToLlvmIr();
    void dump() const;
};

#endif