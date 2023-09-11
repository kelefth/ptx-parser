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

std::string ParamDirectStatement::getName() {
    return Name;
}

std::string ParamDirectStatement::getType() {
    return Type;
}

int ParamDirectStatement::getAlignment() {
    return Alignment;
}

int ParamDirectStatement::getSize() {
    return Size;
}

std::string ParamDirectStatement::ToString() {
    return "";
}

llvm::Value* ParamDirectStatement::ToLlvmIr() { return nullptr; }

void ParamDirectStatement::dump() const {
    std::cout << getLabel() << " .param "
              << Name << " " 
              << "Type: " << Type << " "
              << "Align: " << Alignment << " "
              << "Size: " << Size << " ";
}