#include <iostream>

#include "ParamDirectStatement.h"

ParamDirectStatement::ParamDirectStatement(
    std::string label,
    std::string name,
    std::string type,
    int alignment,
    int size
) : DirectStatement(label, ".param"),
    Name(name),
    Type(type),
    Alignment(alignment),
    Size(size) {}

ParamDirectStatement::ParamDirectStatement(
    std::string name,
    std::string type,
    int alignment,
    int size
) : DirectStatement("", ".param"),
    Name(name), Type(type),
    Alignment(alignment),
    Size(size) {}


std::string ParamDirectStatement::ToString() {
    return "";
}

void ParamDirectStatement::dump() const {
    std::cout << getLabel() << " .param "
              << Name << " " 
              << "Type: " << Type << " "
              << "Align: " << Alignment << " "
              << "Size: " << Size << " ";
}