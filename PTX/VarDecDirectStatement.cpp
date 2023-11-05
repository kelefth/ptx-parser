#include <iostream>

#include "VarDecDirectStatement.h"

VarDecDirectStatement::VarDecDirectStatement(
    unsigned int id,
    std::string label,
    std::string directive,
    std::string addressSpace,
    int alignment,
    std::string type,
    std::string identifier,
    int size
) : DirectStatement(id, label, directive),
    AddressSpace(addressSpace),
    Alignment(alignment),
    Type(type),
    Identifier(identifier),
    Size(size) {}

std::string VarDecDirectStatement::getAddressSpace() {
    return AddressSpace;
}

int VarDecDirectStatement::getAlignment() {
    return Alignment;
}

std::string VarDecDirectStatement::getType() {
    return Type;
}

std::string VarDecDirectStatement::getIdentifier() {
    return Identifier;
}

int VarDecDirectStatement::getSize() {
    return Size;
}

// void LinkingDirectStatement::ToLlvmIr() {

// }

void VarDecDirectStatement::dump() const {
    if (getLabel() != "")
        std::cout << "Label: " << getDirective() << " ";
    std::cout << "Dir.: " << getDirective()
              << " A.S.: " << AddressSpace
              << " Align: " << Alignment
              << " Type: " << Type
              << " Id: " << Identifier
              << " Size: " << Size << std::endl;
}