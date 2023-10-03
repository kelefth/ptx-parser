#include <iostream>

#include "LinkingDirectStatement.h"

LinkingDirectStatement::LinkingDirectStatement(
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

void LinkingDirectStatement::dump() const {
    if (getLabel() != "")
        std::cout << "Label: " << getDirective() << " ";
    std::cout << "Dir.: " << getDirective()
              << " A.S.: " << AddressSpace
              << " Align: " << Alignment
              << " Type: " << Type
              << " Id: " << Identifier
              << " Size: " << Size << std::endl;
}