#ifndef VARDEC_DIRECTSTATEMENT_H
#define VARDEC_DIRECTSTATEMENT_H

#include <memory>

#include "DirectStatement.h"

class VarDecDirectStatement : public DirectStatement {

    std::string AddressSpace;
    int Alignment;
    std::string Type;
    std::string Identifier;
    int Size;

public:

    VarDecDirectStatement(
        unsigned int id,
        std::string label,
        std::string name,
        std::string addressSpace,
        int alignment,
        std::string type,
        std::string identifier,
        int size
    );

    std::string getAddressSpace();
    int getAlignment();
    std::string getType();
    std::string getIdentifier();
    int getSize();

    // void ToLlvmIr();
    void dump() const;
};

#endif