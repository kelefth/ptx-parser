#ifndef LINKING_DIRECTSTATEMENT_H
#define LINKING_DIRECTSTATEMENT_H

#include <memory>

#include "DirectStatement.h"

class LinkingDirectStatement : public DirectStatement {

    std::string AddressSpace;
    int Alignment;
    std::string Type;
    std::string Identifier;
    int Size;

public:

    LinkingDirectStatement(
        unsigned int id,
        std::string label,
        std::string name,
        std::string addressSpace,
        int alignment,
        std::string type,
        std::string identifier,
        int size
    );

    void dump() const;
};

#endif